#pragma once

#include "roo_display.h"

namespace roo_display {

class Backlit {
 public:
  virtual ~Backlit() = default;
  // Sets backlit intensity in the range [0..255].
  virtual void setIntensity(uint8_t intensity) = 0;
};

class NoBacklit : public Backlit {
 public:
  void setIntensity(uint8_t intensity) override {}
};

}  // namespace
