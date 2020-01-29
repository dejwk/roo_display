#pragma once

#include "roo_display/core/device.h"
#include "roo_display/driver/common/touch_calibrated.h"
#include "roo_display/hal/transport.h"

namespace roo_display {

class TouchXpt2046Uncalibrated : public TouchDevice {
 public:
  TouchXpt2046Uncalibrated(int pin_cs, decltype(SPI) &spi = SPI);

  bool getTouch(int16_t *x, int16_t *y, int16_t *z) override;

 private:
  SpiTransactionConfig spi_config_;

  bool pressed_;
  unsigned long latest_confirmed_pressed_timestamp_;
  uint16_t press_x_;
  uint16_t press_y_;
  uint16_t press_z_;
};

class TouchXpt2046 : public TouchDevice {
 public:
  TouchXpt2046(int pin_cs, decltype(SPI) &spi = SPI);

  TouchXpt2046(TouchCalibration calibration, int pin_cs,
               decltype(SPI) &spi = SPI);

  void setCalibration(TouchCalibration calibration) {
    calibration_ = std::move(calibration);
  }

  bool getTouch(int16_t *x, int16_t *y, int16_t *z) override;

 private:
  TouchCalibration calibration_;
  TouchXpt2046Uncalibrated uncalibrated_;
};

}  // namespace roo_display
