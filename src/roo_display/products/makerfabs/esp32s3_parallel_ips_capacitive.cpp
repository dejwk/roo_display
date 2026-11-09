
#include "roo_display/hal/config.h"

#if defined(ESP_PLATFORM) && CONFIG_IDF_TARGET_ESP32S3

#include <utility>

#include "roo_display/driver/st7701.h"
#include "roo_display/products/makerfabs/esp32s3_parallel_ips_capacitive.h"

#if defined(ARDUINO)
#include "roo_io/fs/esp32/arduino/sdmmc.h"
#include "roo_io/fs/esp32/arduino/sdspi.h"
#else
#include "roo_io/fs/esp32/esp-idf/sdmmc.h"
#include "roo_io/fs/esp32/esp-idf/sdspi.h"
#endif

namespace roo_display::products::makerfabs {

namespace {
// Makerfabs E32S3RGB40 (4-inch ST7701S):
// https://github.com/Makerfabs/ESP32-S3-Parallel-TFT-with-Touch-4inch
// The 4.3/7-inch boards use a
// different RGB pinout and do not require these 3-wire panel commands.
constexpr esp32s3_dma::Config kTftConfig480x480 = {.width = 480,
                                                   .height = 480,
                                                   .de = 45,
                                                   .hsync = 5,
                                                   .vsync = 4,
                                                   .pclk = 21,
                                                   .hsync_pulse_width = 8,
                                                   .hsync_back_porch = 50,
                                                   .hsync_front_porch = 10,
                                                   .hsync_polarity = 1,
                                                   .vsync_pulse_width = 8,
                                                   .vsync_back_porch = 20,
                                                   .vsync_front_porch = 10,
                                                   .vsync_polarity = 1,
                                                   .pclk_active_neg = 0,
                                                   .prefer_speed = 16000000,
                                                   .r0 = 39,
                                                   .r1 = 40,
                                                   .r2 = 41,
                                                   .r3 = 42,
                                                   .r4 = 2,
                                                   .g0 = 0,
                                                   .g1 = 9,
                                                   .g2 = 14,
                                                   .g3 = 47,
                                                   .g4 = 48,
                                                   .g5 = 3,
                                                   .b0 = 6,
                                                   .b1 = 7,
                                                   .b2 = 15,
                                                   .b3 = 16,
                                                   .b4 = 8,
                                                   .bswap = false};

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
    const esp32s3_dma::Config& config, Orientation orientation,
    I2cMasterBusHandle i2c)
    : i2c_(std::move(i2c)),
      display_(config),
      // Note: UART 'nack' errors have been observed when reset hold down time
      // is below 300ms. This startup delay isn't very painful because the
      // driver performs reset asynchronously.
      touch_(i2c_, -1, pin_touch_irq(), 300) {
  display_.setOrientation(orientation);
}

void Esp32s3ParallelIpsCapacitive::initTransport() {
  i2c_.init(pin_sda(), pin_scl());
#if defined(ARDUINO)
  roo_io::SD_MMC.setPins(pin_sd_clk(), pin_sd_cmd(), pin_sd_d0());
#else
  roo_io::SDMMC.setPins(pin_sd_clk(), pin_sd_cmd(), pin_sd_d0());
#endif
}

roo_io::BaseEsp32VfsFilesystem& Esp32s3ParallelIpsCapacitive::sd() const {
#if defined(ARDUINO)
  return roo_io::SD_MMC;
#else
  return roo_io::SDMMC;
#endif
}

Esp32s3ParallelIpsCapacitive480x480::Esp32s3ParallelIpsCapacitive480x480(
    Orientation orientation, I2cMasterBusHandle i2c)
    : i2c_(std::move(i2c)),
      display_(kTftConfig480x480),
      touch_(i2c_, -1, pin_touch_irq(), 300) {
  display_.setOrientation(orientation);
}

void Esp32s3ParallelIpsCapacitive480x480::initTransport() {
  i2c_.init(pin_sda(), pin_scl());
  // Panel clock and data share the SD slot's SPI pins; init leaves CS high.
  st7701::Spi9Bit target(pin_st7701_cs(), pin_sd_sck(), pin_sd_mosi());
  target.init();
  st7701::Init().init(target);
#if defined(ARDUINO)
  roo_io::SD_SPI.setCsPin(pin_sd_cs());
  SPI.begin(pin_sd_sck(), pin_sd_miso(), pin_sd_mosi());
#else
  roo_io::SDSPI.setCsPin(pin_sd_cs());
#endif
}

roo_io::BaseEsp32VfsFilesystem& Esp32s3ParallelIpsCapacitive480x480::sd()
    const {
#if defined(ARDUINO)
  return roo_io::SD_SPI;
#else
  return roo_io::SDSPI;
#endif
}

TouchCalibration Esp32s3ParallelIpsCapacitive480x480::touch_calibration() {
  return TouchCalibration(0, 0, 480, 480);
}

Esp32s3ParallelIpsCapacitive800x480::Esp32s3ParallelIpsCapacitive800x480(
    Orientation orientation, I2cMasterBusHandle i2c)
    : Esp32s3ParallelIpsCapacitive(kTftConfig800x480, orientation,
                                   std::move(i2c)) {}

TouchCalibration Esp32s3ParallelIpsCapacitive800x480::touch_calibration() {
  return TouchCalibration(0, 0, 800, 480);
}

Esp32s3ParallelIpsCapacitive1024x600::Esp32s3ParallelIpsCapacitive1024x600(
    Orientation orientation, I2cMasterBusHandle i2c)
    : Esp32s3ParallelIpsCapacitive(kTftConfig1024x600, orientation,
                                   std::move(i2c)) {}

TouchCalibration Esp32s3ParallelIpsCapacitive1024x600::touch_calibration() {
  return TouchCalibration(0, 0, 1024,
                          768);  // Yes, 768 is the correct value.
}

}  // namespace roo_display::products::makerfabs

#endif  // defined(ESP_PLATFORM) && CONFIG_IDF_TARGET_ESP32S3
