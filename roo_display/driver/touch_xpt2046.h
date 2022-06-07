#pragma once

#include "roo_display/core/device.h"
#include "roo_display/driver/common/touch_calibrated.h"
#include "roo_display/hal/gpio.h"
#include "roo_display/hal/transport.h"

namespace roo_display {

static const int kSpiTouchFrequency = 2500000;

typedef SpiSettings<kSpiTouchFrequency, MSBFIRST, SPI_MODE0>
    TouchXpt2046SpiSettings;

template <int pinCS, typename Spi = DefaultSpi, typename Gpio = DefaultGpio>
class TouchXpt2046Uncalibrated : public TouchDevice {
 public:
  explicit TouchXpt2046Uncalibrated(Spi spi = Spi());

  bool getTouch(int16_t& x, int16_t& y, int16_t& z) override;

 private:
  BoundSpi<Spi, TouchXpt2046SpiSettings> spi_transport_;

  bool pressed_;
  unsigned long latest_confirmed_pressed_timestamp_;
  uint16_t press_x_;
  uint16_t press_y_;
  uint16_t press_z_;
};

template <int pinCS, typename Spi = DefaultSpi, typename Gpio = DefaultGpio>
class TouchXpt2046 : public TouchDevice {
 public:
  TouchXpt2046(Spi spi = Spi());

  TouchXpt2046(TouchCalibration calibration, Spi spi = Spi());

  void setCalibration(TouchCalibration calibration) {
    calibration_ = std::move(calibration);
  }

  bool getTouch(int16_t& x, int16_t& y, int16_t& z) override;

 private:
  TouchCalibration calibration_;
  TouchXpt2046Uncalibrated<pinCS, Spi, Gpio> uncalibrated_;
};

// If two subsequent reads are further apart than this parameter, in either
// X or Y direction, their conversion is rejected as UNSETTLED.
// This parameter helps to filter out random noise. If the value is too small,
// though, the panel may have a hard time registering touch.
static const int kMaxRawSettlingDistance = 200;

// How many conversions we attempt in order to get to kMinSettledConversions.
// If we reach this limit, we return the result based on however many settled
// conversions we have seen, even if it is below the minimum.
static const int kMaxConversionAttempts = 100;

// How many settled conversions is enough to consider the reading reliable.
static const int kMinSettledConversions = 8;

// How hard the press needs to be to count as touch.
static const int kInitialTouchZThreshold = 200;

// How hard the press needs to be to count as continued touch (i.e., once
// the display has been touched). See kTouchSensitivityLagMs.
static const int kSustainedTouchZThreshold = 20;

// To avoid spurious drops in drag touch, the driver is more sensitive to touch
// (and thus more prone to picking up noise) for this many ms since last
// definitive touch.
static const int kTouchSensitivityLagMs = 250;

// If there is no touch detected for up to this many ms, we will still report
// that the pad is touched, with previously reported coordinates.
static const int kTouchIntertiaMs = 80;

// Implementation follows.

template <int pinCS, typename Spi, typename Gpio>
TouchXpt2046Uncalibrated<pinCS, Spi, Gpio>::TouchXpt2046Uncalibrated(Spi spi)
    : spi_transport_(std::move(spi)),
      pressed_(false),
      latest_confirmed_pressed_timestamp_(0),
      press_x_(0),
      press_y_(0),
      press_z_(0) {
  Gpio::setOutput(pinCS);
  Gpio::template setHigh<pinCS>();
}

template <typename Spi>
void get_raw_touch_xy(Spi& spi, uint16_t* x, uint16_t* y) {
  uint16_t last_x = -1;
  uint16_t last_y = -1;
  spi.transfer(0xd3);
  *x = spi.transfer16(0xd3) >> 3;
  *x = spi.transfer16(0xd3) >> 3;
  *x = spi.transfer16(0x93) >> 3;

  // Start bit + XP sample request for y position
  // spi.transfer(0x93);
  *y = spi.transfer16(0x93) >> 3;
  *y = spi.transfer16(0x93) >> 3;
  *y = spi.transfer16(0x00) >> 3;
}

template <typename Spi>
uint16_t get_raw_touch_z(Spi& spi) {
  int16_t tz = 0xFFF;
  spi.transfer(0xb1);
  tz += spi.transfer16(0xc1) >> 3;
  tz -= spi.transfer16(0) >> 3;
  return (uint16_t)tz;
}

enum ConversionResult { UNTOUCHED = 0, TOUCHED = 1, UNSETTLED = 2 };

template <typename Spi>
ConversionResult single_conversion(Spi& spi, uint16_t z_threshold, uint16_t* x,
                                   uint16_t* y, uint16_t* z) {
  // Wait until pressure stops increasing
  uint16_t z1 = 1;
  uint16_t z2 = 0;
  while (z1 > z2) {
    z2 = z1;
    z1 = get_raw_touch_z(spi);
  }
  if (z1 <= z_threshold) {
    return UNTOUCHED;
  }

  uint16_t x1, y1, x2, y2;
  get_raw_touch_xy(spi, &x1, &y1);
  get_raw_touch_xy(spi, &x2, &y2);
  if (abs(x1 - x2) > kMaxRawSettlingDistance) return UNSETTLED;
  if (abs(y1 - y2) > kMaxRawSettlingDistance) return UNSETTLED;
  uint16_t z3 = get_raw_touch_z(spi);
  if (z3 <= z_threshold) {
    return UNSETTLED;
  }

  // We have a valid touch sample pair. Return an arithmetic average as the
  // result.
  *x = (x1 + x2) / 2;
  *y = (y1 + y2) / 2;
  *z = z3;
  return TOUCHED;
}

template <int pinCS, typename Spi, typename Gpio>
bool TouchXpt2046Uncalibrated<pinCS, Spi, Gpio>::getTouch(int16_t& x,
                                                          int16_t& y,
                                                          int16_t& z) {
  unsigned long now = millis();
  int z_threshold = kInitialTouchZThreshold;
  if (pressed_ &&
      (now - latest_confirmed_pressed_timestamp_ < kTouchSensitivityLagMs) &&
      z_threshold > kSustainedTouchZThreshold) {
    z_threshold = kSustainedTouchZThreshold;
  }
  BoundSpiTransaction<pinCS, decltype(spi_transport_), Gpio> transaction(
      spi_transport_);

  int settled_conversions = 0;
  uint16_t x_tmp, y_tmp, z_tmp;
  int32_t x_sum = 0;
  int32_t y_sum = 0;
  int32_t z_max = 0;
  int count = 0;

  // Discard a few initial conversions so that the sensor settles.
  for (int i = 0; i < 5; ++i) {
    ConversionResult result =
        single_conversion(spi_transport_, z_threshold, &x_tmp, &y_tmp, &z_tmp);
    if (result == UNSETTLED) continue;
  }

  bool touched = false;
  for (int i = 0; i < kMaxConversionAttempts; ++i) {
    ConversionResult result =
        single_conversion(spi_transport_, z_threshold, &x_tmp, &y_tmp, &z_tmp);
    if (result == UNSETTLED) continue;
    settled_conversions++;
    if (result == TOUCHED) {
      touched = true;
      x_sum += x_tmp;
      y_sum += y_tmp;
      if (z_max < z_tmp) z_max = z_tmp;
      count++;
    }
    if (settled_conversions >= kMinSettledConversions) {
      // We got enough settled conversions to return a result.
      if (touched) {
        x = press_x_ = 4095 - (x_sum / count);
        y = press_y_ = 4095 - (y_sum / count);
        z = press_z_ = z_max;
        if (z >= kInitialTouchZThreshold) {
          // We got a definite press.
          latest_confirmed_pressed_timestamp_ = now;
        }
      } else if (pressed_ &&
                 now - latest_confirmed_pressed_timestamp_ < kTouchIntertiaMs) {
        // We did not detect touch, but the latest confirmed touch was not long
        // ago so we report as touched anyway.
        touched = true;
        x = press_x_;
        y = press_y_;
        z = press_z_;
      }
      pressed_ = touched;
      return touched;
    }
  }
  // No reliable readout despite numerous attempts - aborting.
  pressed_ = false;
  return false;
}

template <int pinCS, typename Spi, typename Gpio>
TouchXpt2046<pinCS, Spi, Gpio>::TouchXpt2046(Spi spi)
    : calibration_(), uncalibrated_(std::move(spi)) {}

template <int pinCS, typename Spi, typename Gpio>
TouchXpt2046<pinCS, Spi, Gpio>::TouchXpt2046(TouchCalibration calibration,
                                             Spi spi)
    : calibration_(std::move(calibration)), uncalibrated_(std::move(spi)) {}

template <int pinCS, typename Spi, typename Gpio>
bool TouchXpt2046<pinCS, Spi, Gpio>::getTouch(int16_t& x, int16_t& y,
                                              int16_t& z) {
  bool touched = uncalibrated_.getTouch(x, y, z);
  return touched ? calibration_.Calibrate(x, y, z) : false;
}

}  // namespace roo_display
