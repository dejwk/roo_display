#pragma once

// https://www.makerfabs.com/esp32-s3-parallel-tft-with-touch-7-inch.html
// Maker: MakerFabs
// Product Code: MTESPS37

#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/esp32s3_dma_parallel_rgb565.h"
#include "roo_display/driver/touch_gt911.h"
#include "roo_display/hal/config.h"
#include "roo_display/hal/i2c.h"
#include "roo_display/products/combo_device.h"

#if !defined(ESP_PLATFORM) || !(CONFIG_IDF_TARGET_ESP32S3)
#warning Compilation target must be ESP32_S3 for this device.
#else

namespace roo_display::products::makerfabs {

/// Makerfabs ESP32-S3 Parallel IPS capacitive display family.
///
/// Note: these combos have a built-in SD card reader. Use SD_MMC interface
/// with pins: clk=12, cmd=11, d0=13.

class Esp32s3ParallelIpsCapacitive : public ComboDevice {
 public:
  /// Supported panel resolutions.
  enum Resolution { k800x480, k1024x600 };
  /// Create a device for the given resolution and orientation.
  Esp32s3ParallelIpsCapacitive(Resolution resolution,
                               Orientation orientation = Orientation(),
                               I2cMasterBusHandle i2c = I2cMasterBusHandle());

  /// Initialize I2C transport for touch.
  void initTransport() { i2c_.init(17, 18); }

  DisplayDevice& display() override { return display_; }

  TouchDevice* touch() override { return &touch_; }

  /// Return touch calibration for the panel.
  TouchCalibration touch_calibration() override;

 private:
  Resolution resolution_;
  I2cMasterBusHandle i2c_;
  roo_display::esp32s3_dma::ParallelRgb565<esp32s3_dma::FLUSH_MODE_LAZY>
      display_;
  roo_display::TouchGt911 touch_;
};

/// Makerfabs 800x480 variant.
class Esp32s3ParallelIpsCapacitive800x480
    : public Esp32s3ParallelIpsCapacitive {
 public:
  Esp32s3ParallelIpsCapacitive800x480(
      Orientation orientation = Orientation(),
      I2cMasterBusHandle i2c = I2cMasterBusHandle())
      : Esp32s3ParallelIpsCapacitive(k800x480, orientation, i2c) {}
};

/// Makerfabs 1024x600 variant.
class Esp32s3ParallelIpsCapacitive1024x600
    : public Esp32s3ParallelIpsCapacitive {
 public:
  Esp32s3ParallelIpsCapacitive1024x600(
      Orientation orientation = Orientation(),
      I2cMasterBusHandle i2c = I2cMasterBusHandle())
      : Esp32s3ParallelIpsCapacitive(k1024x600, orientation, i2c) {}
};

}  // namespace roo_display::products::makerfabs

#endif  // ESP32 && CONFIG_IDF_TARGET_ESP32S3