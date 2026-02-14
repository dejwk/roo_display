// 2026-02-14 19:00:00 v1.5.0 - Remove lazy init; let driver handle full reset.
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

// CH422G I/O expander I2C addresses.
constexpr uint8_t kCh422gAddrWrSet = 0x24;
constexpr uint8_t kCh422gAddrWrIo = 0x38;
constexpr uint8_t kCh422gConfigWriteEnable = 0b00000001;

// CH422G pin assignments.
constexpr uint8_t kExioTpRst = 1;   // GT911 reset.
constexpr uint8_t kExioLcdBl = 2;   // LCD backlight.
constexpr uint8_t kExioLcdRst = 3;  // LCD reset.

// I2C configuration.
constexpr int kI2cSda = 8;
constexpr int kI2cScl = 9;
constexpr uint32_t kI2cFreq = 400000;

// GT911 async reset delay. The GT911 needs ~300ms after RST release to boot.
constexpr int kGt911AsyncInitDelayMs = 300;

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
      ch422g_wr_set_(i2c_, kCh422gAddrWrSet),
      ch422g_wr_io_(i2c_, kCh422gAddrWrIo),
      display_(kWaveshareConfig),
      // INT: inactive — board has hardware pull-down (3.6K) ensuring LOW during reset
      // for I2C address 0x5D selection.
      // RST: routed through CH422G expander via GpioSetter lambda.
      touch_(i2c_,
             GpioSetter(),
             GpioSetter(
                 /*setter=*/[this](uint8_t state) {
                   writeEXIO(kExioTpRst, state > 0);
                 },
                 /*init=*/[this]() { writeEXIO(kExioTpRst, false); }),
             kGt911AsyncInitDelayMs),
      exio_shadow_(0b00001110) {  // LCD_RST=1, LCD_BL=1, TP_RST=1.
  display_.setOrientation(orientation);
}

bool WaveshareEsp32s3TouchLcd43::initTransport() {
  if (!psramFound()) {
    LOG(ERROR) << "PSRAM not found - required for frame buffer";
    return false;
  }

  i2c_.init(kI2cSda, kI2cScl, kI2cFreq);
  ch422g_wr_set_.init();
  ch422g_wr_io_.init();
  delay(10);

  // Set initial CH422G state: LCD reset, backlight, and TP reset high.
  exio_shadow_ = (1 << kExioLcdRst) | (1 << kExioLcdBl) | (1 << kExioTpRst);
  writeEXIO(kExioTpRst, true);

  return true;
}

DisplayDevice& WaveshareEsp32s3TouchLcd43::display() {
  return display_;
}

TouchDevice* WaveshareEsp32s3TouchLcd43::touch() {
  return &touch_;
}

TouchCalibration WaveshareEsp32s3TouchLcd43::touch_calibration() {
  return TouchCalibration(0, 0, 800, 480);
}

void WaveshareEsp32s3TouchLcd43::setBacklight(bool on) {
  writeEXIO(kExioLcdBl, on);
}

void WaveshareEsp32s3TouchLcd43::writeEXIO(uint8_t pin, bool state) {
  if (state) {
    exio_shadow_ |= (1 << pin);
  } else {
    exio_shadow_ &= ~(1 << pin);
  }

  // CH422G requires write-enable at 0x24 before data write at 0x38.
  roo::byte enable_cmd[] = {roo::byte{kCh422gConfigWriteEnable}};
  if (!ch422g_wr_set_.transmit(enable_cmd, 1)) {
    LOG(ERROR) << "CH422G write enable failed";
    return;
  }

  roo::byte output_data[] = {roo::byte{exio_shadow_}};
  if (!ch422g_wr_io_.transmit(output_data, 1)) {
    LOG(ERROR) << "CH422G output write failed";
  }
}

}  // namespace waveshare
}  // namespace products
}  // namespace roo_display

#endif  // ESP32 && CONFIG_IDF_TARGET_ESP32S3
