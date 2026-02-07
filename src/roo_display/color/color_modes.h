#pragma once

#include <inttypes.h>

#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_display/color/interpolation.h"

namespace roo_display {

/// ColorMode template contract.
///
/// A color mode `T` must provide:
/// ```
/// static const int8_t bits_per_pixel;
/// Color toArgbColor(storage_type in) const;
/// storage_type fromArgbColor(Color color) const;
/// TransparencyMode transparency() const;
/// ```
///
/// Meaning and expectations:
/// - `bits_per_pixel`: how many bits encode a single pixel in this mode.
/// - `toArgbColor(in)`: convert a raw pixel value to ARGB8888. If
///   `bits_per_pixel < 8`, the value is stored in the low-order bits of `in`.
/// - `fromArgbColor(color)`: convert ARGB8888 to a raw pixel value. If
///   `bits_per_pixel < 8`, the return value must be in the low-order bits.
/// - `transparency()`: indicates the alpha capabilities of the mode (opaque,
///   binary, or gradual). Used as a rendering optimization hint.
///
/// For optimized blending, consider specializing `RawBlender`.

/// 32-bit ARGB color mode.
class Argb8888 {
 public:
  static const int8_t bits_per_pixel = 32;

  inline constexpr Color toArgbColor(uint32_t in) const { return Color(in); }

  inline constexpr uint32_t fromArgbColor(Color color) const {
    return color.asArgb();
  }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_GRADUAL;
  }
};

/// 32-bit RGBA color mode.
class Rgba8888 {
 public:
  static const int8_t bits_per_pixel = 32;

  inline constexpr Color toArgbColor(uint32_t in) const {
    return Color(in >> 8 | (in & 0xFF) << 24);
  }

  inline constexpr uint32_t fromArgbColor(Color color) const {
    return color.asArgb() << 8 | color.asArgb() >> 24;
  }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_GRADUAL;
  }
};

namespace internal {

inline static constexpr uint32_t TruncTo4bit(uint8_t c) {
  return (c - (c >> 5)) >> 4;
}

inline static constexpr uint32_t TruncTo5bit(uint8_t c) {
  return (c - (c >> 6)) >> 3;
}

inline static constexpr uint32_t TruncTo6bit(uint8_t c) {
  return (c - (c >> 7)) >> 2;
}

}  // namespace internal

/// 24-bit RGB color mode (opaque).
class Rgb888 {
 public:
  static const int8_t bits_per_pixel = 24;

  inline constexpr Color toArgbColor(uint32_t in) const {
    return Color(in | 0xFF000000);
  }

  inline constexpr uint32_t fromArgbColor(Color color) const {
    return color.asArgb() & 0x00FFFFFF;
  }

  constexpr TransparencyMode transparency() const { return TRANSPARENCY_NONE; }
};

/// 24-bit ARGB 6-6-6-6 color mode.
class Argb6666 {
 public:
  static const int8_t bits_per_pixel = 24;

  inline constexpr Color toArgbColor(uint32_t in) const {
    // uint32_t a = ((in >> 16) & 0xFC) | (in >> 22);
    // uint32_t r = ((in >> 10) & 0xFC) | ((in >> 16) & 0x03);
    // uint32_t g = ((in >> 4) & 0xFC) | ((in >> 10) & 0x03);
    // uint32_t b = ((in << 2) & 0xFC) | ((in >> 4) & 0x03);
    // return Color(a(in) << 24 | r(in) << 16 | g(in) << 8 | b(in));
    return Color(((in >> 16) & 0xFC) | (in >> 22),
                 ((in >> 10) & 0xFC) | ((in >> 16) & 0x03),
                 ((in >> 4) & 0xFC) | ((in >> 10) & 0x03),
                 ((in << 2) & 0xFC) | ((in >> 4) & 0x03));
  }

  inline constexpr uint32_t fromArgbColor(Color color) const {
    // uint32_t argb = color.asArgb();
    // return ((color.asArgb() >> 8) & 0xFC0000LL) |
    //        ((color.asArgb() >> 6) & 0x03F000LL) |
    //        ((color.asArgb() >> 4) & 0x000FC0LL) |
    //        ((color.asArgb() >> 2) & 0x00003FLL);
    return internal::TruncTo6bit(color.a()) << 18 |
           internal::TruncTo6bit(color.r()) << 12 |
           internal::TruncTo6bit(color.g()) << 6 |
           internal::TruncTo6bit(color.b());
  }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_GRADUAL;
  }
};

/// 16-bit ARGB 4-4-4-4 color mode.
class Argb4444 {
 public:
  static const int8_t bits_per_pixel = 16;

  inline constexpr Color toArgbColor(uint16_t in) const {
    // uint32_t a = ((in >> 8) & 0xF0) | (in >> 12);
    // uint32_t r = ((in >> 4) & 0xF0) | ((in >> 8) & 0x0F);
    // uint32_t g = ((in >> 0) & 0xF0) | ((in >> 4) & 0x0F);
    // uint32_t b = ((in << 4) & 0xF0) | ((in >> 0) & 0x0F);
    return Color(((in >> 8) & 0xF0) | (in >> 12),
                 ((in >> 4) & 0xF0) | ((in >> 8) & 0x0F),
                 ((in >> 0) & 0xF0) | ((in >> 4) & 0x0F),
                 ((in << 4) & 0xF0) | ((in >> 0) & 0x0F));
  }

  inline constexpr uint16_t fromArgbColor(Color color) const {
    // uint32_t argb = color.asArgb();
    // return ((color.asArgb() >> 16) & 0xF000) |
    //        ((color.asArgb() >> 12) & 0x0F00) |
    //        ((color.asArgb() >> 8) & 0x00F0) | ((color.asArgb() >> 4) &
    //        0x000F);
    return internal::TruncTo4bit(color.a()) << 12 |
           internal::TruncTo4bit(color.r()) << 8 |
           internal::TruncTo4bit(color.g()) << 4 |
           internal::TruncTo4bit(color.b());
  }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_GRADUAL;
  }
};

/// 16-bit RGB565 color mode (opaque).
class Rgb565 {
 public:
  static const int8_t bits_per_pixel = 16;

  inline constexpr Color toArgbColor(uint16_t in) const
      __attribute__((always_inline)) {
    // uint32_t r = ((in >> 8) & 0xF8) | (in >> 13);
    // uint32_t g = ((in >> 3) & 0xFC) | ((in >> 9) & 0x03);
    // uint32_t b = ((in << 3) & 0xF8) | ((in >> 2) & 0x07);
    return Color(((in >> 8) & 0xF8) | (in >> 13),
                 ((in >> 3) & 0xFC) | ((in >> 9) & 0x03),
                 ((in << 3) & 0xF8) | ((in >> 2) & 0x07));
  }

  inline constexpr uint16_t fromArgbColor(Color color) const
      __attribute__((always_inline)) {
    // uint32_t argb = color.asArgb();
    // return ((color.asArgb() >> 8) & 0xF800) |
    //        ((color.asArgb() >> 5) & 0x07E0) |
    //        ((color.asArgb() >> 3) & 0x1F);
    return internal::TruncTo5bit(color.r()) << 11 |
           internal::TruncTo6bit(color.g()) << 5 |
           internal::TruncTo5bit(color.b());
  }

  constexpr TransparencyMode transparency() const { return TRANSPARENCY_NONE; }
};

template <>
struct RawBlender<Rgb565, BLENDING_MODE_SOURCE_OVER> {
  inline uint16_t operator()(uint16_t bg, Color color,
                             const Rgb565& mode) const {
    return mode.fromArgbColor(
        AlphaBlendOverOpaque(mode.toArgbColor(bg), color));
  }
};

// template <>
// struct RawColorInterpolator<Rgb565> {
//   inline Color operator()(uint16_t c1, uint16_t c2, uint16_t fraction,
//                           const Rgb565& mode) const {
//     return InterpolateOpaqueColors(mode.toArgbColor(c1),
//                                    mode.toArgbColor(c2), fraction);
//   }
// };

namespace internal {
inline constexpr uint16_t Resolve565Transparency(uint16_t c, uint16_t t) {
  return c == t ? c ^ 0x40 : c;
}
}  // namespace internal

/// RGB565 with a reserved value representing transparency.
class Rgb565WithTransparency {
 public:
  static const int8_t bits_per_pixel = 16;

  constexpr Rgb565WithTransparency(uint16_t transparent_background_rgb565)
      : transparency_(transparent_background_rgb565) {}

  inline constexpr Color toArgbColor(uint16_t in) const {
    // if (in == transparency_) return Color(0);
    // uint32_t r = ((in >> 8) & 0xF8) | (in >> 13);
    // uint32_t g = ((in >> 3) & 0xFC) | ((in >> 9) & 0x03);
    // uint32_t b = ((in << 3) & 0xF8) | ((in >> 2) & 0x07);
    // return Color(0xFF000000 | r << 16 | g << 8 | b);
    return in == transparency_ ? Color(0)
                               : Color(((in >> 8) & 0xF8) | (in >> 13),
                                       ((in >> 3) & 0xFC) | ((in >> 9) & 0x03),
                                       ((in << 3) & 0xF8) | ((in >> 2) & 0x07));
  }

  inline constexpr uint16_t fromArgbColor(Color color) const {
    return color.a() <= 127 ? transparency_
                            : internal::Resolve565Transparency(
                                  internal::TruncTo5bit(color.r()) << 11 |
                                      internal::TruncTo6bit(color.g()) << 5 |
                                      internal::TruncTo5bit(color.b()),
                                  transparency_);
  }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_BINARY;
  }

  uint16_t raw_transparency_color() const { return transparency_; }

 private:
  uint16_t transparency_;
};

template <>
struct RawBlender<Rgb565WithTransparency, BLENDING_MODE_SOURCE_OVER> {
  inline uint16_t operator()(uint16_t bg, Color color,
                             const Rgb565WithTransparency& mode) const {
    return mode.fromArgbColor(
        bg == mode.transparency()
            ? color
            : AlphaBlendOverOpaque(mode.toArgbColor(bg), color));
  }
};

// 256 shades of gray.
class Grayscale8 {
 public:
  static const int8_t bits_per_pixel = 8;

  inline constexpr Color toArgbColor(uint8_t in) const {
    return Color(0xFF000000 | in * 0x010101);
  }

  inline constexpr uint8_t fromArgbColor(Color color) const {
    // Using fast approximate formula;
    // See https://stackoverflow.com/questions/596216
    return (((int16_t)color.r() * 3) + ((int16_t)color.g() * 4) + color.b()) >>
           3;
  }

  constexpr TransparencyMode transparency() const { return TRANSPARENCY_NONE; }
};

template <>
struct RawBlender<Grayscale8, BLENDING_MODE_SOURCE_OVER> {
  inline uint8_t operator()(uint8_t bg, Color color,
                            const Grayscale8& mode) const {
    uint8_t raw = mode.fromArgbColor(color);
    uint16_t alpha = color.a();
    return internal::__div_255_rounded(alpha * raw + (255 - alpha) * bg);
  }
};

template <>
struct RawBlender<Grayscale8, BLENDING_MODE_SOURCE_OVER_OPAQUE> {
  inline uint8_t operator()(uint8_t bg, Color color,
                            const Grayscale8& mode) const {
    uint8_t raw = mode.fromArgbColor(color);
    uint16_t alpha = color.a();
    return internal::__div_255_rounded(alpha * raw + (255 - alpha) * bg);
  }
};

template <>
struct RawColorInterpolator<Grayscale8> {
  inline Color operator()(uint8_t c1, uint8_t c2, uint16_t fraction,
                          const Grayscale8& mode) const {
    return mode.toArgbColor(
        ((uint16_t)c1 * (256 - fraction) + (uint16_t)c2 * fraction) / 256);
  }
};

// 256 shades of gray + 8-bit alpha.
class GrayAlpha8 {
 public:
  static const int8_t bits_per_pixel = 16;

  inline constexpr Color toArgbColor(uint16_t in) const {
    return Color(in << 24 | (in >> 8) * 0x010101);
  }

  inline constexpr uint16_t fromArgbColor(Color color) const {
    // Using fast approximate formula;
    // See https://stackoverflow.com/questions/596216
    return color.a() >> 24 |
           ((((int16_t)color.r() * 3) + ((int16_t)color.g() * 4) + color.b()) >>
            3) << 8;
  }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_GRADUAL;
  }
};

// 16 shades of gray.
class Grayscale4 {
 public:
  static const int8_t bits_per_pixel = 4;

  inline constexpr Color toArgbColor(uint8_t in) const {
    return Color(0xFF000000 | in * 0x111111);
  }

  inline constexpr uint8_t fromArgbColor(Color color) const {
    // Using fast approximate formula;
    // See https://stackoverflow.com/questions/596216
    return ((((uint16_t)color.r()) * 3) + (((uint16_t)color.g()) * 4) +
            (uint16_t)color.b()) >>
           7;
  }

  constexpr TransparencyMode transparency() const { return TRANSPARENCY_NONE; }
};

template <>
struct RawBlender<Grayscale4, BLENDING_MODE_SOURCE_OVER> {
  inline uint8_t operator()(uint8_t bg, Color color,
                            const Grayscale4& mode) const {
    uint8_t raw = mode.fromArgbColor(color);
    uint16_t alpha = color.a();
    return internal::__div_255_rounded(alpha * raw + (255 - alpha) * bg);
  }
};

template <>
struct RawBlender<Grayscale4, BLENDING_MODE_SOURCE_OVER_OPAQUE> {
  inline uint8_t operator()(uint8_t bg, Color color,
                            const Grayscale4& mode) const {
    uint8_t raw = mode.fromArgbColor(color);
    uint16_t alpha = color.a();
    return internal::__div_255_rounded(alpha * raw + (255 - alpha) * bg);
  }
};

template <>
struct RawColorInterpolator<Grayscale4> {
  inline Color operator()(uint8_t c1, uint8_t c2, uint16_t fraction,
                          const Grayscale4& mode) const {
    return Grayscale8().toArgbColor(
        ((uint16_t)c1 * (256 - fraction) + (uint16_t)c2 * fraction) * 17 / 256);
  }
};

// Semi-transparent monochrome with 256 transparency levels.
class Alpha8 {
 public:
  constexpr Alpha8(Color color) : color_(color) {}

  static const int8_t bits_per_pixel = 8;

  inline constexpr Color toArgbColor(uint8_t in) const {
    return Color((in << 24) | (color_.asArgb() & 0x00FFFFFF));
  }

  inline constexpr uint8_t fromArgbColor(Color color) const {
    return color.a();
  }

  constexpr Color color() const { return color_; }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_GRADUAL;
  }

 private:
  Color color_;
};

template <>
struct RawBlender<Alpha8, BLENDING_MODE_SOURCE_OVER> {
  inline uint8_t operator()(uint8_t bg, Color color, const Alpha8& mode) const {
    uint16_t front_alpha = color.a();
    if (front_alpha == 0xFF || bg == 0xFF) return 0xFF;
    uint16_t tmp = bg * front_alpha;
    return bg + front_alpha - internal::__div_255_rounded(tmp);
  }
};

template <>
struct RawColorInterpolator<Alpha8> {
  inline Color operator()(uint8_t c1, uint8_t c2, uint16_t fraction,
                          const Alpha8& mode) const {
    return mode.toArgbColor(
        ((uint16_t)c1 * (256 - fraction) + (uint16_t)c2 * fraction) / 256);
  }
};

// Semi-transparent monochrome with 16 transparency levels. Good default
// for anti-aliased monochrome bitmaps.
class Alpha4 {
 public:
  constexpr Alpha4(Color color)
      : color_(0xFF000000 | (color.asArgb() & 0x00FFFFFF)) {}

  static const int8_t bits_per_pixel = 4;

  inline constexpr Color toArgbColor(uint8_t in) const {
    return Color(((in | in << 4) << 24) | (color_.asArgb() & 0x00FFFFFF));
  }

  inline constexpr uint8_t fromArgbColor(Color color) const {
    //    return color.a() >> 4;
    return internal::TruncTo4bit(color.a());
  }

  constexpr Color color() const { return color_; }
  void setColor(Color color) { color_ = color; }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_GRADUAL;
  }

 private:
  Color color_;
};

template <>
struct RawBlender<Alpha4, BLENDING_MODE_SOURCE_OVER> {
  inline uint8_t operator()(uint8_t bg, Color color, const Alpha4& mode) const {
    uint8_t front_alpha = color.a();
    if (front_alpha == 0xFF || bg == 0xF) return 0xF;
    bg |= (bg << 4);
    uint16_t tmp = bg * front_alpha;
    return internal::TruncTo4bit(bg + front_alpha -
                                 internal::__div_255_rounded(tmp));
  }
};

template <>
struct RawColorInterpolator<Alpha4> {
  inline Color operator()(uint8_t c1, uint8_t c2, uint16_t fraction,
                          const Alpha4& mode) const {
    return Alpha8(mode.color())
        .toArgbColor(
            ((uint16_t)c1 * (256 - fraction) + (uint16_t)c2 * fraction) * 17 /
            256);
  }
};

// Binary color, wich specified 'foreground' and 'background' values.
// Both can be (semi) transparent. Useful for bit masks.
class Monochrome {
 public:
  constexpr Monochrome(Color fg, Color bg = Color(0x00000000))
      : fg_(fg), bg_(bg) {}

  static const int8_t bits_per_pixel = 1;

  inline constexpr Color toArgbColor(uint8_t in) const {
    return in == 0 ? bg_ : fg_;
  }

  inline constexpr uint8_t fromArgbColor(Color color) const {
    return (color == bg_ || color.a() == 0) ? 0 : 1;
  }

  constexpr Color fg() const { return fg_; }
  void setFg(Color fg) { fg_ = fg; }

  constexpr Color bg() const { return bg_; }
  void setBg(Color bg) { bg_ = bg; }

  constexpr TransparencyMode transparency() const {
    return bg().a() == 0xFF ? (fg().a() == 0xFF   ? TRANSPARENCY_NONE
                               : fg().a() == 0x00 ? TRANSPARENCY_BINARY
                                                  : TRANSPARENCY_GRADUAL)
           : bg().a() == 0x00
               ? (fg().a() == 0x00 || fg().a() == 0xFF ? TRANSPARENCY_BINARY
                                                       : TRANSPARENCY_GRADUAL)
               : TRANSPARENCY_GRADUAL;
  }

  constexpr bool hasTransparency() const {
    return bg().a() != 0xFF || fg().a() != 0xFF;
  }

 private:
  Color fg_;
  Color bg_;
};

template <>
struct RawBlender<Monochrome, BLENDING_MODE_SOURCE_OVER> {
  inline uint8_t operator()(uint8_t bg, Color color,
                            const Monochrome& mode) const {
    return mode.fg().a() == 0 ? bg : mode.fromArgbColor(mode.fg());
  }
};

}  // namespace roo_display
