#pragma once

#include "roo_display/core/device.h"
#include "roo_display/hal/gpio.h"
#include "roo_display/hal/transport.h"

namespace roo_display {

static const int kSpiTouchFrequency = 2500000;

typedef SpiSettings<kSpiTouchFrequency, MSBFIRST, SPI_MODE0>
    TouchXpt2046SpiSettings;

template <int pinCS, typename Spi = DefaultSpi, typename Gpio = DefaultGpio>
class TouchXpt2046 : public TouchDevice {
 public:
  explicit TouchXpt2046(Spi spi = Spi());

  bool getTouch(int16_t& x, int16_t& y, int16_t& z) override;

 private:
  BoundSpi<Spi, TouchXpt2046SpiSettings> spi_transport_;

  bool pressed_;
  unsigned long latest_confirmed_pressed_timestamp_;
  int16_t press_x_;
  int16_t press_y_;
  int16_t press_z_;
  int16_t press_vx_;
  int16_t press_vy_;
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
static const int kInitialTouchZThreshold = 400;

// How hard the press needs to be to count as continued touch (i.e., once
// the display has been touched). See kTouchSensitivityLagMs.
static const int kSustainedTouchZThreshold = 300;

// To avoid spurious drops in drag touch, the driver is more sensitive to touch
// (and thus more prone to picking up noise) for this many ms since last
// definitive touch.
static const int kTouchSensitivityLagMs = 250;

// If there is no touch detected for up to this many ms, we will still report
// that the pad is touched, with previously reported coordinates.
static const int kTouchIntertiaMs = 60;

// Implementation follows.

template <int pinCS, typename Spi, typename Gpio>
TouchXpt2046<pinCS, Spi, Gpio>::TouchXpt2046(Spi spi)
    : spi_transport_(std::move(spi)),
      pressed_(false),
      latest_confirmed_pressed_timestamp_(0),
      press_x_(0),
      press_y_(0),
      press_z_(0),
      press_vx_(0),
      press_vy_(0) {
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
bool TouchXpt2046<pinCS, Spi, Gpio>::getTouch(int16_t& x, int16_t& y,
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
    unsigned long dt = now - latest_confirmed_pressed_timestamp_;
    if (settled_conversions >= kMinSettledConversions) {
      // We got enough settled conversions to return a result.
      if (touched) {
        x = 4095 - (x_sum / count);
        y = 4095 - (y_sum / count);
        z = z_max;
        if (!pressed_) {
          // First touch.
          press_vx_ = 0;
          press_vy_ = 0;
        } else {
          if (dt != 0) {
            // Note: the velocity unit is fairly arbitrary, chosen
            // so that the velocity fits in int16_t.
            press_vx_ = 8 * (x - (int16_t)press_x_) / (long)dt;
            press_vy_ = 8 * (y - (int16_t)press_y_) / (long)dt;
          }
        }
        press_x_ = x;
        press_y_ = y;
        press_z_ = z;
        if (z >= kInitialTouchZThreshold) {
          // We got a definite press.
          latest_confirmed_pressed_timestamp_ = now;
        }
      } else if (pressed_ && dt < kTouchIntertiaMs) {
        // We did not detect touch, but the latest confirmed touch was not long
        // ago so we extrapolate as touched anyway, unless it would go out of
        // bounds.
        int16_t maybe_x = press_x_ + (press_vx_ * (long)dt) / 8;
        int16_t maybe_y = press_y_ + (press_vy_ * (long)dt) / 8;
        // TODO: this extrapolation should probably be based on smoothed
        // values...
        if (maybe_x >= 0 && maybe_x <= 4095 && maybe_y >= 0 &&
            maybe_y <= 4095) {
          touched = true;
          x = maybe_x;
          y = maybe_y;
          z = press_z_;
        }
      }
      pressed_ = touched;
      return touched;
    }
  }
  // No reliable readout despite numerous attempts - aborting.
  pressed_ = false;
  return false;
}

}  // namespace roo_display
