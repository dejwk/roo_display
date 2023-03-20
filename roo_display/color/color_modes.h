#pragma once

#include <inttypes.h>

#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_display/color/transparency_mode.h"

namespace roo_display {

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

class Rgba8888 {
 public:
  static const int8_t bits_per_pixel = 32;

  inline constexpr Color toArgbColor(uint32_t in) const {
    return Color(in >> 8 | (in & 0xFF) << 24);
  }

  inline constexpr uint32_t fromArgbColor(Color color) const {
    return color.asArgb() << 8 | color.asArgb() >> 24;
  }

  inline uint32_t rawAlphaBlend(uint32_t bg, Color color) const {
    return AlphaBlend(toArgbColor(bg), color).asArgb();
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

class Rgb888 {
 public:
  static const int8_t bits_per_pixel = 24;

  inline constexpr Color toArgbColor(uint32_t in) const {
    return Color(in | 0xFF000000);
  }

  inline constexpr uint32_t fromArgbColor(Color color) const {
    return color.asArgb() & 0x00FFFFFF;
  }

  inline uint32_t rawAlphaBlend(uint32_t bg, Color color) const {
    return fromArgbColor(AlphaBlend(toArgbColor(bg), color));
  }

  constexpr TransparencyMode transparency() const { return TRANSPARENCY_NONE; }
};

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

  inline uint8_t rawAlphaBlend(uint16_t bg, Color fg) const {
    return fromArgbColor(AlphaBlend(toArgbColor(bg), fg));
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

}  // namespace roo_display
