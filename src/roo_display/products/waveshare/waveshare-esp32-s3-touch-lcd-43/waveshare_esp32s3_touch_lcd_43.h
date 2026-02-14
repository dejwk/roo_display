// 2026-02-14 19:00:00 v1.5.0 - Remove lazy init; let driver handle full reset.
#pragma once

#include "roo_display/hal/config.h"

#if !defined(ESP32) || !(CONFIG_IDF_TARGET_ESP32S3)
#warning Compilation target must be ESP32_S3 for this device.
#else

#include "roo_display/driver/esp32s3_dma_parallel_rgb565.h"
#include "roo_display/driver/touch_gt911.h"
#include "roo_display/hal/i2c.h"
#include "roo_display/products/combo_device.h"

namespace roo_display::products::waveshare
{

// Waveshare ESP32-S3 Touch LCD 4.3" device wrapper.
//
// GT911 reset is routed through a CH422G I/O expander (GpioSetter lambda).
// The INT pin has a hardware pull-down on the board, keeping it LOW during
// reset so the GT911 reliably selects I2C address 0x5D without software
// intervention.
class WaveshareEsp32s3TouchLcd43 : public ComboDevice {
 public:
  // Constructs the device without bus initialization.
  // Call initTransport() before use.
  WaveshareEsp32s3TouchLcd43(Orientation orientation = Orientation(),
                             I2cMasterBusHandle i2c = I2cMasterBusHandle());

  // Initializes PSRAM, I2C bus, and CH422G expander.
  bool initTransport();

  DisplayDevice& display() override;
  TouchDevice* touch() override;
  TouchCalibration touch_calibration() override;

  void setBacklight(bool on);

 private:
  void writeEXIO(uint8_t pin, bool state);

  I2cMasterBusHandle i2c_;
  I2cSlaveDevice ch422g_wr_set_;
  I2cSlaveDevice ch422g_wr_io_;
  esp32s3_dma::ParallelRgb565Buffered display_;
  TouchGt911 touch_;
  uint8_t exio_shadow_;
};

}  // namespace roo_display::products::waveshare


#endif  // ESP32 && CONFIG_IDF_TARGET_ESP32S3
