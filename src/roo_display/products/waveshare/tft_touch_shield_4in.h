#pragma once

// https://www.waveshare.com/4inch-tft-touch-shield.htm
// Maker: Waveshare
// SKU: 13587

#include "roo_display/core/device.h"
#include "roo_display/core/orientation.h"
#include "roo_display/driver/ili9486.h"
#include "roo_display/driver/touch_xpt2046.h"
#include "roo_display/hal/spi.h"
#include "roo_display/products/combo_device.h"

namespace roo_display::products::waveshare {

static constexpr TouchCalibration kTft4inDefaultCalibration = TouchCalibration(
    161, 140, 3965, 3837, roo_display::Orientation::RightDown());

template <int8_t pinLcdCs, int8_t pinTouchCs, int8_t pinLcdDc,
          int8_t pinLcdReset = -1, int8_t pinLcdBacklit = -1>
class TftTouchShield4in : public ComboDevice {
 public:
  TftTouchShield4in(Orientation orientation = Orientation(),
                    roo_display::DefaultSpi spi = roo_display::DefaultSpi())
      : spi_(spi), display_(spi_), touch_(spi_) {
    display_.setOrientation(orientation);
  }

#if defined(ARDUINO)
  void initTransport() { spi_.init(); }
#endif

  void initTransport(uint8_t sck, uint8_t miso, uint8_t mosi) {
    spi_.init(sck, miso, mosi);
  }

  DisplayDevice& display() override { return display_; }

  TouchDevice* touch() override { return &touch_; }

  TouchCalibration touch_calibration() override {
    return kTft4inDefaultCalibration;
  }

 private:
  roo_display::DefaultSpi spi_;
  roo_display::Ili9486spi<pinLcdCs, pinLcdDc, pinLcdReset> display_;
  roo_display::TouchXpt2046<pinTouchCs> touch_;
};

}  // namespace roo_display::products::waveshare
