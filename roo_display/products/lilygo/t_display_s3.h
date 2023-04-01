#pragma once

// https://www.lilygo.cc/products/t-display-s3
// Maker: LILYGO
// Product Code: T-Display-S3

#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/st7789.h"
#include "roo_display/products/combo_device.h"
#include "roo_display/transport/esp32s3/parallel_lcd_8bit.h"

namespace roo_display::products::lilygo {

class TDisplayS3 : public ComboDevice {
 public:
  TDisplayS3(Orientation orientation = Orientation())
      : display_(esp32s3::ParallelLcd8Bit(
            6, 7, 5, 8, 9,
            esp32s3::ParallelLcd8Bit::DataBus{.pinD0 = 39,
                                              .pinD1 = 40,
                                              .pinD2 = 41,
                                              .pinD3 = 42,
                                              .pinD4 = 45,
                                              .pinD5 = 46,
                                              .pinD6 = 47,
                                              .pinD7 = 48})) {
    display_.setOrientation(orientation);
  }

  void initTransport() {}

  DisplayDevice& display() override { return display_; }

 private:
  roo_display::St7789_Generic<esp32s3::ParallelLcd8Bit, 170, 320, 35, 0, 35, 0>
      display_;
};

}  // namespace roo_display::products::lilygo
