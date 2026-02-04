#pragma once

#include "roo_display/hal/config.h"

#if !defined(ESP32) || !(CONFIG_IDF_TARGET_ESP32S3)
#warning Compilation target must be ESP32_S3 for this device.
#else

// Waveshare ESP32-S3-Touch-LCD-4.3
// Hardware configuration for roo_display library

#include <Wire.h>

#include "roo_display/driver/esp32s3_dma_parallel_rgb565.h"
#include "roo_display/driver/touch_gt911.h"
#include "roo_display/products/combo_device.h"

namespace roo_display::products::waveshare {

class WaveshareEsp32s3TouchLcd43 : public ComboDevice {
 public:
  // Constructor: No bus initialization, just object setup.
  // Call initTransport() to initialize hardware.
  WaveshareEsp32s3TouchLcd43(Orientation orientation = Orientation(),
                             decltype(Wire) &wire = Wire);

  // Verifies PSRAM availability and initializes I2C bus.
  // Performs GT911 reset via CH422G; GT911 completes initialization
  // ~300ms after display.init() is called.
  bool initTransport();

  // ComboDevice interface.
  DisplayDevice &display() override;
  TouchDevice *touch() override;
  TouchCalibration touch_calibration() override;

  // Backlight control.
  void setBacklight(bool on);

 private:
  decltype(Wire) &wire_;
  esp32s3_dma::ParallelRgb565Buffered display_;
  TouchGt911 touch_;
  uint8_t exio_shadow_;
};

}  // namespace roo_display::products::waveshare

#endif  // ESP32 && CONFIG_IDF_TARGET_ESP32S3
