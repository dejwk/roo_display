#pragma once

#include "roo_display.h"

namespace roo_display {

class Backlit {
 public:
  static constexpr uint16_t kMaxIntensity = 256;
  static constexpr uint16_t kHalfIntensity = 128;

  virtual ~Backlit() = default;

  // Sets backlit intensity in the range [0..256].
  virtual void setIntensity(uint16_t intensity) = 0;
};

class NoBacklit : public Backlit {
 public:
  void setIntensity(uint16_t intensity) override {}
};

}  // namespace roo_display
