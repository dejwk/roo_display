#pragma once
#include <cstring>
#include <string>
#include <utility>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "roo_display/color/color.h"
#include "roo_display/color/color_mode_indexed.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/color/named.h"
#include "roo_display/core/device.h"
#include "roo_display/core/rasterizable.h"
#include "roo_display/core/streamable.h"
#include "roo_display/internal/color_format.h"
#include "roo_display/internal/color_io.h"
#include "roo_display/internal/raw_streamable.h"

using namespace testing;
using std::string;

namespace roo_display {

inline static Monochrome WhiteOnBlack() {
  return Monochrome(color::White, color::Black);
}

namespace internal {

char NextChar(std::istream& in) {
  char c = in.get();
  if (c < 0) {
    ADD_FAILURE() << "Reading past EOF";
    return '_';
  }
  return c;
}

template <typename ColorMode>
Color NextColorFromString(const ColorMode&, std::istream&);

template <>
Color NextColorFromString<Monochrome>(const Monochrome& mode,
                                      std::istream& in) {
  char c = NextChar(in);
  if (c == ' ') {
    return mode.bg();
  } else if (c == '*') {
    return mode.fg();
  } else {
    ADD_FAILURE() << "Invalid character for Monochrome: " << (int)c;
    return Color();
  }
}

uint8_t ParseHexNibble(std::istream& in) {
  char c;
  do {
    c = NextChar(in);
  } while (c == ' ');
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  } else {
    ADD_FAILURE() << "Invalid character for hex nibble: " << (int)c;
    return 0;
  }
}

uint8_t ParseGrayscale4(std::istream& in) {
  char c = NextChar(in);
  if (c == ' ') {
    return 0;
  } else if (c == '*') {
    return 0xF;
  }
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  } else {
    ADD_FAILURE() << "Invalid character for Grayscale4: " << (int)c;
    return 0;
  }
}

uint8_t ParseAlpha4(std::istream& in) {
  char c = NextChar(in);
  if (c == ' ') {
    return 0;
  } else if (c == '*') {
    return 0xF;
  }
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  } else {
    ADD_FAILURE() << "Invalid character for Alpha4: " << (int)c;
    return 0;
  }
}

uint8_t ParseHexByte(std::istream& in) {
  return ParseHexNibble(in) << 4 | ParseHexNibble(in);
}

template <>
Color NextColorFromString<Alpha4>(const Alpha4& mode, std::istream& in) {
  Color c = mode.color();
  c.set_a(ParseAlpha4(in) * 0x11);
  return c;
}

template <>
Color NextColorFromString<Grayscale4>(const Grayscale4& mode,
                                      std::istream& in) {
  uint8_t c = ParseGrayscale4(in) * 0x11;
  return Color(c, c, c);
}

template <>
Color NextColorFromString<Alpha8>(const Alpha8& mode, std::istream& in) {
  return Color(((uint32_t)ParseHexByte(in)) << 24);
}

template <>
Color NextColorFromString<internal::Indexed<1>>(
    const internal::Indexed<1>& mode, std::istream& in) {
  uint8_t idx = ParseHexNibble(in) & 0x01;
  return mode.toArgbColor(idx);
}

template <>
Color NextColorFromString<internal::Indexed<2>>(
    const internal::Indexed<2>& mode, std::istream& in) {
  uint8_t idx = ParseHexNibble(in) & 0x03;
  return mode.toArgbColor(idx);
}

template <>
Color NextColorFromString<internal::Indexed<4>>(
    const internal::Indexed<4>& mode, std::istream& in) {
  uint8_t idx = ParseHexNibble(in) & 0x0F;
  return mode.toArgbColor(idx);
}

template <>
Color NextColorFromString<internal::Indexed<8>>(
    const internal::Indexed<8>& mode, std::istream& in) {
  uint8_t idx = ParseHexByte(in);
  return mode.toArgbColor(idx);
}

template <>
Color NextColorFromString<Grayscale8>(const Grayscale8& mode,
                                      std::istream& in) {
  uint8_t c = ParseHexByte(in);
  return Color(c, c, c);
}

template <>
Color NextColorFromString<Argb4444>(const Argb4444& mode, std::istream& in) {
  Color color;
  color.set_a(ParseHexNibble(in) * 17);
  color.set_r(ParseHexNibble(in) * 17);
  color.set_g(ParseHexNibble(in) * 17);
  color.set_b(ParseHexNibble(in) * 17);
  return color;
}

template <>
Color NextColorFromString<Argb8888>(const Argb8888& mode, std::istream& in) {
  Color color;
  color.set_a(ParseHexByte(in));
  color.set_r(ParseHexByte(in));
  color.set_g(ParseHexByte(in));
  color.set_b(ParseHexByte(in));
  return color;
}

uint8_t ParseSixBitDigit(std::istream& in) {
  char c;
  do {
    c = NextChar(in);
  } while (c == ' ');
  if (c == '_') return 0;
  uint8_t offset = 1;
  if (c >= '0' && c <= '9') return c - '0' + offset;
  offset += 10;
  if (c >= 'A' && c <= 'Z') return c - 'A' + offset;
  offset += 26;
  if (c >= 'a' && c <= 'z') return c - 'a' + offset;
  offset += 26;
  return offset;
}

inline uint8_t ParseAndExtendSixBitDigit(std::istream& in) {
  uint8_t digit = ParseSixBitDigit(in);
  return (digit << 2) | (digit >> 4);
}

inline uint8_t ParseAndExtendFiveBitDigit(std::istream& in) {
  uint8_t digit = ParseSixBitDigit(in) & 0xFE;
  return (digit << 2) | (digit >> 3);
}

template <>
Color NextColorFromString<Rgb565>(const Rgb565& mode, std::istream& in) {
  Color color;
  color.set_a(0xFF);
  color.set_r(ParseAndExtendFiveBitDigit(in));
  color.set_g(ParseAndExtendSixBitDigit(in));
  color.set_b(ParseAndExtendFiveBitDigit(in));
  return color;
}

template <>
Color NextColorFromString<Argb6666>(const Argb6666& mode, std::istream& in) {
  Color color;
  color.set_a(ParseAndExtendSixBitDigit(in));
  color.set_r(ParseAndExtendSixBitDigit(in));
  color.set_g(ParseAndExtendSixBitDigit(in));
  color.set_b(ParseAndExtendSixBitDigit(in));
  return color;
}

template <>
Color NextColorFromString<Rgb565WithTransparency>(
    const Rgb565WithTransparency& mode, std::istream& in) {
  while (in.peek() == ' ') in.get();
  if (in.peek() == '.') {
    NextChar(in);
    NextChar(in);
    NextChar(in);
    return color::Transparent;
  }
  Color color;
  color.set_a(0xFF);
  color.set_r(ParseAndExtendFiveBitDigit(in));
  color.set_g(ParseAndExtendSixBitDigit(in));
  color.set_b(ParseAndExtendFiveBitDigit(in));
  return color;
}

template <typename ColorMode>
class ParserStream : public PixelStream {
 public:
  ParserStream(ColorMode mode, const string& content, Box extents, Box bounds)
      : mode_(mode),
        stream_(content),
        extents_(extents),
        bounds_(bounds),
        x_(extents.xMin()),
        y_(extents.yMin()) {}

  void Read(Color* buf, uint16_t size) override {
    while (size-- > 0) *buf++ = next();
  }

  void Skip(uint32_t count) override { skip(count); }

  TransparencyMode transparency() const { return mode_.transparency(); }

  Color next() {
    while (y_ < bounds_.yMin()) streamNext();
    while (x_ < bounds_.xMin() || x_ > bounds_.xMax()) streamNext();
    return streamNext();
  }

  Color streamNext() {
    Color result = NextColorFromString(mode_, stream_);
    x_++;
    if (x_ > extents_.xMax()) {
      x_ = extents_.xMin();
      y_++;
    }
    return result;
  }

  void skip(int16_t count) {
    while (count-- > 0) next();
  }

 private:
  ColorMode mode_;
  std::istringstream stream_;
  Box extents_;
  Box bounds_;
  int16_t x_, y_;
};

template <typename ColorMode>
class ParserStreamable : public Streamable {
 public:
  ParserStreamable(Box extents, string data)
      : ParserStreamable(ColorMode(), std::move(extents), std::move(data)) {}

  ParserStreamable(ColorMode mode, Box extents, string data)
      : mode_(mode), extents_(std::move(extents)), data_(std::move(data)) {}

  // int16_t width() const { return width_; }
  // int16_t height() const { return height_; }
  Box extents() const override { return extents_; }
  const ColorMode& color_mode() const { return mode_; }

  TransparencyMode getTransparencyMode() const override {
    return color_mode().transparency();
  }

  std::unique_ptr<ParserStream<ColorMode>> createRawStream() const {
    return std::unique_ptr<ParserStream<ColorMode>>(
        new ParserStream<ColorMode>(mode_, data_, extents(), extents()));
  }

  std::unique_ptr<PixelStream> createStream() const override {
    return std::unique_ptr<PixelStream>(
        new ParserStream<ColorMode>(mode_, data_, extents(), extents()));
  }

  std::unique_ptr<PixelStream> createStream(const Box& bounds) const override {
    return std::unique_ptr<PixelStream>(
        new ParserStream<ColorMode>(mode_, data_, extents(), bounds));
  }

 private:
  ColorMode mode_;
  Box extents_;
  string data_;
};

template <typename ColorMode>
class ParserDrawable : public Drawable {
 public:
  ParserDrawable(Box extents, string data)
      : streamable_(std::move(extents), data) {}

  ParserDrawable(ColorMode mode, Box extents, string data)
      : streamable_(mode, std::move(extents), data) {}

  Box extents() const override { return streamable_.extents(); }

 private:
  void drawTo(const Surface& s) const override { s.drawObject(streamable_); };

  ParserStreamable<ColorMode> streamable_;
};

template <typename ColorMode>
class ParserRasterizable : public Rasterizable {
 public:
  ParserRasterizable(Box extents, string data)
      : ParserRasterizable(ColorMode(), std::move(extents), data) {}

  ParserRasterizable(ColorMode mode, Box extents, string data)
      : extents_(std::move(extents)) {
    ParserStreamable<ColorMode> streamable(mode, extents_, data);
    auto stream = streamable.createRawStream();
    for (int i = 0; i < extents_.area(); ++i) {
      raster_.push_back(stream->next());
    }
  }

  Box extents() const override { return extents_; }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    while (count-- > 0) {
      *result++ = raster_[*y++ * extents_.width() + *x++];
    }
  }

 private:
  Box extents_;
  std::vector<Color> raster_;
};

template <typename RawStreamable,
          typename ColorMode = typename std::decay<
              decltype(std::declval<RawStreamable>().color_mode())>::type>
class StreamablePrinter;

template <typename RawStreamable>
class StreamablePrinter<RawStreamable, Monochrome> {
 public:
  StreamablePrinter(Monochrome color_mode)
      : color_mode_(std::move(color_mode)) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.createRawStream();
    for (int16_t j = 0; j < streamable.extents().height(); ++j) {
      os << "\n          \"";
      for (int16_t i = 0; i < streamable.extents().width(); ++i) {
        Color c = stream->next();
        if (c == color_mode_.fg()) {
          os << "*";
        } else if (c == color_mode_.bg()) {
          os << " ";
        } else {
          os << "x";
        }
      }
      os << "\"";
    }
  }

 private:
  Monochrome color_mode_;
};

char hexDigit(int d) { return (d >= 10) ? d - 10 + 'A' : d + '0'; }

template <typename IOStream>
void PrintHexByte(int d, IOStream& os) {
  os << hexDigit((d >> 4) & 0xF);
  os << hexDigit(d & 0xF);
}

template <typename RawStreamable>
class StreamablePrinter<RawStreamable, Argb8888> {
 public:
  StreamablePrinter(Argb8888 ignored) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.createRawStream();
    for (int16_t j = 0; j < streamable.extents().height(); ++j) {
      os << "\n          \"";
      for (int16_t i = 0; i < streamable.extents().width(); ++i) {
        Color color = stream->next();
        PrintHexByte(color.a(), os);
        PrintHexByte(color.r(), os);
        PrintHexByte(color.g(), os);
        PrintHexByte(color.b(), os);
        if (i + 1 < streamable.extents().width()) os << " ";
      }
      os << "\"";
    }
  }
};

template <typename RawStreamable>
class StreamablePrinter<RawStreamable, Argb4444> {
 public:
  StreamablePrinter(Argb4444 ignored) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.createRawStream();
    for (int16_t j = 0; j < streamable.extents().height(); ++j) {
      os << "\n          \"";
      for (int16_t i = 0; i < streamable.extents().width(); ++i) {
        Color color = stream->next();
        os << hexDigit(color.a() >> 4);
        os << hexDigit(color.r() >> 4);
        os << hexDigit(color.g() >> 4);
        os << hexDigit(color.b() >> 4);
        if (i + 1 < streamable.extents().width()) os << " ";
      }
      os << "\"";
    }
  }
};

template <typename RawStreamable>
class StreamablePrinter<RawStreamable, Alpha4> {
 public:
  StreamablePrinter(Alpha4 ignored) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.createRawStream();
    for (int16_t j = 0; j < streamable.extents().height(); ++j) {
      os << "\n          \"";
      for (int16_t i = 0; i < streamable.extents().width(); ++i) {
        Color color = stream->next();
        os << hexDigit(color.a() >> 4);
      }
      os << "\"";
    }
  }
};

char grayscale4Digit(int d) {
  return (d == 0) ? ' ' : (d == 0xF) ? '*' : (d >= 10) ? d - 10 + 'A' : d + '0';
}

// Very similar to Alpha4.
template <typename RawStreamable>
class StreamablePrinter<RawStreamable, Grayscale4> {
 public:
  StreamablePrinter(Grayscale4 ignored) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.createRawStream();
    for (int16_t j = 0; j < streamable.extents().height(); ++j) {
      os << "\n          \"";
      for (int16_t i = 0; i < streamable.extents().width(); ++i) {
        Color color = stream->next();
        if (color.r() == color.g() && color.r() == color.b() &&
            color.a() == 0xFF) {
          os << grayscale4Digit(color.r() >> 4);
        } else {
          os << "x";
        }
      }
      os << "\"";
    }
  }
};

template <typename RawStreamable>
class StreamablePrinter<RawStreamable, Alpha8> {
 public:
  StreamablePrinter(Alpha8 ignored) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.createRawStream();
    for (int16_t j = 0; j < streamable.extents().height(); ++j) {
      os << "\n          \"";
      for (int16_t i = 0; i < streamable.extents().width(); ++i) {
        Color color = stream->next();
        internal::PrintHexByte(color.a(), os);
        if (i + 1 < streamable.extents().width()) os << " ";
      }
      os << "\"";
    }
  }
};

template <typename RawStreamable>
class StreamablePrinter<RawStreamable, Grayscale8> {
 public:
  StreamablePrinter(Grayscale8 ignored) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.createRawStream();
    for (int16_t j = 0; j < streamable.extents().height(); ++j) {
      os << "\n          \"";
      for (int16_t i = 0; i < streamable.extents().width(); ++i) {
        Color color = stream->next();
        if (color.r() == color.g() && color.r() == color.b() &&
            color.a() == 0xFF) {
          internal::PrintHexByte(color.r(), os);
        } else {
          os << "x";
        }
        if (i + 1 < streamable.extents().width()) os << " ";
      }
      os << "\"";
    }
  }
};

char sixBitDigit(int d) {
  if (d == 0) return '_';
  d -= 1;
  if (d < 10) return d + '0';
  d -= 10;
  if (d < 26) return d + 'A';
  d -= 26;
  if (d < 26) return d + 'a';
  d -= 26;
  return d + '*';
}

// Representation:
// "!!!" means not representible in the 565 color mode
// Otherwise, it's RGB, with R and B brought to 6-bit by repeating
// MSB as LSB, and where each character is:
// _ for 0
// * for 63
// 0-9A-Za-z for 1-62
// In particular, "___" is black, "*__" is red, "***" is white.
template <typename RawStreamable>
class StreamablePrinter<RawStreamable, Rgb565> {
 public:
  StreamablePrinter(Rgb565 mode) : mode_(std::move(mode)) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.createRawStream();
    for (int16_t j = 0; j < streamable.extents().height(); ++j) {
      os << "\n          \"";
      for (int16_t i = 0; i < streamable.extents().width(); ++i) {
        Color color = stream->next();
        Color mapped = mode_.toArgbColor(mode_.fromArgbColor(color));
        if (mapped.asArgb() != color.asArgb()) {
          os << "!!!";
          // } else if (color.a() == 0) {
          //   os << "...";
        } else {
          os << sixBitDigit(color.r() >> 2);
          os << sixBitDigit(color.g() >> 2);
          os << sixBitDigit(color.b() >> 2);
        }
        if (i + 1 < streamable.extents().width()) os << " ";
      }
      os << "\"";
    }
  }

 private:
  Rgb565 mode_;
};

template <typename RawStreamable>
class StreamablePrinter<RawStreamable, Argb6666> {
 public:
  StreamablePrinter(Argb6666 mode) : mode_(std::move(mode)) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.createRawStream();
    for (int16_t j = 0; j < streamable.extents().height(); ++j) {
      os << "\n          \"";
      for (int16_t i = 0; i < streamable.extents().width(); ++i) {
        Color color = stream->next();
        Color mapped = mode_.toArgbColor(mode_.fromArgbColor(color));
        if (mapped.asArgb() != color.asArgb()) {
          os << "!!!";
          // } else if (color.a() == 0) {
          //   os << "...";
        } else {
          os << sixBitDigit(color.a() >> 2);
          os << sixBitDigit(color.r() >> 2);
          os << sixBitDigit(color.g() >> 2);
          os << sixBitDigit(color.b() >> 2);
        }
        if (i + 1 < streamable.extents().width()) os << " ";
      }
      os << "\"";
    }
  }

 private:
  Argb6666 mode_;
};

// Representation:
// "..." means transparency
// "!!!" means not representible in the 565 color mode
// Otherwise, it's RGB, with R and B brought to 6-bit by repeating
// MSB as LSB, and where each character is:
// _ for 0
// * for 63
// 0-9A-Za-z for 1-62
// In particular, "___" is black, "*__" is red, "***" is white.
template <typename RawStreamable>
class StreamablePrinter<RawStreamable, Rgb565WithTransparency> {
 public:
  StreamablePrinter(Rgb565WithTransparency mode) : mode_(std::move(mode)) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.createRawStream();
    for (int16_t j = 0; j < streamable.extents().height(); ++j) {
      os << "\n          \"";
      for (int16_t i = 0; i < streamable.extents().width(); ++i) {
        Color color = stream->next();
        Color mapped = mode_.toArgbColor(mode_.fromArgbColor(color));
        if (mapped.asArgb() != color.asArgb()) {
          os << "!!!";
        } else if (color.a() == 0) {
          os << "...";
        } else {
          os << sixBitDigit(color.r() >> 2);
          os << sixBitDigit(color.g() >> 2);
          os << sixBitDigit(color.b() >> 2);
        }
        if (i + 1 < streamable.extents().width()) os << " ";
      }
      os << "\"";
    }
  }

 private:
  Rgb565WithTransparency mode_;
};

template <typename RawStreamable, uint8_t bits>
class StreamablePrinter<RawStreamable, internal::Indexed<bits>> {
 public:
  StreamablePrinter(internal::Indexed<bits> mode) : mode_(std::move(mode)) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.createRawStream();
    for (int16_t j = 0; j < streamable.extents().height(); ++j) {
      os << "\n          \"";
      for (int16_t i = 0; i < streamable.extents().width(); ++i) {
        Color color = stream->next();
        int idx = FindIndex(color);
        if (idx < 0) {
          os << "x";
        } else if (bits <= 4) {
          os << hexDigit(idx);
        } else {
          PrintHexByte(idx, os);
          if (i + 1 < streamable.extents().width()) os << " ";
        }
      }
      os << "\"";
    }
  }

 private:
  int FindIndex(Color color) const {
    const Palette* palette = mode_.palette();
    const Color* colors = palette->colors();
    int size = palette->size();
    for (int i = 0; i < size; ++i) {
      if (colors[i] == color) return i;
    }
    return -1;
  }

  internal::Indexed<bits> mode_;
};

}  // namespace internal

// Basic debug-printing

std::ostream& operator<<(std::ostream& os, Orientation orientation) {
  os << orientation.asString();
  return os;
}

// Basic debug-printing of colors and color modes

std::ostream& operator<<(std::ostream& os, Color color) {
  os << "0x";
  internal::PrintHexByte(color.a(), os);
  internal::PrintHexByte(color.r(), os);
  internal::PrintHexByte(color.g(), os);
  internal::PrintHexByte(color.b(), os);
  return os;
}

std::ostream& operator<<(std::ostream& os, const Monochrome& mode) {
  os << "Monochrome(fg: " << mode.fg() << ", bg: " << mode.bg() << ")";
  return os;
}

std::ostream& operator<<(std::ostream& os, Argb8888 mode) {
  os << "ARGB_8888";
  return os;
}

std::ostream& operator<<(std::ostream& os, Argb6666 mode) {
  os << "ARGB_6666";
  return os;
}

std::ostream& operator<<(std::ostream& os, Argb4444 mode) {
  os << "ARGB_4444";
  return os;
}

std::ostream& operator<<(std::ostream& os, Alpha4 mode) {
  os << "Alpha4(" << mode.color() << ")";
  return os;
}

std::ostream& operator<<(std::ostream& os, Alpha8 mode) {
  os << "Alpha8(" << mode.color() << ")";
  return os;
}

std::ostream& operator<<(std::ostream& os, Rgb565 mode) {
  os << "RGB_565";
  return os;
}

std::ostream& operator<<(std::ostream& os, Rgb565WithTransparency mode) {
  os << "RGB_565_WITH_TRANSPARENCY";
  if (mode.transparency() != TRANSPARENCY_NONE) {
    uint16_t transparency = mode.fromArgbColor(Color(0));
    os << " (transparency: 0x";
    internal::PrintHexByte(transparency >> 8, os);
    internal::PrintHexByte((uint8_t)(transparency & 0xFF), os);
    os << ")";
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, Grayscale4 mode) {
  os << "GRAYSCALE_4";
  return os;
}

std::ostream& operator<<(std::ostream& os, Grayscale8 mode) {
  os << "GRAYSCALE_8";
  return os;
}

template <uint8_t bits>
std::ostream& operator<<(std::ostream& os, const internal::Indexed<bits>&) {
  os << "INDEXED_" << static_cast<int>(bits) << "BPP";
  return os;
}

bool operator==(const Monochrome& a, const Monochrome& b) {
  return a.bg() == b.bg() && a.fg() == b.fg();
}

bool operator==(const Argb8888& a, const Argb8888& b) { return true; }

bool operator==(const Argb6666& a, const Argb6666& b) { return true; }

bool operator==(const Argb4444& a, const Argb4444& b) { return true; }

bool operator==(const Alpha8& a, const Alpha8& b) { return true; }

bool operator==(const Alpha4& a, const Alpha4& b) { return true; }

bool operator==(const Grayscale8& a, const Grayscale8& b) { return true; }

bool operator==(const Grayscale4& a, const Grayscale4& b) { return true; }

bool operator==(const Rgb565& a, const Rgb565& b) { return true; }

bool operator==(const Rgb565WithTransparency& a,
                const Rgb565WithTransparency& b) {
  return a.fromArgbColor(color::Transparent) ==
         b.fromArgbColor(color::Transparent);
}

template <uint8_t bits>
bool operator==(const internal::Indexed<bits>& a,
                const internal::Indexed<bits>& b) {
  return a.palette() == b.palette();
}

// Prints the content of the specified streamable in a human-readable form,
// using a specified color mode.
template <typename ostream_type, typename RawStreamable,
          typename RawStream = RawStreamTypeOf<RawStreamable>,
          typename ColorMode = ColorModeOf<RawStreamable>>
ostream_type& PrintStreamableContent(ostream_type& os,
                                     const RawStreamable& streamable,
                                     ColorMode color_mode) {
  internal::StreamablePrinter<RawStreamable, ColorMode> printer(color_mode);
  printer.PrintContent(streamable, os);
  return os;
}

template <typename RawStreamable,
          typename Stream = RawStreamTypeOf<RawStreamable>,
          typename ColorMode = ColorModeOf<RawStreamable>>
std::ostream& operator<<(std::ostream& os, const RawStreamable& streamable) {
  Box extents = streamable.extents();
  os << streamable.color_mode();
  os << " [";
  os << extents.xMin();
  os << ", ";
  os << extents.yMin();
  os << ", ";
  os << extents.xMax();
  os << ", ";
  os << extents.yMax();
  os << "]";
  PrintStreamableContent(os, streamable, streamable.color_mode());
  return os;
}

template <typename mode_a, typename mode_b>
struct ColorModeComparer {
  bool operator()(const mode_a& a, const mode_b& b) const { return false; }
};

template <typename mode>
struct ColorModeComparer<mode, mode> {
  bool operator()(const mode& a, const mode& b) const { return a == b; }
};

template <typename mode_a, typename mode_b>
bool AreColorModesEqual(const mode_a& a, const mode_b& b) {
  ColorModeComparer<mode_a, mode_b> cmp;
  return cmp(a, b);
}

// Verifies a raw-streamable (actual) against an expectation, also raw
// streamable.
template <typename ExpectedStreamable>
class RawStreamableColorMatcher {
 public:
  RawStreamableColorMatcher(ExpectedStreamable expected)
      : expected_(std::move(expected)) {}

  template <typename T>
  bool MatchAndExplain(const T& actual, MatchResultListener* listener) const {
    bool matches = true;
    std::ostringstream mismatch_message;
    if (actual.extents() != expected_.extents()) {
      *listener << "Dimension mismatch: " << actual.extents();
      matches = false;
    } else {
      int32_t count = actual.extents().width() * actual.extents().height();
      auto actual_stream = actual.createRawStream();
      auto expected_stream = expected_.createRawStream();
      for (int i = 0; i < count; ++i) {
        Color expected_color = expected_stream->next();
        Color actual_color = actual_stream->next();
        if (actual_color.asArgb() != expected_color.asArgb()) {
          mismatch_message << "\nFirst mismatch at ("
                           << i % actual.extents().width() << ", "
                           << i / actual.extents().width()
                           << "), expected: " << expected_color
                           << ", seen: " << actual_color;
          matches = false;
          break;
        }
      }
    }
    if (!matches) {
      if (!AreColorModesEqual(actual.color_mode(), expected_.color_mode())) {
        *listener << "\nWhich, in the expectation's color space, is: ";
        PrintStreamableContent(*listener, actual, expected_.color_mode());
      }
      *listener << mismatch_message.str();
    }
    return matches;
  }

  void DescribeTo(::std::ostream* os) const { *os << expected_; }

  void DescribeNegationTo(::std::ostream* os) const {
    *os << "does not equal ";
    DescribeTo(os);
  }

 private:
  ExpectedStreamable expected_;
};

template <typename ColorMode>
inline PolymorphicMatcher<
    RawStreamableColorMatcher<internal::ParserStreamable<ColorMode>>>
MatchesContent(ColorMode mode, int16_t width, int16_t height, string expected) {
  return MatchesContent(mode, Box(0, 0, width - 1, height - 1),
                        std::move(expected));
}

template <typename ColorMode>
inline PolymorphicMatcher<
    RawStreamableColorMatcher<internal::ParserStreamable<ColorMode>>>
MatchesContent(ColorMode mode, Box bounds, string expected) {
  return MakePolymorphicMatcher(
      RawStreamableColorMatcher<internal::ParserStreamable<ColorMode>>(
          internal::ParserStreamable<ColorMode>(mode, bounds, expected)));
}

template <typename RawStreamable>
inline PolymorphicMatcher<RawStreamableColorMatcher<RawStreamable>>
MatchesContent(RawStreamable streamable) {
  return MakePolymorphicMatcher(
      RawStreamableColorMatcher<RawStreamable>(std::move(streamable)));
}

class TestColorStream {
 public:
  TestColorStream(const Color* data) : data_(data) {}
  Color next() { return *data_++; }

 private:
  const Color* data_;
};

template <typename ColorMode>
class TestColorStreamable {
 public:
  TestColorStreamable(int16_t width, int16_t height, const Color* data,
                      ColorMode color_mode = ColorMode())
      : color_mode_(color_mode),
        extents_(0, 0, width - 1, height - 1),
        data_(data) {}

  const Box& extents() const { return extents_; }

  const ColorMode& color_mode() const { return color_mode_; }

  std::unique_ptr<TestColorStream> createRawStream() const {
    return std::unique_ptr<TestColorStream>(new TestColorStream(data_));
  }

 private:
  ColorMode color_mode_;
  Box extents_;
  const Color* data_;
};

template <typename ColorMode>
internal::ParserStreamable<ColorMode> MakeTestStreamable(const ColorMode& mode,
                                                         Box extents,
                                                         string content) {
  return internal::ParserStreamable<ColorMode>(mode, std::move(extents),
                                               content);
}

template <typename ColorMode>
internal::ParserDrawable<ColorMode> MakeTestDrawable(const ColorMode& mode,
                                                     Box extents,
                                                     string content) {
  return internal::ParserDrawable<ColorMode>(mode, std::move(extents), content);
}

template <typename ColorMode>
internal::ParserRasterizable<ColorMode> MakeTestRasterizable(
    const ColorMode& mode, Box extents, string content) {
  return internal::ParserRasterizable<ColorMode>(mode, std::move(extents),
                                                 content);
}

DrawableRawStreamable<RawStreamableFilledRect> SolidRect(int16_t x0, int16_t y0,
                                                         int16_t x1, int16_t y1,
                                                         Color color) {
  return MakeDrawableRawStreamable(
      RawStreamableFilledRect(Box(x0, y0, x1, y1), color));
}

template <typename ColorMode>
class FakeOffscreen;

template <typename ColorMode>
TestColorStreamable<ColorMode> RasterOf(const FakeOffscreen<ColorMode>&);

template <typename ColorMode>
class FakeOffscreen : public DisplayDevice {
 public:
  FakeOffscreen(int16_t width, int16_t height,
                Color background = color::Transparent,
                ColorMode color_mode = ColorMode())
      : DisplayDevice(width, height),
        color_mode_(std::move(color_mode)),
        buffer_(new Color[width * height]),
        extents_(0, 0, width - 1, height - 1) {
    writeRect(BLENDING_MODE_SOURCE, 0, 0, width - 1, height - 1, background);
    resetPixelDrawCount();
  }

  FakeOffscreen(Box extents, Color background = color::Transparent,
                ColorMode color_mode = ColorMode())
      : DisplayDevice(extents.width(), extents.height()),
        color_mode_(std::move(color_mode)),
        buffer_(new Color[extents.width() * extents.height()]),
        extents_(extents) {
    writeRect(BLENDING_MODE_SOURCE, 0, 0, extents.width() - 1,
              extents.height() - 1, background);
    resetPixelDrawCount();
  }

  FakeOffscreen(const FakeOffscreen& other)
      : DisplayDevice(other.raw_width(), other.raw_height()),
        color_mode_(other.color_mode()),
        buffer_(new Color[other.raw_width() * other.raw_height()]),
        extents_(other.extents()),
        pixel_draw_count_(other.pixel_draw_count_) {
    memcpy(buffer_.get(), other.buffer_.get(),
           other.raw_width() * other.raw_height() * sizeof(Color));
  }

  void orientationUpdated() override {}

  Box extents() const { return extents_; }

  // Required to implement 'Streamable'.
  const ColorMode& color_mode() const { return color_mode_; }

  // Required to implement 'Streamable'.
  std::unique_ptr<TestColorStream> createRawStream() const {
    return std::unique_ptr<TestColorStream>(new TestColorStream(buffer_.get()));
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    window_ = Box(x0, y0, x1, y1);
    blending_mode_ = mode;
    cursor_x_ = x0;
    cursor_y_ = y0;
  }

  void write(Color* color, uint32_t pixel_count) override {
    while (pixel_count-- > 0) {
      writePixel(blending_mode_, cursor_x_, cursor_y_, *color++);
      ++cursor_x_;
      if (cursor_x_ > window_.xMax()) {
        cursor_x_ = window_.xMin();
        ++cursor_y_;
      }
    }
  }

  void writePixels(BlendingMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    while (pixel_count-- > 0) {
      writePixel(mode, *x++, *y++, *color++);
    }
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    while (pixel_count-- > 0) {
      writePixel(mode, *x++, *y++, color);
    }
  }

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      writeRect(mode, *x0++, *y0++, *x1++, *y1++, *color++);
    }
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      writeRect(mode, *x0++, *y0++, *x1++, *y1++, color);
    }
  }

  const ColorFormat& getColorFormat() const override {
    static const Argb8888 mode;
    static const internal::ColorFormatImpl<Argb8888, roo_io::kNativeEndian>
        color_format(mode);
    return color_format;
  }

  const Color* buffer() { return buffer_.get(); }

  uint64_t pixelDrawCount() const { return pixel_draw_count_; }

  void resetPixelDrawCount() { pixel_draw_count_ = 0; }

  void writeRect(BlendingMode mode, int16_t x0, int16_t y0, int16_t x1,
                 int16_t y1, Color color) {
    for (int16_t y = y0; y <= y1; ++y) {
      for (int16_t x = x0; x <= x1; ++x) {
        writePixel(mode, x, y, color);
      }
    }
  }

  void writePixel(BlendingMode mode, int16_t x, int16_t y, Color color) {
    EXPECT_GE(x, 0);
    EXPECT_LT(x, effective_width());
    EXPECT_GE(y, 0);
    EXPECT_LT(y, effective_height());
    if (x < 0 || y < 0 || x >= effective_width() || y >= effective_height()) {
      abort();
      return;
    }
    if (orientation().isXYswapped()) {
      std::swap(x, y);
    }
    // Now, x is horizontal, and y is vertical, in raw dimensions.
    if (orientation().isRightToLeft()) {
      x = raw_width() - x - 1;
    }
    if (orientation().isBottomToTop()) {
      y = raw_height() - y - 1;
    }
    color = ApplyBlending(mode, buffer_[y * raw_width() + x], color);
    color = color_mode_.toArgbColor(color_mode_.fromArgbColor(color));
    buffer_[y * raw_width() + x] = color;
    ++pixel_draw_count_;
  }

 private:
  friend TestColorStreamable<ColorMode> RasterOf<ColorMode>(
      const FakeOffscreen<ColorMode>&);

  ColorMode color_mode_;
  std::unique_ptr<Color[]> buffer_;
  Box extents_;
  Box window_;
  BlendingMode blending_mode_;
  int16_t cursor_x_;
  int16_t cursor_y_;
  uint64_t pixel_draw_count_ = 0;
};

template <typename ColorMode>
TestColorStreamable<ColorMode> RasterOf(
    const FakeOffscreen<ColorMode>& offscreen) {
  return TestColorStreamable<ColorMode>(
      offscreen.raw_width(), offscreen.raw_height(), offscreen.buffer_.get(),
      offscreen.color_mode_);
}

template <typename ReferenceDevice>
using ColorModeOfDevice = typename std::remove_reference<
    decltype(RasterOf(std::declval<const ReferenceDevice>())
                 .color_mode())>::type;

// To be used for the 'reference' devices, using trivial filter
// implementations.
template <typename ColorMode, typename Filter>
class FakeFilteringOffscreen : public DisplayOutput {
 public:
  FakeFilteringOffscreen(int16_t width, int16_t height,
                         Color background = color::Transparent,
                         ColorMode color_mode = ColorMode())
      : offscreen_(width, height, background, color_mode),
        filter_(offscreen_.extents()) {}

  FakeFilteringOffscreen(Box extents, Color background = color::Transparent,
                         ColorMode color_mode = ColorMode())
      : offscreen_(extents, background, color_mode),
        filter_(offscreen_.extents()) {}

  void setOrientation(Orientation orientation) {
    offscreen_.setOrientation(orientation);
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    window_ = Box(x0, y0, x1, y1);
    blending_mode_ = mode;
    cursor_x_ = x0;
    cursor_y_ = y0;
  }

  void write(Color* color, uint32_t pixel_count) override {
    while (pixel_count-- > 0) {
      writePixel(blending_mode_, cursor_x_, cursor_y_, *color++);
      ++cursor_x_;
      if (cursor_x_ > window_.xMax()) {
        cursor_x_ = window_.xMin();
        ++cursor_y_;
      }
    }
  }

  void writePixels(BlendingMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    while (pixel_count-- > 0) {
      writePixel(mode, *x++, *y++, *color++);
    }
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    while (pixel_count-- > 0) {
      writePixel(mode, *x++, *y++, color);
    }
  }

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      writeRect(mode, *x0++, *y0++, *x1++, *y1++, *color++);
    }
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      writeRect(mode, *x0++, *y0++, *x1++, *y1++, color);
    }
  }

  const ColorFormat& getColorFormat() const override {
    return offscreen_.getColorFormat();
  }

  const FakeOffscreen<ColorMode>& offscreen() const { return offscreen_; }

  void writeRect(BlendingMode mode, int16_t x0, int16_t y0, int16_t x1,
                 int16_t y1, Color color) {
    for (int16_t y = y0; y <= y1; ++y) {
      for (int16_t x = x0; x <= x1; ++x) {
        writePixel(mode, x, y, color);
      }
    }
  }

  void writePixel(BlendingMode mode, int16_t x, int16_t y, Color color) {
    filter_.writePixel(mode, x, y, color, &offscreen_);
  }

 private:
  FakeOffscreen<ColorMode> offscreen_;
  Filter filter_;
  Box window_;
  BlendingMode blending_mode_;
  int16_t cursor_x_;
  int16_t cursor_y_;
};

template <typename ColorMode, typename Filter>
TestColorStreamable<ColorMode> RasterOf(
    const FakeFilteringOffscreen<ColorMode, Filter>& filtering) {
  return RasterOf(filtering.offscreen());
}

template <typename ColorMode, typename FilterFactory>
class FilteredOutput : public DisplayOutput {
 public:
  FilteredOutput(int16_t width, int16_t height,
                 Color background = color::Transparent,
                 ColorMode color_mode = ColorMode())
      : offscreen_(width, height, background, color_mode),
        filter_(FilterFactory::Create(offscreen_, offscreen_.extents())) {}

  FilteredOutput(Box extents, Color background = color::Transparent,
                 ColorMode color_mode = ColorMode())
      : offscreen_(extents, background, color_mode),
        filter_(FilterFactory::Create(offscreen_)) {}

  void setOrientation(Orientation orientation) {
    offscreen_.setOrientation(orientation);
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    filter_->setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    filter_->write(color, pixel_count);
  }

  void writePixels(BlendingMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    filter_->writePixels(mode, color, x, y, pixel_count);
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    filter_->fillPixels(mode, color, x, y, pixel_count);
  }

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    filter_->writeRects(mode, color, x0, y0, x1, y1, count);
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    filter_->fillRects(mode, color, x0, y0, x1, y1, count);
  }

  const ColorFormat& getColorFormat() const override {
    return filter_->getColorFormat();
  }

  const FakeOffscreen<ColorMode>& offscreen() const { return offscreen_; }

 private:
  FakeOffscreen<ColorMode> offscreen_;
  std::unique_ptr<DisplayOutput> filter_;
};

template <typename ColorMode, typename FilterFactory>
TestColorStreamable<ColorMode> RasterOf(
    const FilteredOutput<ColorMode, FilterFactory>& filtering) {
  return RasterOf(filtering.offscreen());
}

// Ensures that drawTo uses createStream().
class ForcedStreamable : public Streamable {
 public:
  ForcedStreamable(const Streamable* delegate) : delegate_(delegate) {}

  Box extents() const override { return delegate_->extents(); }

  std::unique_ptr<PixelStream> createStream() const override {
    return delegate_->createStream();
  }

  std::unique_ptr<PixelStream> createStream(const Box& bounds) const override {
    return delegate_->createStream(bounds);
  }

  TransparencyMode getTransparencyMode() const override {
    return delegate_->getTransparencyMode();
  }

 private:
  const Streamable* delegate_;
};

// Ensures that drawTo and createStream() use ReadPixels().
class ForcedRasterizable : public Rasterizable {
 public:
  ForcedRasterizable(const Rasterizable* delegate) : delegate_(delegate) {}

  Box extents() const override { return delegate_->extents(); }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    delegate_->readColors(x, y, count, result);
  }

  TransparencyMode getTransparencyMode() const override {
    return delegate_->getTransparencyMode();
  }

 private:
  const Rasterizable* delegate_;
};

template <typename Obj>
class ForcedFillRect : public Drawable {
 public:
  ForcedFillRect(Obj delegate) : delegate_(std::move(delegate)) {}
  Box extents() const override { return delegate_.extents(); }

 private:
  void drawTo(const Surface& s) const override {
    Surface news = s;
    news.set_fill_mode(FILL_MODE_RECTANGLE);
    news.drawObject(delegate_);
  }

  Obj delegate_;
};

template <typename Obj>
ForcedFillRect<Obj> ForceFillRect(Obj obj) {
  return ForcedFillRect<Obj>(std::move(obj));
}

// Returns a copy of the specified drawable as a fake offscreen with the
// specified color mode and background. The result has the same extents as the
// input drawable.
template <typename ColorMode>
FakeOffscreen<ColorMode> CoercedTo(
    const Drawable& drawable, ColorMode color_mode = ColorMode(),
    Color fill = color::Transparent, FillMode fill_mode = FILL_MODE_VISIBLE,
    BlendingMode blending_mode = BLENDING_MODE_SOURCE_OVER,
    Color bgcolor = color::Transparent) {
  FakeOffscreen<ColorMode> result(drawable.extents(), fill, color_mode);
  result.begin();
  Box extents = drawable.extents();
  Surface s(result, -extents.xMin(), -extents.yMin(),
            Box(0, 0, extents.width() - 1, extents.height() - 1), false,
            bgcolor, fill_mode, blending_mode);
  s.drawObject(drawable);
  result.end();
  return result;
}

}  // namespace roo_display