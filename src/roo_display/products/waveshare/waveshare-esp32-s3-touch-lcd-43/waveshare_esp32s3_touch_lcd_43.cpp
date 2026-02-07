// 2025-02-06 22:00:00 v1.3.0 - I2C HAL abstraction (original naming)
// Waveshare ESP32-S3-Touch-LCD-4.3 display device implementation.

#include "roo_display/hal/config.h"

#if !defined(ESP32) || !(CONFIG_IDF_TARGET_ESP32S3)
#else

#include "roo_logging.h"
#include "waveshare_esp32s3_touch_lcd_43.h"

namespace roo_display {
namespace products {
namespace waveshare {

namespace {

// CH422G I/O Expander I2C addresses.
constexpr uint8_t CH422G_ADDR_WR_SET = 0x24;
constexpr uint8_t CH422G_ADDR_WR_IO = 0x38;
constexpr uint8_t CH422G_CONFIG_WRITE_ENABLE = 0b00000001;

// CH422G pin assignments.
constexpr uint8_t EXIO_TP_RST = 1;   // GT911 Reset.
constexpr uint8_t EXIO_LCD_BL = 2;   // LCD Backlight.
constexpr uint8_t EXIO_LCD_RST = 3;  // LCD Reset.

// I2C configuration.
constexpr int I2C_SDA = 8;
constexpr int I2C_SCL = 9;
constexpr uint32_t I2C_FREQ = 400000;

// GT911 configuration.
constexpr int8_t GT911_INT_PIN = 4;
constexpr uint8_t GT911_I2C_ADDR = 0x5D;
constexpr int GT911_ASYNC_INIT_DELAY_MS = 300;

// Display configuration.
constexpr esp32s3_dma::Config kWaveshareConfig = {
    .width = 800,
    .height = 480,
    .de = 5,
    .hsync = 46,
    .vsync = 3,
    .pclk = 7,
    .hsync_pulse_width = 4,
    .hsync_back_porch = 8,
    .hsync_front_porch = 8,
    .hsync_polarity = 0,
    .vsync_pulse_width = 4,
    .vsync_back_porch = 16,
    .vsync_front_porch = 16,
    .vsync_polarity = 0,
    .pclk_active_neg = 1,
    .prefer_speed = 16000000,
    .r0 = 1,
    .r1 = 2,
    .r2 = 42,
    .r3 = 41,
    .r4 = 40,
    .g0 = 39,
    .g1 = 0,
    .g2 = 45,
    .g3 = 48,
    .g4 = 47,
    .g5 = 21,
    .b0 = 14,
    .b1 = 38,
    .b2 = 18,
    .b3 = 17,
    .b4 = 10,
    .bswap = false};

}  // namespace

WaveshareEsp32s3TouchLcd43::WaveshareEsp32s3TouchLcd43(
    Orientation orientation, I2cMasterBusHandle i2c)
    : i2c_(i2c),
      ch422g_wr_set_(i2c_, CH422G_ADDR_WR_SET),
      ch422g_wr_io_(i2c_, CH422G_ADDR_WR_IO),
      display_(kWaveshareConfig),
      touch_(i2c_, -1, GT911_INT_PIN, GT911_ASYNC_INIT_DELAY_MS),
      exio_shadow_(0b00001110),  // LCD_RST=1, LCD_BL=1, TP_RST=1.
      touch_initialized_(false) {
  display_.setOrientation(orientation);
}

bool WaveshareEsp32s3TouchLcd43::initTransport() {
  if (!psramFound()) {
    LOG(ERROR) << "PSRAM not found - required for frame buffer";
    return false;
  }

  // Initialize I2C bus using HAL.
  i2c_.init(I2C_SDA, I2C_SCL, I2C_FREQ);

  // Initialize both CH422G I2C slave device addresses.
  ch422g_wr_set_.init();
  ch422g_wr_io_.init();

  delay(10);  // Allow bus to stabilize.

  // Set initial CH422G output state: LCD reset and backlight high.
  exio_shadow_ = (1 << EXIO_LCD_RST) | (1 << EXIO_LCD_BL) | (1 << EXIO_TP_RST);
  writeEXIO(EXIO_TP_RST, true);

  return true;
}

DisplayDevice& WaveshareEsp32s3TouchLcd43::display() {
  return display_;
}

TouchDevice* WaveshareEsp32s3TouchLcd43::touch() {
  // Lazy initialization: reset GT911 on first access.
  if (!touch_initialized_) {
    initTouchHardware();
    touch_initialized_ = true;
  }
  return &touch_;
}

TouchCalibration WaveshareEsp32s3TouchLcd43::touch_calibration() {
  return TouchCalibration(0, 0, 800, 480);
}

void WaveshareEsp32s3TouchLcd43::setBacklight(bool on) {
  writeEXIO(EXIO_LCD_BL, on);
}

void WaveshareEsp32s3TouchLcd43::initTouchHardware() {
  // The GT911 samples the INT pin state during reset to determine its I2C
  // address. INT must be LOW during reset to select address 0x5D.

  // Step 1: Pull INT pin LOW to select I2C address 0x5D.
  pinMode(GT911_INT_PIN, OUTPUT);
  digitalWrite(GT911_INT_PIN, LOW);
  delay(10);

  // Step 2: Assert GT911 reset via CH422G (active LOW).
  // CRITICAL: Reset shadow register to ensure only LCD pins are high.
  exio_shadow_ = (1 << EXIO_LCD_RST) | (1 << EXIO_LCD_BL);
  writeEXIO(EXIO_TP_RST, false);
  delay(100);

  // Step 3: Release reset. GT911 boots asynchronously (~300ms).
  writeEXIO(EXIO_TP_RST, true);

  // Step 4: Configure INT pin as input for interrupt handling.
  pinMode(GT911_INT_PIN, INPUT);

  // Note: The TouchGt911 driver includes a 300ms async initialization delay,
  // ensuring the GT911 has completed boot before communication begins.
}

void WaveshareEsp32s3TouchLcd43::writeEXIO(uint8_t pin, bool state) {
  // Update shadow register to track output state.
  if (state) {
    exio_shadow_ |= (1 << pin);
  } else {
    exio_shadow_ &= ~(1 << pin);
  }

  // CH422G protocol: Send write enable to 0x24, then data to 0x38.
  // Step 1: Enable write mode at address 0x24.
  roo::byte enable_cmd[] = {roo::byte{CH422G_CONFIG_WRITE_ENABLE}};
  if (!ch422g_wr_set_.transmit(enable_cmd, 1)) {
    LOG(ERROR) << "CH422G write enable failed";
    return;
  }

  // Step 2: Write output data to address 0x38.
  roo::byte output_data[] = {roo::byte{exio_shadow_}};
  if (!ch422g_wr_io_.transmit(output_data, 1)) {
    LOG(ERROR) << "CH422G output write failed";
  }
}

}  // namespace waveshare
}  // namespace products
}  // namespace roo_display

#endif  // ESP32 && CONFIG_IDF_TARGET_ESP32S3
