// Waveshare ESP32-S3-Touch-LCD-4.3
// Implementation file

#include "waveshare_esp32s3_touch_lcd_43.h"
#include "roo_logging.h"

namespace roo_display::products::waveshare {

namespace {

// CH422G I/O Expander register definitions
constexpr uint8_t CH422G_ADDR_WR_SET = 0x24;
constexpr uint8_t CH422G_ADDR_WR_IO = 0x38;
constexpr uint8_t CH422G_CONFIG_WRITE_ENABLE = 0b00000001;

// CH422G pin assignments
constexpr uint8_t EXIO_TP_RST = 1;   // GT911 Reset
constexpr uint8_t EXIO_LCD_BL = 2;   // LCD Backlight
constexpr uint8_t EXIO_LCD_RST = 3;  // LCD Reset

// I2C configuration
constexpr gpio_num_t I2C_SDA = GPIO_NUM_8;
constexpr gpio_num_t I2C_SCL = GPIO_NUM_9;
constexpr uint32_t I2C_FREQ = 400000;

// GT911 configuration
constexpr gpio_num_t GT911_INT_PIN = GPIO_NUM_4;
constexpr uint8_t GT911_I2C_ADDR = 0x5D;
constexpr int GT911_ASYNC_INIT_DELAY_MS = 300;  // Async init delay

// Display configuration
constexpr esp32s3_dma::Config kWaveshareConfig = {
    .width = 800,
    .height = 480,
    .de = 5, .hsync = 46, .vsync = 3, .pclk = 7,
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
    .r0 = 1, .r1 = 2, .r2 = 42, .r3 = 41, .r4 = 40,
    .g0 = 39, .g1 = 0, .g2 = 45, .g3 = 48, .g4 = 47, .g5 = 21,
    .b0 = 14, .b1 = 38, .b2 = 18, .b3 = 17, .b4 = 10,
    .bswap = false
};

// Write to CH422G I/O expander pin
void writeEXIO(TwoWire& wire, uint8_t& shadow, uint8_t pin,
               bool state) {
  wire.beginTransmission(CH422G_ADDR_WR_SET);
  wire.write(CH422G_CONFIG_WRITE_ENABLE);
  wire.endTransmission();

  if (state) {
    shadow |= (1 << pin);
  } else {
    shadow &= ~(1 << pin);
  }

  wire.beginTransmission(CH422G_ADDR_WR_IO);
  wire.write(shadow);
  wire.endTransmission();
}

// GT911 reset sequence via CH422G (non-blocking)
// Returns immediately - GT911 boots in background
void resetGT911(TwoWire& wire, uint8_t& shadow) {
  // Step 1: INT pin LOW (selects GT911 I2C address 0x5D)
  pinMode(GT911_INT_PIN, OUTPUT);
  digitalWrite(GT911_INT_PIN, LOW);
  delay(10);

  // Step 2: Assert GT911 reset via CH422G
  shadow = (1 << EXIO_LCD_RST) | (1 << EXIO_LCD_BL);
  writeEXIO(wire, shadow, EXIO_TP_RST, LOW);
  delay(100);

  // Step 3: Release reset - GT911 boots in background
  writeEXIO(wire, shadow, EXIO_TP_RST, HIGH);

  // Step 4: INT ready for interrupts
  pinMode(GT911_INT_PIN, INPUT);

  // Note: No verification here - GT911 needs 300ms to boot
  // TouchGt911 will auto-initialize after async delay
}

}  // anonymous namespace

// Constructor with async GT911 initialization
WaveshareEsp32s3TouchLcd43::WaveshareEsp32s3TouchLcd43(
    Orientation orientation, decltype(Wire)& wire)
    : wire_(wire),
      display_(kWaveshareConfig),
      touch_(wire_, -1, -1, GT911_ASYNC_INIT_DELAY_MS),  // ← 300ms
      exio_shadow_(0b00001110) {
  display_.setOrientation(orientation);
}

// Initialize I2C and perform GT911 reset (non-blocking)
bool WaveshareEsp32s3TouchLcd43::initTransport() {
  // Check if Wire already initialized
  wire_.beginTransmission(CH422G_ADDR_WR_SET);
  if (wire_.endTransmission() == 0) {
    // Wire already initialized, skip
    return true;
  }

  // Initialize I2C bus
  wire_.begin(I2C_SDA, I2C_SCL, I2C_FREQ);
  delay(10);  // Allow bus to stabilize

  // Perform GT911 reset via CH422G
  // Returns immediately - GT911 boots in background
  // TouchGt911 will auto-initialize after 300ms delay
  resetGT911(wire_, exio_shadow_);

  return true;
}

// Initialize hardware (GT911 auto-inits asynchronously)
bool WaveshareEsp32s3TouchLcd43::init() {
  // Verify PSRAM is available
  if (!psramFound()) {
    LOG(ERROR) << "PSRAM not found - required for frame buffer";
    return false;
  }

  // Note: GT911 reset already done in initTransport()
  // TouchGt911 will complete initialization automatically
  // after 300ms delay (async via FreeRTOS task)

  return true;
}

// ComboDevice interface implementations
DisplayDevice& WaveshareEsp32s3TouchLcd43::display() {
  return display_;
}

TouchDevice* WaveshareEsp32s3TouchLcd43::touch() {
  return &touch_;
}

TouchCalibration WaveshareEsp32s3TouchLcd43::touch_calibration() {
  return TouchCalibration(0, 0, 800, 480);
}

// Backlight control
void WaveshareEsp32s3TouchLcd43::setBacklight(bool on) {
  writeEXIO(wire_, exio_shadow_, EXIO_LCD_BL, on);
}

}  // namespace roo_display::products::waveshare