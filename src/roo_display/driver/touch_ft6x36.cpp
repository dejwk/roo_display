#include "roo_display/driver/touch_ft6x36.h"

#include <Wire.h>

namespace roo_display {

namespace {

static constexpr uint8_t kTouchI2cAddr = 0x38;
static constexpr int kRegNumTouches = 2;

static constexpr int kRegBaseXh = 3;
static constexpr int kRegBaseXl = 4;
static constexpr int kRegBaseYh = 5;
static constexpr int kRegBaseYl = 6;
static constexpr int kRegBaseWeight = 7;
static constexpr int kRegBaseSize = 8;

static constexpr int kTouchBufferSize = 6;

}  // namespace

TouchFt6x36::TouchFt6x36() : TouchFt6x36(Wire) {}

// The panel reports the same, cached, coordinates if scanned more frequently
// than every 25ms. So let's not bother asking, just serve cached values.
// (This also allows velocity detection to work).
TouchFt6x36::TouchFt6x36(decltype(Wire)& wire)
    : BasicTouchDevice<2>(Config{.min_sampling_interval_ms = 25,
                                 .touch_intertia_ms = 0,
                                 .smoothing_factor = 0.0}),
      wire_(wire) {}

int TouchFt6x36::readTouch(TouchPoint* point) {
  static constexpr uint8_t size = 16;
  uint8_t data[size];
  wire_.beginTransmission(kTouchI2cAddr);
  wire_.write(0);
  if (wire_.endTransmission() != 0) return 0;
  if (wire_.requestFrom(kTouchI2cAddr, size) < size) return 0;
  for (uint8_t i = 0; i < size; i++) data[i] = wire_.read();
  TouchPoint* p = point;
  int touched = data[kRegNumTouches] & 0x0F;
  if (touched == 0 || touched > 2) return 0;
  int confirmed_touched = 0;
  for (uint8_t i = 0; i < 2; i++) {
    const int base = i * kTouchBufferSize;
    int kind = data[base + kRegBaseXh] >> 6;
    if ((kind & 1) != 0) {
      // Lift-up or no-event.
      continue;
    }
    ++confirmed_touched;
    p->id = data[base + kRegBaseYh] >> 4;
    p->x = ((data[base + kRegBaseXh] << 8) | data[base + kRegBaseXl]) & 0x0FFF;
    p->y = ((data[base + kRegBaseYh] << 8) | data[base + kRegBaseYl]) & 0x0FFF;
    p->z = data[base + kRegBaseWeight] << 4;  // 0-4080.
    if (p->z == 0) p->z = 1024;
    ++p;
  }
  return std::min(touched, confirmed_touched);
}

}  // namespace roo_display
