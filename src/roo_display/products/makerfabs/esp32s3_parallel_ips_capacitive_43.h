#pragma once

// https://www.makerfabs.com/esp32-s3-parallel-tft-with-touch-4-3-inch.html
// Maker: MakerFabs
// Product Code: E32S3RGB43

#include "roo_display/backlit/esp32_ledc.h"
#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/esp32s3_dma_parallel_rgb565.h"
#include "roo_display/driver/touch_gt911.h"
#include "roo_display/hal/i2c.h"
#include "roo_display/hal/spi.h"
#include "roo_display/products/combo_device.h"

namespace roo_display::products::makerfabs {

namespace {
constexpr esp32s3_dma::Config kTftConfig = {.width = 800,
                                            .height = 480,
                                            .de = 40,
                                            .hsync = 39,
                                            .vsync = 41,
                                            .pclk = 42,
                                            .hsync_pulse_width = 4,
                                            .hsync_back_porch = 8,
                                            .hsync_front_porch = 8,
                                            .hsync_polarity = 0,
                                            .vsync_pulse_width = 4,
                                            .vsync_back_porch = 8,
                                            .vsync_front_porch = 8,
                                            .vsync_polarity = 0,
                                            .pclk_active_neg = 1,
                                            .prefer_speed = -1,
                                            .r0 = 45,
                                            .r1 = 48,
                                            .r2 = 47,
                                            .r3 = 21,
                                            .r4 = 14,
                                            .g0 = 5,
                                            .g1 = 6,
                                            .g2 = 7,
                                            .g3 = 15,
                                            .g4 = 16,
                                            .g5 = 4,
                                            .b0 = 8,
                                            .b1 = 3,
                                            .b2 = 46,
                                            .b3 = 9,
                                            .b4 = 1,
                                            .bswap = false};
}

/// Makerfabs ESP32-S3 4.3" parallel IPS capacitive device.
class Esp32s3ParallelIpsCapacitive43 : public ComboDevice {
 public:
  Esp32s3ParallelIpsCapacitive43(
      Orientation orientation = Orientation(),
      roo_display::I2cMasterBusHandle i2c = roo_display::I2cMasterBusHandle(),
      int pwm_channel = 1)
      : spi_(),
        i2c_(i2c),
        display_(kTftConfig),
        touch_(i2c_, -1, 38),
        backlit_(2) {
    display_.setOrientation(orientation);
  }

  /// Initialize I2C transport for touch.
  void initTransport() { i2c_.init(17, 18); }

  /// Return display device.
  DisplayDevice& display() override { return display_; }

  /// Return touch device.
  TouchDevice* touch() override { return &touch_; }

  /// Return touch calibration.
  TouchCalibration touch_calibration() override {
    return TouchCalibration(0, 0, 800, 480);
  }

  /// Return SPI bus handle.
  roo_display::DefaultSpi& spi() { return spi_; }
  /// SD card CS pin.
  constexpr int8_t sd_cs() const { return 10; }

  /// Return backlight controller.
  Backlit& backlit() { return backlit_; }

 private:
  roo_display::DefaultSpi spi_;
  roo_display::I2cMasterBusHandle i2c_;
  roo_display::esp32s3_dma::ParallelRgb565Buffered display_;
  roo_display::TouchGt911 touch_;
  LedcBacklit backlit_;
};

}  // namespace roo_display::products::makerfabs
