#pragma once

// https://www.makerfabs.com/esp32-s3-parallel-tft-with-touch-7-inch.html
// Maker: MakerFabs
// Product Code: MTESPS37

#include <SPI.h>
#include <Wire.h>

#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/esp32s3_dma_parallel_rgb565.h"
#include "roo_display/driver/touch_gt911.h"
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
                                            .hsync_back_porch = 16,
                                            .hsync_front_porch = 80,
                                            .hsync_polarity = 0,
                                            .vsync_pulse_width = 4,
                                            .vsync_back_porch = 4,
                                            .vsync_front_porch = 22,
                                            .vsync_polarity = 0,
                                            .pclk_active_neg = 1,
                                            .prefer_speed = 16000000,

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

class Esp32s3ParallelIpsCapacitive70 : public ComboDevice {
 public:
  Esp32s3ParallelIpsCapacitive70(Orientation orientation = Orientation(),
                                 decltype(Wire)& wire = Wire,
                                 int pwm_channel = 1)
      : spi_(HSPI), wire_(wire), display_(kTftConfig), touch_(wire, -1, 38) {
    display_.setOrientation(orientation);
    digitalWrite(10, LOW);
  }

  void initTransport() {
    wire_.begin(17, 18);
  }

  DisplayDevice& display() override { return display_; }

  TouchDevice* touch() override { return &touch_; }

  decltype(SPI)& spi() { return spi_; }
  constexpr int8_t sd_cs() const { return 10; }

  TouchCalibration touch_calibration() override {
    return TouchCalibration(0, 0, 800, 480);
  }

 private:
  decltype(SPI) spi_;
  decltype(Wire)& wire_;
  roo_display::esp32s3_dma::ParallelRgb565Buffered display_;
  // LAZY is much faster (up to 2x!) but unfortunately causes display tearing;
  // the update rate seems too fast for ESP32-S3.
  // roo_display::esp32s3_dma::ParallelRgb565<esp32s3_dma::FLUSH_MODE_LAZY>
  // display_;
  // roo_display::esp32s3_dma::ParallelRgb565<esp32s3_dma::FLUSH_MODE_AGGRESSIVE>
  // display_;
  roo_display::TouchGt911 touch_;
};

}  // namespace roo_display::products::makerfabs
