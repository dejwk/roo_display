#pragma once

#include <algorithm>

#include "roo_display/core/device.h"
#include "roo_time.h"

namespace roo_display {

// Base class for touch drivers. Takes care of capping hardware sampling
// frequency, data smoothing, and spurious gaps in the readout.
template <int max_touch_points>
class BasicTouchDevice : public TouchDevice {
 public:
  struct Config {
    // If a request comes within this interval from a previous read, the results
    // of the previous read are reported. This setting reduces the maximum
    // device sampling frequency.
    int min_sampling_interval_ms;

    // If there is no touch detected for up to this many ms, we will still
    // report that the pad is touched, with previously reported coordinates.
    // This helps overcome brief 'lost-contact' touch gaps, which can throw off
    // gesture detection algorithms.
    int touch_intertia_ms;

    // Controls exponential smoothing. In (0, 1], where 0 corresponds to no
    // smoothing, and 1 corresponds to infinite smoothing. The value represents
    // weight of the old sample at dt = 10ms behind the new sample.
    float smoothing_factor;
  };

  BasicTouchDevice(Config config)
      : config_(std::move(config)),
        detection_timestamp_(roo_time::Uptime::Start()),
        touch_points_(),
        points_touched_(0) {}

  virtual ~BasicTouchDevice() = default;

  TouchResult getTouch(TouchPoint* points, int max_points) override;

 protected:
  virtual int readTouch(TouchPoint* points) = 0;

 private:
  TouchResult pushResult(TouchPoint* points, int max_points) {
    size_t point_count =
        std::min(points_touched_, std::min(max_points, max_touch_points));
    for (size_t i = 0; i < point_count; ++i) {
      points[i] = touch_points_[i];
    }
    return TouchResult(detection_timestamp_, points_touched_);
  }

  Config config_;
  roo_time::Uptime detection_timestamp_;
  TouchPoint touch_points_[max_touch_points];
  int points_touched_;
};

template <int max_touch_points>
TouchResult BasicTouchDevice<max_touch_points>::getTouch(TouchPoint* points,
                                                         int max_points) {
  roo_time::Uptime now = roo_time::Uptime::Now();
  roo_time::Duration dt = now - detection_timestamp_;
  if (dt < roo_time::Millis(config_.min_sampling_interval_ms)) {
    // Immediately return the last result without attempting to scan.
    return pushResult(points, max_points);
  }
  TouchPoint readout[max_touch_points];
  int points_touched = readTouch(readout);
  if (points_touched == 0) {
    if (dt < roo_time::Millis(config_.touch_intertia_ms)) {
      // We did not detect touch, but the latest confirmed touch was not long
      // ago so we report that one anyway, but do not update the
      // detection_timestamp_ to reflect that we're reporting a stale value.
      return pushResult(points, max_points);
    }
    // Report definitive no touch.
    detection_timestamp_ = now;
    points_touched_ = 0;
    return pushResult(points, max_points);
  }
  // Touch has been detected. Need to smooth the values and report it.
  float alpha = 1 - pow(config_.smoothing_factor, dt.inMicros() / 10000.0);
  for (int i = 0; i < points_touched; i++) {
    TouchPoint& p = readout[i];
    p.vx = 0;
    p.vy = 0;
    // Identify a previous touch coordinate, if any.
    for (int j = 0; j < points_touched_; ++j) {
      if (touch_points_[j].id == p.id) {
        TouchPoint& prev = touch_points_[j];
        // Match! Smooth out the value, and calculate velocity.
        int16_t x = prev.x * (1 - alpha) + p.x * alpha;
        int16_t y = prev.y * (1 - alpha) + p.y * alpha;
        int16_t z = prev.z * (1 - alpha) + p.z * alpha;
        if (dt > roo_time::Micros(0)) {
          // Velocity in pixels / s.
          p.vx = 1000000LL * (x - prev.x) / dt.inMicros();
          p.vy = 1000000LL * (y - prev.y) / dt.inMicros();
        }
        p.x = x;
        p.y = y;
        p.z = z;
        continue;
      }
    }
  }
  // Copy over and report.
  detection_timestamp_ = now;
  points_touched_ = points_touched;
  std::copy(readout, readout + points_touched, touch_points_);
  return pushResult(points, max_points);
}

}  // namespace roo_display