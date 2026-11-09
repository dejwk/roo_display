#pragma once

// Makerfabs ESP32-S3 parallel IPS boards:
// E32S3RGB40 (480x480):
// https://github.com/Makerfabs/ESP32-S3-Parallel-TFT-with-Touch-4inch
//
// MTESPS37 (800x480):
// https://www.makerfabs.com/esp32-s3-parallel-tft-with-touch-7-inch.html

#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/esp32s3_dma_parallel_rgb565.h"
#include "roo_display/driver/touch_gt911.h"
#include "roo_display/hal/config.h"
#include "roo_display/hal/i2c.h"
#include "roo_display/products/combo_device.h"
#include "roo_io/fs/esp32/base_vfs_filesystem.h"

#if !defined(ESP_PLATFORM) || !(CONFIG_IDF_TARGET_ESP32S3)
#warning Compilation target must be ESP32_S3 for this device.
#else

namespace roo_display::products::makerfabs {

/// Shared display, touch, and SDMMC implementation for Makerfabs ESP32-S3 IPS
/// boards.
///
/// This implementation base is not directly constructible; named board classes
/// provide their own panel calibration.
class Esp32s3ParallelIpsCapacitive : public ComboDevice {
 public:
  using Display =
      roo_display::esp32s3_dma::ParallelRgb565<esp32s3_dma::FLUSH_MODE_LAZY>;

  using ColorMode = Display::ColorMode;
  static constexpr ColorPixelOrder pixel_order = Display::pixel_order;
  static constexpr ByteOrder byte_order = Display::byte_order;

  /// Return the RGB display device.
  Display& display() override { return display_; }

  /// Return the GT911 touch device.
  TouchDevice* touch() override { return &touch_; }

  /// Initialize touch I2C and SDMMC pins.
  void initTransport();

  /// Return the board's SDMMC filesystem.
  roo_io::BaseEsp32VfsFilesystem& sd() const;

  /// Return board-specific touch calibration.
  TouchCalibration touch_calibration() override = 0;

  /// SD card D0 pin.
  constexpr int8_t pin_sd_d0() const { return 13; }

  /// SD card CLK pin.
  constexpr int8_t pin_sd_clk() const { return 12; }

  /// SD card CMD pin.
  constexpr int8_t pin_sd_cmd() const { return 11; }

  /// I2C SDA pin.
  constexpr int8_t pin_sda() const { return 17; }

  /// I2C SCL pin.
  constexpr int8_t pin_scl() const { return 18; }

  /// GT911 touch interrupt pin.
  constexpr int8_t pin_touch_irq() const { return 38; }

 protected:
  /// Create shared display and touch hardware for the supplied panel config.
  Esp32s3ParallelIpsCapacitive(const esp32s3_dma::Config& config,
                               Orientation orientation, I2cMasterBusHandle i2c);

 private:
  I2cMasterBusHandle i2c_;
  Display display_;
  roo_display::TouchGt911 touch_;
};

/// Makerfabs 480x480 ST7701S board with an SPI SD slot.
class Esp32s3ParallelIpsCapacitive480x480 : public ComboDevice {
 public:
  using Display =
      roo_display::esp32s3_dma::ParallelRgb565<esp32s3_dma::FLUSH_MODE_LAZY>;

  using ColorMode = Display::ColorMode;
  static constexpr ColorPixelOrder pixel_order = Display::pixel_order;
  static constexpr ByteOrder byte_order = Display::byte_order;

  /// Create the ST7701S board with @p orientation and @p i2c.
  Esp32s3ParallelIpsCapacitive480x480(
      Orientation orientation = Orientation(),
      I2cMasterBusHandle i2c = I2cMasterBusHandle());

  /// Initialize touch I2C, the ST7701S panel, and SPI SD pins.
  /// Under ESP-IDF, the caller must initialize the SPI2 bus.
  void initTransport();

  /// Return the RGB display device.
  Display& display() override { return display_; }

  /// Return the GT911 touch device.
  TouchDevice* touch() override { return &touch_; }

  /// Return the board's SPI SD filesystem.
  roo_io::BaseEsp32VfsFilesystem& sd() const;

  /// Return calibration for the 480x480 touch panel.
  TouchCalibration touch_calibration() override;

  /// ST7701S 3-wire SPI chip-select pin.
  constexpr int8_t pin_st7701_cs() const { return 1; }

  /// SD card SPI chip-select pin.
  constexpr int8_t pin_sd_cs() const { return 10; }

  /// Shared panel and SD SPI clock pin.
  constexpr int8_t pin_sd_sck() const { return 12; }

  /// Shared panel and SD SPI MOSI pin.
  constexpr int8_t pin_sd_mosi() const { return 11; }

  /// SD card SPI MISO pin.
  constexpr int8_t pin_sd_miso() const { return 13; }

  /// I2C SDA pin.
  constexpr int8_t pin_sda() const { return 17; }

  /// I2C SCL pin.
  constexpr int8_t pin_scl() const { return 18; }

  /// GT911 touch interrupt pin.
  constexpr int8_t pin_touch_irq() const { return 38; }

 private:
  I2cMasterBusHandle i2c_;
  Display display_;
  roo_display::TouchGt911 touch_;
};

/// Makerfabs 800x480 board with an SDMMC slot.
class Esp32s3ParallelIpsCapacitive800x480
    : public Esp32s3ParallelIpsCapacitive {
 public:
  /// Create the 800x480 board with @p orientation and @p i2c.
  Esp32s3ParallelIpsCapacitive800x480(
      Orientation orientation = Orientation(),
      I2cMasterBusHandle i2c = I2cMasterBusHandle());

  /// Return calibration for the 800x480 touch panel.
  TouchCalibration touch_calibration() override;
};

/// Makerfabs 1024x600 board with an SDMMC slot.
class Esp32s3ParallelIpsCapacitive1024x600
    : public Esp32s3ParallelIpsCapacitive {
 public:
  /// Create the 1024x600 board with @p orientation and @p i2c.
  Esp32s3ParallelIpsCapacitive1024x600(
      Orientation orientation = Orientation(),
      I2cMasterBusHandle i2c = I2cMasterBusHandle());

  /// Return calibration for the 1024x600 touch panel.
  TouchCalibration touch_calibration() override;
};

}  // namespace roo_display::products::makerfabs

#endif  // ESP32 && CONFIG_IDF_TARGET_ESP32S3
