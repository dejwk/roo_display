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

#if !defined(ESP32) || !(CONFIG_IDF_TARGET_ESP32S3)
#warning Compilation target must be ESP32_S3 for this device.
#else

namespace roo_display::products::makerfabs {

class Esp32s3ParallelIpsCapacitive : public ComboDevice {
 public:
  enum Resolution { k800x480, k1024x600 };
  Esp32s3ParallelIpsCapacitive(Resolution resolution,
                               Orientation orientation = Orientation(),
                               decltype(Wire)& wire = Wire);

  void initTransport() {
    pinMode(10, OUTPUT);
    digitalWrite(10, LOW);
    wire_.begin(17, 18);
  }

  DisplayDevice& display() override { return display_; }

  TouchDevice* touch() override { return &touch_; }

  decltype(SPI)& spi() { return spi_; }
  constexpr int8_t sd_cs() const { return 10; }

  TouchCalibration touch_calibration() override;

 private:
  Resolution resolution_;
  decltype(SPI) spi_;
  decltype(Wire)& wire_;
  roo_display::esp32s3_dma::ParallelRgb565<esp32s3_dma::FLUSH_MODE_LAZY>
      display_;
  roo_display::TouchGt911 touch_;
};

class Esp32s3ParallelIpsCapacitive800x480
    : public Esp32s3ParallelIpsCapacitive {
 public:
  Esp32s3ParallelIpsCapacitive800x480(Orientation orientation = Orientation(),
                                      decltype(Wire)& wire = Wire)
      : Esp32s3ParallelIpsCapacitive(k800x480, orientation, wire) {}
};

class Esp32s3ParallelIpsCapacitive1024x600
    : public Esp32s3ParallelIpsCapacitive {
 public:
  Esp32s3ParallelIpsCapacitive1024x600(Orientation orientation = Orientation(),
                                       decltype(Wire)& wire = Wire)
      : Esp32s3ParallelIpsCapacitive(k1024x600, orientation, wire) {}
};

}  // namespace roo_display::products::makerfabs

#endif  // ESP32 && CONFIG_IDF_TARGET_ESP32S3