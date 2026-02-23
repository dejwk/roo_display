#pragma once

#include <inttypes.h>

#include "roo_backport/byte.h"
#include "roo_io.h"
#include "roo_io/memory/fill.h"

namespace roo_display {

/// ARGB8888 color stored as a 32-bit unsigned integer.
///
/// This is a lightweight, trivially-constructible type that can be passed by
/// value, with convenience accessors and type safety.
class Color {
 public:
  /// Construct transparent black.
  Color() : argb_(0) {}

  /// Construct opaque color from RGB.
  constexpr Color(uint8_t r, uint8_t g, uint8_t b)
      : argb_(0xFF000000LL | (r << 16) | (g << 8) | b) {}

  /// Construct ARGB color from components.
  constexpr Color(uint8_t a, uint8_t r, uint8_t g, uint8_t b)
      : argb_((a << 24) | (r << 16) | (g << 8) | b) {}

  /// Construct from a packed ARGB value.
  constexpr Color(uint32_t argb) : argb_(argb) {}

  /// Return packed ARGB value.
  constexpr uint32_t asArgb() const { return argb_; }

  /// Alpha channel.
  constexpr uint8_t a() const { return (argb_ >> 24) & 0xFF; }
  /// Red channel.
  constexpr uint8_t r() const { return (argb_ >> 16) & 0xFF; }
  /// Green channel.
  constexpr uint8_t g() const { return (argb_ >> 8) & 0xFF; }
  /// Blue channel.
  constexpr uint8_t b() const { return argb_ & 0xFF; }

  /// Set alpha channel.
  void set_a(uint8_t a) {
    argb_ &= 0x00FFFFFF;
    argb_ |= ((uint32_t)a) << 24;
  }

  /// Set red channel.
  void set_r(uint8_t r) {
    argb_ &= 0xFF00FFFF;
    argb_ |= ((uint32_t)r) << 16;
  }

  /// Set green channel.
  void set_g(uint8_t g) {
    argb_ &= 0xFFFF00FF;
    argb_ |= ((uint32_t)g) << 8;
  }

  /// Set blue channel.
  void set_b(uint8_t g) {
    argb_ &= 0xFFFFFF00;
    argb_ |= ((uint32_t)g);
  }

  /// Return a copy with the specified alpha channel.
  constexpr Color withA(uint8_t a) const {
    return Color(a << 24 | (argb_ & 0x00FFFFFF));
  }

  /// Return a copy with the specified red channel.
  constexpr Color withR(uint8_t r) const {
    return Color(r << 16 | (argb_ & 0xFF00FFFF));
  }

  /// Return a copy with the specified green channel.
  constexpr Color withG(uint8_t g) const {
    return Color(g << 8 | (argb_ & 0xFFFF00FF));
  }

  /// Return a copy with the specified blue channel.
  constexpr Color withB(uint8_t b) const {
    return Color(b << 0 | (argb_ & 0xFFFFFF00));
  }

  /// Return true if the color is fully opaque (alpha = 255).
  constexpr bool isOpaque() const { return a() == 0xFF; }

  /// Return a fully opaque copy (alpha = 255).
  constexpr Color toOpaque() { return Color(asArgb() | 0xFF000000); }

 private:
  uint32_t argb_;
};

/// Equality operator for colors.
inline constexpr bool operator==(const Color &a, const Color &b) {
  return a.asArgb() == b.asArgb();
}

/// Inequality operator for colors.
inline constexpr bool operator!=(const Color &a, const Color &b) {
  return a.asArgb() != b.asArgb();
}

/// Fill an array with a single color.
inline void FillColor(Color *buf, uint32_t count, Color color) {
  roo_io::PatternFill<sizeof(Color)>((roo::byte *)buf, count,
                                     (const roo::byte *)(&color));
}

template <typename ColorMode>
/// Truncate a color to a given color mode and back to ARGB.
inline constexpr Color TruncateColor(Color color,
                                     ColorMode mode = ColorMode()) {
  return mode.toArgbColor(mode.fromArgbColor(color));
}

/// Return an opaque gray with r = g = b = level.
inline constexpr Color Graylevel(uint8_t level) {
  return Color(level, level, level);
}

namespace color {

/// Transparent color (special behavior in fills).
///
/// When drawn in `kFillRectangle`, substituted by the current surface's
/// background color. When drawn in `kFillVisible`, pixels are not pushed
/// to the device, leaving previous content intact.
static constexpr auto Transparent = Color(0);

/// Background color placeholder.
///
/// Substituted by the current surface background regardless of fill mode.
/// Use this to force background pixels to be pushed even in
/// `kFillVisible`.
static constexpr auto Background = Color(0x00FFFFFF);

}  // namespace color

}  // namespace roo_display
