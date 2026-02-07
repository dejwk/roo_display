// 2025-02-06 22:00:00 v1.3.0 - I2C HAL abstraction (original naming)
#pragma once
#include "roo_display/hal/config.h"

#if !defined(ESP32) || !(CONFIG_IDF_TARGET_ESP32S3)
#warning Compilation target must be ESP32_S3 for this device.
#else

// Waveshare ESP32-S3-Touch-LCD-4.3 display device support.
// Hardware configuration for roo_display library.

#include "roo_display/driver/esp32s3_dma_parallel_rgb565.h"
#include "roo_display/driver/touch_gt911.h"
#include "roo_display/hal/i2c.h"
#include "roo_display/products/combo_device.h"

namespace roo_display::products::waveshare
{

// Driver for the Waveshare ESP32-S3-Touch-LCD-4.3 combo device.
// This device features an 800x480 RGB565 parallel display with GT911
// capacitive touch controller. The GT911 reset pin is controlled via
// a CH422G I/O expander rather than direct GPIO.
class WaveshareEsp32s3TouchLcd43 : public ComboDevice {
 public:
  // Constructs the device driver with optional orientation and I2C bus handle.
  // Does not perform hardware initialization; call initTransport() explicitly.
  WaveshareEsp32s3TouchLcd43(Orientation orientation = Orientation(),
                             I2cMasterBusHandle i2c = I2cMasterBusHandle());

  // Initializes the I2C bus and verifies PSRAM availability.
  // Touch controller reset is deferred until first touch() access.
  // Returns true on success, false if PSRAM is not found.
  bool initTransport();

  // ComboDevice interface implementation.
  DisplayDevice& display() override;
  TouchDevice* touch() override;  // Lazy initialization on first call.
  TouchCalibration touch_calibration() override;

  // Controls the LCD backlight via CH422G I/O expander.
  void setBacklight(bool on);

 private:
  I2cMasterBusHandle i2c_;
  I2cSlaveDevice ch422g_wr_set_;  // CH422G write set register (0x24).
  I2cSlaveDevice ch422g_wr_io_;   // CH422G write I/O register (0x38).
  esp32s3_dma::ParallelRgb565Buffered display_;
  TouchGt911 touch_;
  uint8_t exio_shadow_;       // Shadow register for CH422G output state.
  bool touch_initialized_;    // Tracks whether touch hardware has been reset.

  // Performs GT911 hardware reset via CH422G I/O expander.
  // Implements the complete INT pin sequence required for correct
  // I2C address selection (0x5D).
  void initTouchHardware();

  // Writes to a CH422G I/O expander pin.
  void writeEXIO(uint8_t pin, bool state);
};

} // namespace roo_display::products::waveshare

#endif  // ESP32 && CONFIG_IDF_TARGET_ESP32S3

