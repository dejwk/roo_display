#pragma once

#include <inttypes.h>

#include "roo_display/internal/hashtable.h"
#include "roo_display/internal/memfill.h"

namespace roo_display {

// When drawing using semi-transparent colors, specified if and how the
// previous content is combined with the new content.
enum PaintMode {
  // The new ARGB8888 value is alpha-blended over the old one. This is the
  // default paint mode.
  PAINT_MODE_BLEND,

  // The new ARGB8888 value completely replaces the old one.
  PAINT_MODE_REPLACE,
};

// Color is represented in ARGB8, and stored as 32-bit unsigned integer. It is a
// lightweight trivially-constructible type; i.e. it can be treated as uint32
// for all intents and purposes (e.g., passed by value), except that it has
// convenience methods and is type-safe.
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
  Color withA(uint8_t a) const {
    Color c = *this;
    c.set_a(a);
    return c;
  }

  // Returns a new color, which is identical to this color except it has
  // the specified red component.
  Color withR(uint8_t r) const {
    Color c = *this;
    c.set_r(r);
    return c;
  }

  // Returns a new color, which is identical to this color except it has
  // the specified green component.
  Color withG(uint8_t g) const {
    Color c = *this;
    c.set_g(g);
    return c;
  }

  // Returns a new color, which is identical to this color except it has
  // the specified blue component.
  Color withB(uint8_t b) const {
    Color c = *this;
    c.set_b(b);
    return c;
  }

  // Returns true if the color is fully opaque; i.e. if the alpha value = 255.
  constexpr bool opaque() const { return a() == 0xFF; }

  // Makes the color fully opaque by setting its alpha value to 255.
  constexpr Color toOpaque() { return Color(asArgb() | 0xFF000000); }

  // Utility function to quickly fill an array with a single color.
  static void Fill(Color *buf, uint32_t count, Color color) {
    pattern_fill<sizeof(Color)>(reinterpret_cast<uint8_t *>(buf), count,
                                reinterpret_cast<uint8_t *>(&color));
  }

 private:
  uint32_t argb_;
};

inline constexpr bool operator==(const Color &a, const Color &b) {
  return a.asArgb() == b.asArgb();
}

inline constexpr bool operator!=(const Color &a, const Color &b) {
  return a.asArgb() != b.asArgb();
}

// In practice, seems to be measurably faster than actually dividing by 255
// and counting on the compiler to optimize.
inline constexpr uint8_t __div_255(uint32_t arg) {
  return ((arg + 1) * 257) >> 16;
}

// Calculates alpha-blending of the foreground color (fgc) over the background
// color (bgc), ignoring background color's alpha, as if it is fully opaque.
inline Color AlphaBlendOverOpaque(Color bgc, Color fgc) {
  uint16_t alpha = fgc.a();
  uint16_t inv_alpha = alpha ^ 0xFF;
  uint8_t r = (uint8_t)(__div_255(alpha * fgc.r() + inv_alpha * bgc.r()));
  uint8_t g = (uint8_t)(__div_255(alpha * fgc.g() + inv_alpha * bgc.g()));
  uint8_t b = (uint8_t)(__div_255(alpha * fgc.b() + inv_alpha * bgc.b()));

  // https://stackoverflow.com/questions/12011081
  // This implementation is slightly faster (14ms vs 16ms in a microbenchmark)
  // than the implementation above, but it is one-off for some important
  // cases, like fg = 0x00000000 (transparency), and bg = 0xFFFFFFFF (white),
  // which should produce white but produces 0xFFFEFEFE.
  // uint16_t alpha = fgc.a() + 1;
  // uint16_t inv_alpha = 256 - alpha;
  // uint8_t r = (uint8_t)((alpha * fgc.r() + inv_alpha * bgc.r()) >> 8);
  // uint8_t g = (uint8_t)((alpha * fgc.g() + inv_alpha * bgc.g()) >> 8);
  // uint8_t b = (uint8_t)((alpha * fgc.b() + inv_alpha * bgc.b()) >> 8);
  return Color(0xFF000000 | r << 16 | g << 8 | b);
}

// Calculates alpha-blending of the foreground color (fgc) over the background
// color (bgc), in a general case, where bgc might be semi-transparent.
// If the background is (or should be treated as) opaque, use
// AlphaBlendOverOpaque() instead.
inline Color AlphaBlend(Color bgc, Color fgc) {
  uint16_t front_alpha = fgc.a();
  if (front_alpha == 0xFF) {
    return fgc;
  }
  uint16_t back_alpha = bgc.a();
  if (back_alpha == 0xFF) {
    return AlphaBlendOverOpaque(bgc, fgc);
  }
  if (front_alpha == 0) {
    return bgc;
  }
  if (back_alpha == 0) {
    return fgc;
  }
  // Blends a+b so that, when later applied over c, the result is the same as if
  // they were applied in succession; e.g. c+(a+b) == (c+a)+b.
  uint16_t tmp = back_alpha * front_alpha;
  uint16_t alpha = back_alpha + front_alpha - ((tmp + (tmp >> 8)) >> 8);
  uint16_t front_multi = ((front_alpha + 1) << 8) / (alpha + 1);
  uint16_t back_multi = 256 - front_multi;

  uint8_t r = (uint8_t)((front_multi * fgc.r() + back_multi * bgc.r()) >> 8);
  uint8_t g = (uint8_t)((front_multi * fgc.g() + back_multi * bgc.g()) >> 8);
  uint8_t b = (uint8_t)((front_multi * fgc.b() + back_multi * bgc.b()) >> 8);
  return Color(alpha << 24 | r << 16 | g << 8 | b);
}

inline Color CombineColors(Color bgc, Color fgc, PaintMode paint_mode) {
  switch (paint_mode) {
    case PAINT_MODE_REPLACE: {
      return fgc;
    }
    case PAINT_MODE_BLEND: {
      return AlphaBlend(bgc, fgc);
    }
    default: {
      return Color(0);
    }
  }
}

// Ratio is expected in the range of [0, 128], inclusively.
// The resulting color = c1 * (ratio/128) + c2 * (1 - ratio/128).
// Use ratio = 64 for the arithmetic average.
inline constexpr Color AverageColors(Color c1, Color c2, uint8_t ratio) {
  return Color(
      (((uint8_t)c1.a() * ratio + (uint8_t)c2.a() * (128 - ratio)) >> 7) << 24 |
      (((uint8_t)c1.r() * ratio + (uint8_t)c2.r() * (128 - ratio)) >> 7) << 16 |
      (((uint8_t)c1.g() * ratio + (uint8_t)c2.g() * (128 - ratio)) >> 7) << 8 |
      (((uint8_t)c1.b() * ratio + (uint8_t)c2.b() * (128 - ratio)) >> 7) << 0);
}

enum TransparencyMode {
  TRANSPARENCY_NONE,    // All colors are fully opaque.
  TRANSPARENCY_BINARY,  // Colors are either fully opaque or fully transparent.
  TRANSPARENCY_GRADUAL  // Colors may include partial transparency (alpha
                        // channel).
};

namespace internal {

struct ColorHash {
  uint32_t operator()(Color color) const { return color.asArgb(); }
};

typedef HashSet<Color, ColorHash> ColorSet;

}  // namespace internal

// ColorMode is the following template contract:
// class T {
//  public:
//   // How many bits are needed to represent a single pixel.
//   static const int8_t bits_per_pixel = ...;
//
//   // Converts raw value to ARGB8888 color. If bits_per_pixel < 8, the raw
//   // value will be passed in the low-order bits.
//   inline Color toArgbColor(storage_type in) const;
//
//   // Converts ARGB8888 color to a raw value. If bits_per_pixel < 8, the raw
//   // value is written to the low-order bits.
//   inline storage_type fromArgbColor(Color color) const;
//
//   // Returns the value that specifies what are possible values of alpha.
//   TransparencyMode transparency() const { return TRANSPARENCY_GRADUAL; }
// };

class Argb8888 {
 public:
  static const int8_t bits_per_pixel = 32;

  inline constexpr Color toArgbColor(uint32_t in) const { return Color(in); }

  inline constexpr uint32_t fromArgbColor(Color color) const {
    return color.asArgb();
  }

  inline uint32_t rawAlphaBlend(uint32_t bg, Color color) const {
    return AlphaBlend(Color(bg), color).asArgb();
  }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_GRADUAL;
  }
};

inline static constexpr uint32_t truncTo4bit(uint8_t c) {
  return (c - (c >> 5)) >> 4;
}

inline static constexpr uint32_t truncTo5bit(uint8_t c) {
  return (c - (c >> 6)) >> 3;
}

inline static constexpr uint32_t truncTo6bit(uint8_t c) {
  return (c - (c >> 7)) >> 2;
}

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
    return truncTo6bit(color.a()) << 18 | truncTo6bit(color.r()) << 12 |
           truncTo6bit(color.g()) << 6 | truncTo6bit(color.b());
  }

  inline uint32_t rawAlphaBlend(uint32_t bg, Color color) const {
    return fromArgbColor(AlphaBlend(toArgbColor(bg), color));
  }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_GRADUAL;
  }

 private:
};

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
    return truncTo4bit(color.a()) << 12 | truncTo4bit(color.r()) << 8 |
           truncTo4bit(color.g()) << 4 | truncTo4bit(color.b());
  }

  inline uint16_t rawAlphaBlend(uint16_t bg, Color color) const {
    return fromArgbColor(AlphaBlend(toArgbColor(bg), color));
  }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_GRADUAL;
  }
};

// The most common mode used by microcontrollers.
class Rgb565 {
 public:
  static const int8_t bits_per_pixel = 16;

  inline constexpr Color toArgbColor(uint16_t in) const {
    // uint32_t r = ((in >> 8) & 0xF8) | (in >> 13);
    // uint32_t g = ((in >> 3) & 0xFC) | ((in >> 9) & 0x03);
    // uint32_t b = ((in << 3) & 0xF8) | ((in >> 2) & 0x07);
    return Color(((in >> 8) & 0xF8) | (in >> 13),
                 ((in >> 3) & 0xFC) | ((in >> 9) & 0x03),
                 ((in << 3) & 0xF8) | ((in >> 2) & 0x07));
  }

  inline constexpr uint16_t fromArgbColor(Color color) const {
    // uint32_t argb = color.asArgb();
    // return ((color.asArgb() >> 8) & 0xF800) |
    //        ((color.asArgb() >> 5) & 0x07E0) |
    //        ((color.asArgb() >> 3) & 0x1F);
    return truncTo5bit(color.r()) << 11 | truncTo6bit(color.g()) << 5 |
           truncTo5bit(color.b());
  }

  inline uint16_t rawAlphaBlend(uint16_t bg, Color color) const {
    return fromArgbColor(AlphaBlendOverOpaque(toArgbColor(bg), color));
  }

  constexpr TransparencyMode transparency() const { return TRANSPARENCY_NONE; }
};

// Variant of Rgb565 that reserves one 16-bit value to represent transparent
// color.
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
    // if (color.a() <= 127) {
    //   return transparency_;
    // }
    // uint32_t argb = color.asArgb();
    // return ((argb >> 8) & 0xF800) | ((argb >> 5) & 0x07E0) |
    //        ((argb >> 3) & 0x1F);
    return color.a() <= 127
               ? transparency_
               : truncTo5bit(color.r()) << 11 | truncTo6bit(color.g()) << 5 |
                     truncTo5bit(color.b());
  }

  inline uint16_t rawAlphaBlend(uint16_t bg, Color color) const {
    return fromArgbColor(bg == transparency_
                             ? color
                             : AlphaBlendOverOpaque(toArgbColor(bg), color));
  }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_BINARY;
  }

 private:
  uint16_t transparency_;
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

  inline uint8_t rawAlphaBlend(uint8_t bg, Color fg) const {
    uint8_t raw = fromArgbColor(fg);
    uint16_t alpha = fg.a() + 1;
    return (alpha * raw + (256 - alpha) * bg) >> 8;
  }

  constexpr TransparencyMode transparency() const { return TRANSPARENCY_NONE; }
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

  inline uint8_t rawAlphaBlend(uint8_t bg, Color fg) const {
    uint8_t raw = fromArgbColor(fg);
    uint16_t alpha = fg.a() + 1;
    return (alpha * raw + (256 - alpha) * bg) >> 8;
  }

  constexpr TransparencyMode transparency() const { return TRANSPARENCY_NONE; }
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

  inline uint8_t rawAlphaBlend(uint8_t bg, Color fg) const {
    uint16_t front_alpha = fg.a();
    if (front_alpha == 0xFF || bg == 0xFF) return 0xFF;
    uint16_t tmp = bg * front_alpha;
    return bg + front_alpha - ((tmp + (tmp >> 8)) >> 8);
  }

  constexpr Color color() const { return color_; }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_GRADUAL;
  }

 private:
  Color color_;
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
    return truncTo4bit(color.a());
  }

  inline uint8_t rawAlphaBlend(uint8_t bg, Color fg) const {
    uint8_t front_alpha = fg.a();
    if (front_alpha == 0xFF || bg == 0xF) return 0xF;
    bg |= (bg << 4);
    uint16_t tmp = bg * front_alpha;
    return truncTo4bit(bg + front_alpha - ((tmp + (tmp >> 8)) >> 8));
  }

  constexpr Color color() const { return color_; }
  void setColor(Color color) { color_ = color; }

  constexpr TransparencyMode transparency() const {
    return TRANSPARENCY_GRADUAL;
  }

 private:
  Color color_;
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

  inline uint8_t rawAlphaBlend(uint8_t bg, Color fg) const {
    return fg.a() == 0 ? bg : fromArgbColor(fg);
  }

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

template <typename ColorMode, int8_t bits_per_pixel = ColorMode::bits_per_pixel>
struct ColorTraits;

template <typename ColorMode>
struct ColorTraits<ColorMode, 1> {
  typedef uint8_t storage_type;
  static const int8_t bytes_per_pixel = 1;
  static const int8_t pixels_per_byte = 8;
};

template <typename ColorMode>
struct ColorTraits<ColorMode, 2> {
  typedef uint8_t storage_type;
  static const int8_t bytes_per_pixel = 1;
  static const int8_t pixels_per_byte = 4;
};

template <typename ColorMode>
struct ColorTraits<ColorMode, 4> {
  typedef uint8_t storage_type;
  static const int8_t bytes_per_pixel = 1;
  static const int8_t pixels_per_byte = 2;
};

template <typename ColorMode>
struct ColorTraits<ColorMode, 8> {
  typedef uint8_t storage_type;
  static const int8_t bytes_per_pixel = 1;
  static const int8_t pixels_per_byte = 1;
};

template <typename ColorMode>
struct ColorTraits<ColorMode, 16> {
  typedef uint16_t storage_type;
  static const int8_t bytes_per_pixel = 2;
  static const int8_t pixels_per_byte = 1;
};

template <typename ColorMode>
struct ColorTraits<ColorMode, 24> {
  typedef uint32_t storage_type;
  static const int8_t bytes_per_pixel = 3;
  static const int8_t pixels_per_byte = 1;
};

template <typename ColorMode>
struct ColorTraits<ColorMode, 32> {
  typedef uint32_t storage_type;
  static const int8_t bytes_per_pixel = 4;
  static const int8_t pixels_per_byte = 1;
};

template <typename ColorMode>
using ColorStorageType = typename ColorTraits<ColorMode>::storage_type;

template <typename ColorMode>
static constexpr Color TruncateColor(Color color,
                                     ColorMode mode = ColorMode()) {
  return mode.toArgbColor(mode.fromArgbColor(color));
}

// Convenience function to return an opaque gray with r = g = b = level.
static constexpr Color Graylevel(uint8_t level) {
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

// Pink colors

static constexpr auto Pink = Color(0xFF, 0xC0, 0xCB);
static constexpr auto LightPink = Color(0xFF, 0xB6, 0xC1);
static constexpr auto HotPink = Color(0xFF, 0x69, 0xB4);
static constexpr auto DeepPink = Color(0xFF, 0x14, 0x93);
static constexpr auto PaleVioletRed = Color(0xDB, 0x70, 0x93);
static constexpr auto MediumVioletRed = Color(0xC7, 0x15, 0x85);

// Red colors

static constexpr auto LightSalmon = Color(0xFF, 0xA0, 0x7A);
static constexpr auto Salmon = Color(0xFA, 0x80, 0x72);
static constexpr auto DarkSalmon = Color(0xE9, 0x96, 0x7A);
static constexpr auto LightCoral = Color(0xF0, 0x80, 0x80);
static constexpr auto IndianRed = Color(0xCD, 0x5C, 0x5C);
static constexpr auto Crimson = Color(0xDC, 0x14, 0x3C);
static constexpr auto Firebrick = Color(0xB2, 0x22, 0x22);
static constexpr auto DarkRed = Color(0x8B, 0x00, 0x00);
static constexpr auto Red = Color(0xFF, 0x00, 0x00);

// Orange colors

static constexpr auto OrangeRed = Color(0xFF, 0x45, 0x00);
static constexpr auto Tomato = Color(0xFF, 0x63, 0x47);
static constexpr auto Coral = Color(0xFF, 0x7F, 0x50);
static constexpr auto DarkOrange = Color(0xFF, 0x8C, 0x00);
static constexpr auto Orange = Color(0xFF, 0xA5, 0x00);

// Yellow colors

static constexpr auto Yellow = Color(0xFF, 0xFF, 0x00);
static constexpr auto LightYellow = Color(0xFF, 0xFF, 0xE0);
static constexpr auto LemonChiffon = Color(0xFF, 0xFA, 0xCD);
static constexpr auto LightGoldenrodYellow = Color(0xFA, 0xFA, 0xD2);
static constexpr auto PapayaWhip = Color(0xFF, 0xEF, 0xD5);
static constexpr auto Moccasin = Color(0xFF, 0xE4, 0xB5);
static constexpr auto PeachPuff = Color(0xFF, 0xDA, 0xB9);
static constexpr auto PaleGoldenrod = Color(0xEE, 0xE8, 0xAA);
static constexpr auto Khaki = Color(0xF0, 0xE6, 0x8C);
static constexpr auto DarkKhaki = Color(0xBD, 0xB7, 0x6B);
static constexpr auto Gold = Color(0xFF, 0xD7, 0x00);

// Brown colors

static constexpr auto Cornsilk = Color(0xFF, 0xF8, 0xDC);
static constexpr auto BlanchedAlmond = Color(0xFF, 0xEB, 0xCD);
static constexpr auto Bisque = Color(0xFF, 0xE4, 0xC4);
static constexpr auto NavajoWhite = Color(0xFF, 0xDE, 0xAD);
static constexpr auto Wheat = Color(0xF5, 0xDE, 0xB3);
static constexpr auto Burlywood = Color(0xDE, 0xB8, 0x87);
static constexpr auto Tan = Color(0xD2, 0xB4, 0x8C);
static constexpr auto RosyBrown = Color(0xBC, 0x8F, 0x8F);
static constexpr auto SandyBrown = Color(0xF4, 0xA4, 0x60);
static constexpr auto Goldenrod = Color(0xDA, 0xA5, 0x20);
static constexpr auto DarkGoldenrod = Color(0xB8, 0x86, 0x0B);
static constexpr auto Peru = Color(0xCD, 0x85, 0x3F);
static constexpr auto Chocolate = Color(0xD2, 0x69, 0x1E);
static constexpr auto SaddleBrown = Color(0x8B, 0x45, 0x13);
static constexpr auto Sienna = Color(0xA0, 0x52, 0x2D);
static constexpr auto Brown = Color(0xA5, 0x2A, 0x2A);
static constexpr auto Maroon = Color(0x80, 0x00, 0x00);

// Green colors
static constexpr auto DarkOliveGreen = Color(0x55, 0x6B, 0x2F);
static constexpr auto Olive = Color(0x80, 0x80, 0x00);
static constexpr auto OliveDrab = Color(0x6B, 0x8E, 0x23);
static constexpr auto YellowGreen = Color(0x9A, 0xCD, 0x32);
static constexpr auto LimeGreen = Color(0x32, 0xCD, 0x32);
static constexpr auto Lime = Color(0x00, 0xFF, 0x00);
static constexpr auto LawnGreen = Color(0x7C, 0xFC, 0x00);
static constexpr auto Chartreuse = Color(0x7F, 0xFF, 0x00);
static constexpr auto GreenYellow = Color(0xAD, 0xFF, 0x2F);
static constexpr auto SpringGreen = Color(0x00, 0xFF, 0x7F);
static constexpr auto MediumSpringGreen = Color(0x00, 0xFA, 0x9A);
static constexpr auto LightGreen = Color(0x90, 0xEE, 0x90);
static constexpr auto PaleGreen = Color(0x98, 0xFB, 0x98);
static constexpr auto DarkSeaGreen = Color(0x8F, 0xBC, 0x8F);
static constexpr auto MediumAquamarine = Color(0x66, 0xCD, 0xAA);
static constexpr auto MediumSeaGreen = Color(0x3C, 0xB3, 0x71);
static constexpr auto SeaGreen = Color(0x2E, 0x8B, 0x57);
static constexpr auto ForestGreen = Color(0x22, 0x8B, 0x22);
static constexpr auto Green = Color(0x00, 0x80, 0x00);
static constexpr auto DarkGreen = Color(0x00, 0x64, 0x00);

// Cyan colors

static constexpr auto Aqua = Color(0x00, 0xFF, 0xFF);
static constexpr auto Cyan = Color(0x00, 0xFF, 0xFF);
static constexpr auto LightCyan = Color(0xE0, 0xFF, 0xFF);
static constexpr auto PaleTurquoise = Color(0xAF, 0xEE, 0xEE);
static constexpr auto Aquamarine = Color(0x7F, 0xFF, 0xD4);
static constexpr auto Turquoise = Color(0x40, 0xE0, 0xD0);
static constexpr auto MediumTurquoise = Color(0x48, 0xD1, 0xCC);
static constexpr auto DarkTurquoise = Color(0x00, 0xCE, 0xD1);
static constexpr auto LightSeaGreen = Color(0x20, 0xB2, 0xAA);
static constexpr auto CadetBlue = Color(0x5F, 0x9E, 0xA0);
static constexpr auto DarkCyan = Color(0x00, 0x8B, 0x8B);
static constexpr auto Teal = Color(0x00, 0x80, 0x80);

// Blue colors

static constexpr auto LightSteelBlue = Color(0xB0, 0xC4, 0xDE);
static constexpr auto PowderBlue = Color(0xB0, 0xE0, 0xE6);
static constexpr auto LightBlue = Color(0xAD, 0xD8, 0xE6);
static constexpr auto SkyBlue = Color(0x87, 0xCE, 0xEB);
static constexpr auto LightSkyBlue = Color(0x87, 0xCE, 0xFA);
static constexpr auto DeepSkyBlue = Color(0x00, 0xBF, 0xFF);
static constexpr auto DodgerBlue = Color(0x1E, 0x90, 0xFF);
static constexpr auto CornflowerBlue = Color(0x64, 0x95, 0xED);
static constexpr auto SteelBlue = Color(0x46, 0x82, 0xB4);
static constexpr auto RoyalBlue = Color(0x41, 0x69, 0xE1);
static constexpr auto Blue = Color(0x00, 0x00, 0xFF);
static constexpr auto MediumBlue = Color(0x00, 0x00, 0xCD);
static constexpr auto DarkBlue = Color(0x00, 0x00, 0x8B);
static constexpr auto Navy = Color(0x00, 0x00, 0x80);
static constexpr auto MidnightBlue = Color(0x19, 0x19, 0x70);

// Purple, violet, and magenta colors

static constexpr auto Lavender = Color(0xE6, 0xE6, 0xFA);
static constexpr auto Thistle = Color(0xD8, 0xBF, 0xD8);
static constexpr auto Plum = Color(0xDD, 0xA0, 0xDD);
static constexpr auto Violet = Color(0xEE, 0x82, 0xEE);
static constexpr auto Orchid = Color(0xDA, 0x70, 0xD6);
static constexpr auto Fuchsia = Color(0xFF, 0x00, 0xFF);
static constexpr auto Magenta = Color(0xFF, 0x00, 0xFF);
static constexpr auto MediumOrchid = Color(0xBA, 0x55, 0xD3);
static constexpr auto MediumPurple = Color(0x93, 0x70, 0xDB);
static constexpr auto BlueViolet = Color(0x8A, 0x2B, 0xE2);
static constexpr auto DarkViolet = Color(0x94, 0x00, 0xD3);
static constexpr auto DarkOrchid = Color(0x99, 0x32, 0xCC);
static constexpr auto DarkMagenta = Color(0x8B, 0x00, 0x8B);
static constexpr auto Purple = Color(0x80, 0x00, 0x80);
static constexpr auto Indigo = Color(0x4B, 0x00, 0x82);
static constexpr auto DarkSlateBlue = Color(0x48, 0x3D, 0x8B);
static constexpr auto SlateBlue = Color(0x6A, 0x5A, 0xCD);
static constexpr auto MediumSlateBlue = Color(0x7B, 0x68, 0xEE);

// White colors

static constexpr auto White = Color(0xFF, 0xFF, 0xFF);
static constexpr auto Snow = Color(0xFF, 0xFA, 0xFA);
static constexpr auto Honeydew = Color(0xF0, 0xFF, 0xF0);
static constexpr auto MintCream = Color(0xF5, 0xFF, 0xFA);
static constexpr auto Azure = Color(0xF0, 0xFF, 0xFF);
static constexpr auto AliceBlue = Color(0xF0, 0xF8, 0xFF);
static constexpr auto GhostWhite = Color(0xF8, 0xF8, 0xFF);
static constexpr auto WhiteSmoke = Color(0xF5, 0xF5, 0xF5);
static constexpr auto Seashell = Color(0xFF, 0xF5, 0xEE);
static constexpr auto Beige = Color(0xF5, 0xF5, 0xDC);
static constexpr auto OldLace = Color(0xFD, 0xF5, 0xE6);
static constexpr auto FloralWhite = Color(0xFF, 0xFA, 0xF0);
static constexpr auto Ivory = Color(0xFF, 0xFF, 0xF0);
static constexpr auto AntiqueWhite = Color(0xFA, 0xEB, 0xD7);
static constexpr auto Linen = Color(0xFA, 0xF0, 0xE6);
static constexpr auto LavenderBlush = Color(0xFF, 0xF0, 0xF5);
static constexpr auto MistyRose = Color(0xFF, 0xE4, 0xE1);

// Gray and black colors

static constexpr auto Gainsboro = Color(0xDC, 0xDC, 0xDC);
static constexpr auto LightGray = Color(0xD3, 0xD3, 0xD3);
static constexpr auto Silver = Color(0xC0, 0xC0, 0xC0);
static constexpr auto DarkGray = Color(0xA9, 0xA9, 0xA9);
static constexpr auto Gray = Color(0x80, 0x80, 0x80);
static constexpr auto DimGray = Color(0x69, 0x69, 0x69);
static constexpr auto LightSlateGray = Color(0x77, 0x88, 0x99);
static constexpr auto SlateGray = Color(0x70, 0x80, 0x90);
static constexpr auto DarkSlateGray = Color(0x2F, 0x4F, 0x4F);
static constexpr auto Black = Color(0x00, 0x00, 0x00);

// Standard colors truncated to RGB565

namespace rgb565 {

// Pink colors

static constexpr auto Pink = TruncateColor<Rgb565>(color::Pink);
static constexpr auto LightPink = TruncateColor<Rgb565>(color::LightPink);
static constexpr auto HotPink = TruncateColor<Rgb565>(color::HotPink);
static constexpr auto DeepPink = TruncateColor<Rgb565>(color::DeepPink);
static constexpr auto PaleVioletRed =
    TruncateColor<Rgb565>(color::PaleVioletRed);
static constexpr auto MediumVioletRed =
    TruncateColor<Rgb565>(color::MediumVioletRed);

// Red colors

static constexpr auto LightSalmon = TruncateColor<Rgb565>(color::LightSalmon);
static constexpr auto Salmon = TruncateColor<Rgb565>(color::Salmon);
static constexpr auto DarkSalmon = TruncateColor<Rgb565>(color::DarkSalmon);
static constexpr auto LightCoral = TruncateColor<Rgb565>(color::LightCoral);
static constexpr auto IndianRed = TruncateColor<Rgb565>(color::IndianRed);
static constexpr auto Crimson = TruncateColor<Rgb565>(color::Crimson);
static constexpr auto Firebrick = TruncateColor<Rgb565>(color::Firebrick);
static constexpr auto DarkRed = TruncateColor<Rgb565>(color::DarkRed);
static constexpr auto Red = TruncateColor<Rgb565>(color::Red);

// Orange colors

static constexpr auto OrangeRed = TruncateColor<Rgb565>(color::OrangeRed);
static constexpr auto Tomato = TruncateColor<Rgb565>(color::Tomato);
static constexpr auto Coral = TruncateColor<Rgb565>(color::Coral);
static constexpr auto DarkOrange = TruncateColor<Rgb565>(color::DarkOrange);
static constexpr auto Orange = TruncateColor<Rgb565>(color::Orange);

// Yellow colors

static constexpr auto Yellow = TruncateColor<Rgb565>(color::Yellow);
static constexpr auto LightYellow = TruncateColor<Rgb565>(color::LightYellow);
static constexpr auto LemonChiffon = TruncateColor<Rgb565>(color::LemonChiffon);
static constexpr auto LightGoldenrodYellow =
    TruncateColor<Rgb565>(color::LightGoldenrodYellow);
static constexpr auto PapayaWhip = TruncateColor<Rgb565>(color::PapayaWhip);
static constexpr auto Moccasin = TruncateColor<Rgb565>(color::Moccasin);
static constexpr auto PeachPuff = TruncateColor<Rgb565>(color::PeachPuff);
static constexpr auto PaleGoldenrod =
    TruncateColor<Rgb565>(color::PaleGoldenrod);
static constexpr auto Khaki = TruncateColor<Rgb565>(color::Khaki);
static constexpr auto DarkKhaki = TruncateColor<Rgb565>(color::DarkKhaki);
static constexpr auto Gold = TruncateColor<Rgb565>(color::Gold);

// Brown colors

static constexpr auto Cornsilk = TruncateColor<Rgb565>(color::Cornsilk);
static constexpr auto BlanchedAlmond =
    TruncateColor<Rgb565>(color::BlanchedAlmond);
static constexpr auto Bisque = TruncateColor<Rgb565>(color::Bisque);
static constexpr auto NavajoWhite = TruncateColor<Rgb565>(color::NavajoWhite);
static constexpr auto Wheat = TruncateColor<Rgb565>(color::Wheat);
static constexpr auto Burlywood = TruncateColor<Rgb565>(color::Burlywood);
static constexpr auto Tan = TruncateColor<Rgb565>(color::Tan);
static constexpr auto RosyBrown = TruncateColor<Rgb565>(color::RosyBrown);
static constexpr auto SandyBrown = TruncateColor<Rgb565>(color::SandyBrown);
static constexpr auto Goldenrod = TruncateColor<Rgb565>(color::Goldenrod);
static constexpr auto DarkGoldenrod =
    TruncateColor<Rgb565>(color::DarkGoldenrod);
static constexpr auto Peru = TruncateColor<Rgb565>(color::Peru);
static constexpr auto Chocolate = TruncateColor<Rgb565>(color::Chocolate);
static constexpr auto SaddleBrown = TruncateColor<Rgb565>(color::SaddleBrown);
static constexpr auto Sienna = TruncateColor<Rgb565>(color::Sienna);
static constexpr auto Brown = TruncateColor<Rgb565>(color::Brown);
static constexpr auto Maroon = TruncateColor<Rgb565>(color::Maroon);

// Green colors
static constexpr auto DarkOliveGreen =
    TruncateColor<Rgb565>(color::DarkOliveGreen);
static constexpr auto Olive = TruncateColor<Rgb565>(color::Olive);
static constexpr auto OliveDrab = TruncateColor<Rgb565>(color::OliveDrab);
static constexpr auto YellowGreen = TruncateColor<Rgb565>(color::YellowGreen);
static constexpr auto LimeGreen = TruncateColor<Rgb565>(color::LimeGreen);
static constexpr auto Lime = TruncateColor<Rgb565>(color::Lime);
static constexpr auto LawnGreen = TruncateColor<Rgb565>(color::LawnGreen);
static constexpr auto Chartreuse = TruncateColor<Rgb565>(color::Chartreuse);
static constexpr auto GreenYellow = TruncateColor<Rgb565>(color::GreenYellow);
static constexpr auto SpringGreen = TruncateColor<Rgb565>(color::SpringGreen);
static constexpr auto MediumSpringGreen =
    TruncateColor<Rgb565>(color::MediumSpringGreen);
static constexpr auto LightGreen = TruncateColor<Rgb565>(color::LightGreen);
static constexpr auto PaleGreen = TruncateColor<Rgb565>(color::PaleGreen);
static constexpr auto DarkSeaGreen = TruncateColor<Rgb565>(color::DarkSeaGreen);
static constexpr auto MediumAquamarine =
    TruncateColor<Rgb565>(color::MediumAquamarine);
static constexpr auto MediumSeaGreen =
    TruncateColor<Rgb565>(color::MediumSeaGreen);
static constexpr auto SeaGreen = TruncateColor<Rgb565>(color::SeaGreen);
static constexpr auto ForestGreen = TruncateColor<Rgb565>(color::ForestGreen);
static constexpr auto Green = TruncateColor<Rgb565>(color::Green);
static constexpr auto DarkGreen = TruncateColor<Rgb565>(color::DarkGreen);

// Cyan colors

static constexpr auto Aqua = TruncateColor<Rgb565>(color::Aqua);
static constexpr auto Cyan = TruncateColor<Rgb565>(color::Cyan);
static constexpr auto LightCyan = TruncateColor<Rgb565>(color::LightCyan);
static constexpr auto PaleTurquoise =
    TruncateColor<Rgb565>(color::PaleTurquoise);
static constexpr auto Aquamarine = TruncateColor<Rgb565>(color::Aquamarine);
static constexpr auto Turquoise = TruncateColor<Rgb565>(color::Turquoise);
static constexpr auto MediumTurquoise =
    TruncateColor<Rgb565>(color::MediumTurquoise);
static constexpr auto DarkTurquoise =
    TruncateColor<Rgb565>(color::DarkTurquoise);
static constexpr auto LightSeaGreen =
    TruncateColor<Rgb565>(color::LightSeaGreen);
static constexpr auto CadetBlue = TruncateColor<Rgb565>(color::CadetBlue);
static constexpr auto DarkCyan = TruncateColor<Rgb565>(color::DarkCyan);
static constexpr auto Teal = TruncateColor<Rgb565>(color::Teal);

// Blue colors

static constexpr auto LightSteelBlue =
    TruncateColor<Rgb565>(color::LightSteelBlue);
static constexpr auto PowderBlue = TruncateColor<Rgb565>(color::PowderBlue);
static constexpr auto LightBlue = TruncateColor<Rgb565>(color::LightBlue);
static constexpr auto SkyBlue = TruncateColor<Rgb565>(color::SkyBlue);
static constexpr auto LightSkyBlue = TruncateColor<Rgb565>(color::LightSkyBlue);
static constexpr auto DeepSkyBlue = TruncateColor<Rgb565>(color::DeepSkyBlue);
static constexpr auto DodgerBlue = TruncateColor<Rgb565>(color::DodgerBlue);
static constexpr auto CornflowerBlue =
    TruncateColor<Rgb565>(color::CornflowerBlue);
static constexpr auto SteelBlue = TruncateColor<Rgb565>(color::SteelBlue);
static constexpr auto RoyalBlue = TruncateColor<Rgb565>(color::RoyalBlue);
static constexpr auto Blue = TruncateColor<Rgb565>(color::Blue);
static constexpr auto MediumBlue = TruncateColor<Rgb565>(color::MediumBlue);
static constexpr auto DarkBlue = TruncateColor<Rgb565>(color::DarkBlue);
static constexpr auto Navy = TruncateColor<Rgb565>(color::Navy);
static constexpr auto MidnightBlue = TruncateColor<Rgb565>(color::MidnightBlue);

// Purple, violet, and magenta colors

static constexpr auto Lavender = TruncateColor<Rgb565>(color::Lavender);
static constexpr auto Thistle = TruncateColor<Rgb565>(color::Thistle);
static constexpr auto Plum = TruncateColor<Rgb565>(color::Plum);
static constexpr auto Violet = TruncateColor<Rgb565>(color::Violet);
static constexpr auto Orchid = TruncateColor<Rgb565>(color::Orchid);
static constexpr auto Fuchsia = TruncateColor<Rgb565>(color::Fuchsia);
static constexpr auto Magenta = TruncateColor<Rgb565>(color::Magenta);
static constexpr auto MediumOrchid = TruncateColor<Rgb565>(color::MediumOrchid);
static constexpr auto MediumPurple = TruncateColor<Rgb565>(color::MediumPurple);
static constexpr auto BlueViolet = TruncateColor<Rgb565>(color::BlueViolet);
static constexpr auto DarkViolet = TruncateColor<Rgb565>(color::DarkViolet);
static constexpr auto DarkOrchid = TruncateColor<Rgb565>(color::DarkOrchid);
static constexpr auto DarkMagenta = TruncateColor<Rgb565>(color::DarkMagenta);
static constexpr auto Purple = TruncateColor<Rgb565>(color::Purple);
static constexpr auto Indigo = TruncateColor<Rgb565>(color::Indigo);
static constexpr auto DarkSlateBlue =
    TruncateColor<Rgb565>(color::DarkSlateBlue);
static constexpr auto SlateBlue = TruncateColor<Rgb565>(color::SlateBlue);
static constexpr auto MediumSlateBlue =
    TruncateColor<Rgb565>(color::MediumSlateBlue);

// White colors

static constexpr auto White = TruncateColor<Rgb565>(color::White);
static constexpr auto Snow = TruncateColor<Rgb565>(color::Snow);
static constexpr auto Honeydew = TruncateColor<Rgb565>(color::Honeydew);
static constexpr auto MintCream = TruncateColor<Rgb565>(color::MintCream);
static constexpr auto Azure = TruncateColor<Rgb565>(color::Azure);
static constexpr auto AliceBlue = TruncateColor<Rgb565>(color::AliceBlue);
static constexpr auto GhostWhite = TruncateColor<Rgb565>(color::GhostWhite);
static constexpr auto WhiteSmoke = TruncateColor<Rgb565>(color::WhiteSmoke);
static constexpr auto Seashell = TruncateColor<Rgb565>(color::Seashell);
static constexpr auto Beige = TruncateColor<Rgb565>(color::Beige);
static constexpr auto OldLace = TruncateColor<Rgb565>(color::OldLace);
static constexpr auto FloralWhite = TruncateColor<Rgb565>(color::FloralWhite);
static constexpr auto Ivory = TruncateColor<Rgb565>(color::Ivory);
static constexpr auto AntiqueWhite = TruncateColor<Rgb565>(color::AntiqueWhite);
static constexpr auto Linen = TruncateColor<Rgb565>(color::Linen);
static constexpr auto LavenderBlush =
    TruncateColor<Rgb565>(color::LavenderBlush);
static constexpr auto MistyRose = TruncateColor<Rgb565>(color::MistyRose);

// Gray and black colors

static constexpr auto Gainsboro = TruncateColor<Rgb565>(color::Gainsboro);
static constexpr auto LightGray = TruncateColor<Rgb565>(color::LightGray);
static constexpr auto Silver = TruncateColor<Rgb565>(color::Silver);
static constexpr auto DarkGray = TruncateColor<Rgb565>(color::DarkGray);
static constexpr auto Gray = TruncateColor<Rgb565>(color::Gray);
static constexpr auto DimGray = TruncateColor<Rgb565>(color::DimGray);
static constexpr auto LightSlateGray =
    TruncateColor<Rgb565>(color::LightSlateGray);
static constexpr auto SlateGray = TruncateColor<Rgb565>(color::SlateGray);
static constexpr auto DarkSlateGray =
    TruncateColor<Rgb565>(color::DarkSlateGray);
static constexpr auto Black = TruncateColor<Rgb565>(color::Black);

}  // namespace rgb565

}  // namespace color

}  // namespace roo_display
