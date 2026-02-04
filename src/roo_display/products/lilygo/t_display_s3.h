#pragma once

// https://www.lilygo.cc/products/t-display-s3
// Maker: LILYGO
// Product Code: T-Display-S3

#if !defined(ESP32) || !(CONFIG_IDF_TARGET_ESP32S3)
#warning Compilation target must be ESP32_S3 for this device.
#else

#include "roo_display/backlit/esp32_ledc.h"
#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/st7789.h"
#include "roo_display/products/combo_device.h"
#include "roo_display/transport/esp32s3/parallel_lcd_8bit.h"

namespace roo_display::products::lilygo {

/// LILYGO T-Display S3 device wrapper.
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
                                              .pinD7 = 48})),
                                              backlit_(38) {
    display_.setOrientation(orientation);
  }

  /// Initialize transport/backlight.
  void initTransport() {
    backlit_.begin();
  }

  /// Return display device.
  DisplayDevice& display() override { return display_; }

  /// Return backlight controller.
  Backlit& backlit() { return backlit_; }

 private:
  roo_display::St7789_Generic<esp32s3::ParallelLcd8Bit, 170, 320, 35, 0, 35, 0>
      display_;

  LedcBacklit backlit_;
};

}  // namespace roo_display::products::lilygo

#endif
