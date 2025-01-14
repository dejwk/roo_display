#pragma once

#include <inttypes.h>

#include "roo_io/base/byte.h"
#include "roo_io/memory/fill.h"

namespace roo_display {

// Color is represented in ARGB8888, and stored as 32-bit unsigned integer. It
// is a lightweight trivially-constructible type; i.e. it can be treated as
// uint32 for all intents and purposes (e.g., passed by value), except that it
// has convenience methods and is type-safe.
class Color {
 public:
  Color() : argb_(0) {}

  constexpr Color(uint8_t r, uint8_t g, uint8_t b)
      : argb_(0xFF000000LL | (r << 16) | (g << 8) | b) {}

  constexpr Color(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
      : argb_((a << 24) | (r << 16) | (g << 8) | b) {}

  constexpr Color(uint32_t argb) : argb_(argb) {}

  constexpr uint32_t asArgb() const { return argb_; }

  constexpr uint8_t a() const { return (argb_ >> 24) & 0xFF; }
  constexpr uint8_t r() const { return (argb_ >> 16) & 0xFF; }
  constexpr uint8_t g() const { return (argb_ >> 8) & 0xFF; }
  constexpr uint8_t b() const { return argb_ & 0xFF; }

  void set_a(uint8_t a) {
    argb_ &= 0x00FFFFFF;
    argb_ |= ((uint32_t)a) << 24;
  }

  void set_r(uint8_t r) {
    argb_ &= 0xFF00FFFF;
    argb_ |= ((uint32_t)r) << 16;
  }

  void set_g(uint8_t g) {
    argb_ &= 0xFFFF00FF;
    argb_ |= ((uint32_t)g) << 8;
  }

  void set_b(uint8_t g) {
    argb_ &= 0xFFFFFF00;
    argb_ |= ((uint32_t)g);
  }

  // Returns a new color, which is identical to this color except it has
  // the specified alpha component.
  constexpr Color withA(uint8_t a) const {
    return Color(a << 24 | (argb_ & 0x00FFFFFF));
  }

  // Returns a new color, which is identical to this color except it has
  // the specified red component.
  constexpr Color withR(uint8_t r) const {
    return Color(r << 16 | (argb_ & 0xFF00FFFF));
  }

  // Returns a new color, which is identical to this color except it has
  // the specified green component.
  constexpr Color withG(uint8_t g) const {
    return Color(g << 8 | (argb_ & 0xFFFF00FF));
  }

  // Returns a new color, which is identical to this color except it has
  // the specified blue component.
  constexpr Color withB(uint8_t b) const {
    return Color(b << 0 | (argb_ & 0xFFFFFF00));
  }

  // Returns true if the color is fully opaque; i.e. if the alpha value = 255.
  constexpr bool isOpaque() const { return a() == 0xFF; }

  // Makes the color fully opaque by setting its alpha value to 255.
  constexpr Color toOpaque() { return Color(asArgb() | 0xFF000000); }

 private:
  uint32_t argb_;
};

inline constexpr bool operator==(const Color &a, const Color &b) {
  return a.asArgb() == b.asArgb();
}

inline constexpr bool operator!=(const Color &a, const Color &b) {
  return a.asArgb() != b.asArgb();
}

// Utility function to quickly fill an array with a single color.
inline void FillColor(Color *buf, uint32_t count, Color color) {
  roo_io::PatternFill<sizeof(Color)>((roo_io::byte *)buf, count,
                                     (const roo_io::byte *)(&color));
}

template <typename ColorMode>
inline constexpr Color TruncateColor(Color color,
                                     ColorMode mode = ColorMode()) {
  return mode.toArgbColor(mode.fromArgbColor(color));
}

// Convenience function to return an opaque gray with r = g = b = level.
inline constexpr Color Graylevel(uint8_t level) {
  return Color(level, level, level);
}

namespace color {

// When drawn in FILL_MODE_RECTANGLE, substituted by the current surface's
// background color. When drawn in FILL_MODE_VISIBLE, the pixels are not
// actually pushed to the underlying device, leaving the previous content
// intact.
static constexpr auto Transparent = Color(0);

// Substituted by the current surface's background color when drawn,
// regardless of the fill mode. Use it as a background color if you want
// the pixels actually pushed to the device, even in FILL_MODE_VISIBLE.
static constexpr auto Background = Color(0x00FFFFFF);

}  // namespace color

}  // namespace roo_display
