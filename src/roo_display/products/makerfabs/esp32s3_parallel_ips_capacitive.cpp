
#include <Arduino.h>

#if !defined(ESP32) || !(CONFIG_IDF_TARGET_ESP32S3)
#else

#include "roo_display/products/makerfabs/esp32s3_parallel_ips_capacitive.h"

namespace roo_display::products::makerfabs {

namespace {
constexpr esp32s3_dma::Config kTftConfig800x480 = {.width = 800,
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

constexpr esp32s3_dma::Config kTftConfig1024x600 = {.width = 1024,
                                                    .height = 600,
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
}  // namespace

Esp32s3ParallelIpsCapacitive::Esp32s3ParallelIpsCapacitive(
    Resolution resolution, Orientation orientation, decltype(Wire)& wire)
    : resolution_(resolution),
      spi_(HSPI),
      wire_(wire),
      display_(resolution_ == k800x480 ? kTftConfig800x480
                                       : kTftConfig1024x600),
      // Note: UART 'nack' errors have been observed when reset hold down time
      // is below 300ms. This startup delay isn't very painful because the
      // driver performs reset asynchronously.
      touch_(wire, -1, 38, 300) {
  display_.setOrientation(orientation);
}

TouchCalibration Esp32s3ParallelIpsCapacitive::touch_calibration() {
  return resolution_ == k800x480
             ? TouchCalibration(0, 0, 800, 480)
             : TouchCalibration(0, 0, 1024,
                                768);  // Yes, that's the correct value.
}

}  // namespace roo_display::products::makerfabs

#endif  // ESP32 && CONFIG_IDF_TARGET_ESP32S3
