#include "touch_xpt2046.h"

#include "roo_display/core/box.h"

static const int kSpiTouchFrequency = 2500000;

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
static const int kInitialTouchZThreshold = 100;

// How hard the press needs to be to count as continued touch (i.e., once
// the display has been touched). See kTouchSensitivityLagMs.
static const int kSustainedTouchZThreshold = 50;

// To avoid spurious drops in drag touch, the driver is more sensitive to touch
// (and thus more prone to picking up noise) for this many ms since last
// definitive touch.
static const int kTouchSensitivityLagMs = 200;

// If there is no touch detected for up to this many ms, we will still report
// that the pad is touched, with previously reported coordinates.
static const int kTouchIntertiaMs = 100;

// How do we exponentially average subsequent conversion. In the range of (0 -
// 255, where 0 is no smoothing, i.e., the last converted value is returned.
// Generally, the last converted value is taken with weight of smoothing_factor
// / 256.
static const int kExponentialSmoothingFactor = 224;

namespace roo_display {

TouchXpt2046Uncalibrated::TouchXpt2046Uncalibrated(int pin_cs,
                                                   decltype(SPI)& spi)
    : spi_config_(
          GenericSpi(spi, SPISettings(kSpiTouchFrequency, MSBFIRST, SPI_MODE0)),
          pin_cs),
      pressed_(false),
      latest_confirmed_pressed_timestamp_(0),
      press_x_(0),
      press_y_(0),
      press_z_(0) {
  pinMode(pin_cs, OUTPUT);
  digitalWrite(pin_cs, HIGH);
}

void get_raw_touch_xy(GenericSpi& spi, uint16_t* x, uint16_t* y) {
  spi.transfer(0xd0);
  *x = spi.transfer16(0) >> 3;

  // Start bit + XP sample request for y position
  spi.transfer(0x90);
  *y = spi.transfer16(0) >> 3;
}

uint16_t get_raw_touch_z(GenericSpi& spi) {
  int16_t tz = 0xFFF;
  spi.transfer(0xb1);
  tz += spi.transfer16(0xc1) >> 3;
  tz -= spi.transfer16(0x91) >> 3;
  return (uint16_t)tz;
}

enum ConversionResult { UNTOUCHED = 0, TOUCHED = 1, UNSETTLED = 2 };

ConversionResult single_conversion(GenericSpi& spi, uint16_t z_threshold,
                                   uint16_t* x, uint16_t* y, uint16_t* z) {
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

  // We have a valid touch sample pair. Return an arithmetic average as the
  // result.
  *x = (x1 + x2) / 2;
  *y = (y1 + y2) / 2;
  return TOUCHED;
}

class Smoother {
 public:
  Smoother(int smoothing_factor)
      : smoothing_factor_(smoothing_factor), value_(0), initialized_(false) {}
  uint16_t value() const { return value_; }

  void update(uint16_t value) {
    if (!initialized_) {
      value_ = value;
      initialized_ = true;
    } else {
      value_ = ((uint32_t)value_ * smoothing_factor_ +
                (uint32_t)value * (256 - smoothing_factor_)) /
               256;
    }
  }

 private:
  int smoothing_factor_;
  uint16_t value_;
  bool initialized_;
};

bool TouchXpt2046Uncalibrated::getTouch(int16_t* x, int16_t* y, int16_t* z) {
  unsigned long now = millis();
  int z_threshold = kInitialTouchZThreshold;
  if (pressed_ &&
      (now - latest_confirmed_pressed_timestamp_ < kTouchSensitivityLagMs) &&
      z_threshold > kSustainedTouchZThreshold) {
    z_threshold = kSustainedTouchZThreshold;
  }
  SpiTransaction transaction(spi_config_);

  int settled_conversions = 0;
  uint16_t x_tmp, y_tmp, z_tmp;
  Smoother x_result(kExponentialSmoothingFactor);
  Smoother y_result(kExponentialSmoothingFactor);
  Smoother z_result(kExponentialSmoothingFactor);

  bool touched = false;
  for (int i = 0; i < kMaxConversionAttempts; ++i) {
    ConversionResult result = single_conversion(spi_config_.spi(), z_threshold,
                                                &x_tmp, &y_tmp, &z_tmp);
    if (result == UNSETTLED) continue;
    settled_conversions++;
    if (result == TOUCHED) {
      touched = true;
      x_result.update(x_tmp);
      y_result.update(y_tmp);
      z_result.update(z_tmp);
    }
    if (settled_conversions >= kMinSettledConversions) {
      // We got enough settled conversions to return a result.
      if (touched) {
        *x = press_x_ = 4096 - x_result.value();
        *y = press_y_ = 4096 - y_result.value();
        *z = press_z_ = z_result.value();
        if (z_result.value() >= kInitialTouchZThreshold) {
          // We got a definite press.
          latest_confirmed_pressed_timestamp_ = now;
        }
      } else if (pressed_ &&
                 now - latest_confirmed_pressed_timestamp_ < kTouchIntertiaMs) {
        // We did not detect touch, but the latest confirmed touch was not long
        // ago so we report as touched anyway.
        touched = true;
        *x = press_x_;
        *y = press_y_;
        *z = press_z_;
      }
      pressed_ = touched;
      return touched;
    }
  }
  // No reliable readout despite numerous attempts - aborting.
  pressed_ = false;
  return false;
}

TouchXpt2046::TouchXpt2046(int pin_cs, decltype(SPI)& spi)
    : calibration_(), uncalibrated_(pin_cs, spi) {}

TouchXpt2046::TouchXpt2046(TouchCalibration calibration, int pin_cs,
                           decltype(SPI)& spi)
    : calibration_(std::move(calibration)), uncalibrated_(pin_cs, spi) {}

bool TouchXpt2046::getTouch(int16_t* x, int16_t* y, int16_t* z) {
  bool touched = uncalibrated_.getTouch(x, y, z);
  return touched ? calibration_.Calibrate(x, y, z) : false;
}

}  // namespace roo_display
