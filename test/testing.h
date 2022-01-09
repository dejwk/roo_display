#pragma once
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "roo_display/core/color.h"
#include "roo_display/core/device.h"
#include "roo_display/core/rasterizable.h"
#include "roo_display/core/streamable.h"
#include "roo_display/internal/raw_streamable.h"

using namespace testing;
using std::string;

namespace roo_display {

inline static Monochrome WhiteOnBlack() {
  return Monochrome(color::White, color::Black);
}

namespace internal {

char NextChar(std::istream& in) {
  if (in.eof()) {
    ADD_FAILURE() << "Reading past EOF";
    return '_';
  }
  return in.get();
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
    ADD_FAILURE();
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
    ADD_FAILURE();
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
    ADD_FAILURE();
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
    ADD_FAILURE();
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
  ParserStream<ColorMode>(ColorMode mode, const string& content)
      : mode_(mode), stream_(content) {}

  void Read(Color* buf, uint16_t size) override {
    while (size-- > 0) *buf++ = next();
  }

  void Skip(uint32_t count) override { skip(count); }

  TransparencyMode transparency() const { return mode_.transparency(); }

  Color next() { return NextColorFromString(mode_, stream_); }

  void skip(int16_t count) {
    while (count-- > 0) next();
  }

 private:
  ColorMode mode_;
  std::istringstream stream_;
};

template <typename ColorMode>
class ParserStreamable : public Streamable {
 public:
  ParserStreamable(int16_t width, int16_t height, string data)
      : ParserStreamable(ColorMode(), width, height, std::move(data)) {}

  ParserStreamable(ColorMode mode, int16_t width, int16_t height, string data)
      : mode_(mode), width_(width), height_(height), data_(std::move(data)) {}

  int16_t width() const { return width_; }
  int16_t height() const { return height_; }
  Box extents() const { return Box(0, 0, width() - 1, height() - 1); }
  const ColorMode& color_mode() const { return mode_; }

  TransparencyMode GetTransparencyMode() const override {
    return color_mode().transparency();
  }

  std::unique_ptr<ParserStream<ColorMode>> CreateRawStream() const {
    return std::unique_ptr<ParserStream<ColorMode>>(
        new ParserStream<ColorMode>(mode_, data_));
  }

  std::unique_ptr<PixelStream> CreateStream() const override {
    return std::unique_ptr<PixelStream>(
        new ParserStream<ColorMode>(mode_, data_));
  }

 private:
  ColorMode mode_;
  int16_t width_;
  int16_t height_;
  string data_;
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
    auto stream = streamable.CreateRawStream();
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
    auto stream = streamable.CreateRawStream();
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
    auto stream = streamable.CreateRawStream();
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
    auto stream = streamable.CreateRawStream();
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
    auto stream = streamable.CreateRawStream();
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
    auto stream = streamable.CreateRawStream();
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
    auto stream = streamable.CreateRawStream();
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
  StreamablePrinter(Rgb565 mode) : mode_(mode) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.CreateRawStream();
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
  StreamablePrinter(Argb6666 mode) : mode_(mode) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.CreateRawStream();
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
  StreamablePrinter(Rgb565WithTransparency mode) : mode_(mode) {}

  template <typename RawStream>
  void PrintContent(const RawStreamable& streamable, RawStream& os) {
    auto stream = streamable.CreateRawStream();
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
  os << extents.width();
  os << "x";
  os << extents.height();
  os << " " << streamable.color_mode();
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

template <typename ExpectedStreamable>
class ColorMatcher {
 public:
  ColorMatcher(ExpectedStreamable expected) : expected_(std::move(expected)) {}

  template <typename T>
  bool MatchAndExplain(const T& actual, MatchResultListener* listener) const {
    bool matches = true;
    std::ostringstream mismatch_message;
    if (actual.extents().width() != expected_.extents().width() ||
        actual.extents().height() != expected_.extents().height()) {
      mismatch_message << "\nDimension mismatch: " << actual.extents().width()
                       << "x" << actual.extents().height();
      matches = false;
    } else {
      // if (actual.extents() != expected_.extents()) {
      //   *listener << "Dimension mismatch: " << actual.extents();
      //   return false;
      // }
      int32_t count = actual.extents().width() * actual.extents().height();
      auto actual_stream = actual.CreateRawStream();
      auto expected_stream = expected_.CreateRawStream();
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
inline PolymorphicMatcher<ColorMatcher<internal::ParserStreamable<ColorMode>>>
MatchesContent(ColorMode mode, int16_t width, int16_t height, string expected) {
  return MakePolymorphicMatcher(
      ColorMatcher<internal::ParserStreamable<ColorMode>>(
          internal::ParserStreamable<ColorMode>(mode, width, height,
                                                expected)));
}

template <typename RawStreamable>
inline PolymorphicMatcher<ColorMatcher<RawStreamable>> MatchesContent(
    RawStreamable streamable) {
  return MakePolymorphicMatcher(
      ColorMatcher<RawStreamable>(std::move(streamable)));
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

  std::unique_ptr<TestColorStream> CreateRawStream() const {
    return std::unique_ptr<TestColorStream>(new TestColorStream(data_));
  }

 private:
  ColorMode color_mode_;
  Box extents_;
  const Color* data_;
};

template <typename ColorMode>
internal::ParserStreamable<ColorMode> MakeTestStreamable(const ColorMode& mode,
                                                         int16_t width,
                                                         int16_t height,
                                                         string content) {
  return internal::ParserStreamable<ColorMode>(mode, width, height, content);
}

template <typename ColorMode>
DrawableRawStreamable<internal::ParserStreamable<ColorMode>> MakeTestDrawable(
    const ColorMode& mode, int16_t width, int16_t height, string content) {
  return MakeDrawableRawStreamable(
      MakeTestStreamable(mode, width, height, std::move(content)));
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
        buffer_(new Color[width * height]) {
    writeRect(PAINT_MODE_REPLACE, 0, 0, width - 1, height - 1, background);
  }

  FakeOffscreen(Box extents, Color background = color::Transparent,
                ColorMode color_mode = ColorMode())
      : DisplayDevice(extents.width(), extents.height()),
        color_mode_(std::move(color_mode)),
        buffer_(new Color[extents.width() * extents.height()]) {
    writeRect(PAINT_MODE_REPLACE, 0, 0, extents.width() - 1,
              extents.height() - 1, background);
  }

  FakeOffscreen(const FakeOffscreen& other)
      : DisplayDevice(other.raw_width(), other.raw_height()),
        color_mode_(other.color_mode()),
        buffer_(new Color[other.raw_width() * other.raw_height()]) {
    memcpy(buffer_.get(), other.buffer_.get(),
           other.raw_width() * other.raw_height() * sizeof(Color));
  }

  void orientationUpdated() override {}

  // Required to implement 'Streamable'.
  Box extents() const {
    return Box(0, 0, effective_width() - 1, effective_height() - 1);
  }

  // Required to implement 'Streamable'.
  const ColorMode& color_mode() const { return color_mode_; }

  // Required to implement 'Streamable'.
  std::unique_ptr<TestColorStream> CreateRawStream() const {
    return std::unique_ptr<TestColorStream>(new TestColorStream(buffer_.get()));
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  PaintMode mode) override {
    window_ = Box(x0, y0, x1, y1);
    paint_mode_ = mode;
    cursor_x_ = x0;
    cursor_y_ = y0;
  }

  void write(Color* color, uint32_t pixel_count) override {
    while (pixel_count-- > 0) {
      writePixel(paint_mode_, cursor_x_, cursor_y_, *color++);
      ++cursor_x_;
      if (cursor_x_ > window_.xMax()) {
        cursor_x_ = window_.xMin();
        ++cursor_y_;
      }
    }
  }

  void writePixels(PaintMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    while (pixel_count-- > 0) {
      writePixel(mode, *x++, *y++, *color++);
    }
  }

  void fillPixels(PaintMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    while (pixel_count-- > 0) {
      writePixel(mode, *x++, *y++, color);
    }
  }

  void writeRects(PaintMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      writeRect(mode, *x0++, *y0++, *x1++, *y1++, *color++);
    }
  }

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      writeRect(mode, *x0++, *y0++, *x1++, *y1++, color);
    }
  }

  const Color* buffer() { return buffer_.get(); }

  void writeRect(PaintMode mode, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                 Color color) {
    for (int16_t y = y0; y <= y1; ++y) {
      for (int16_t x = x0; x <= x1; ++x) {
        writePixel(mode, x, y, color);
      }
    }
  }

  void writePixel(PaintMode mode, int16_t x, int16_t y, Color color) {
    EXPECT_GE(x, 0);
    EXPECT_LT(x, effective_width());
    EXPECT_GE(y, 0);
    EXPECT_LT(y, effective_height());
    if (x < 0 || y < 0 || x >= effective_width() || y >= effective_height()) {
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
    if (mode == PAINT_MODE_BLEND) {
      color = alphaBlend(buffer_[y * raw_width() + x], color);
    }
    color = color_mode_.toArgbColor(color_mode_.fromArgbColor(color));
    buffer_[y * raw_width() + x] = color;
  }

 private:
  friend TestColorStreamable<ColorMode> RasterOf<ColorMode>(
      const FakeOffscreen<ColorMode>&);

  ColorMode color_mode_;
  std::unique_ptr<Color[]> buffer_;
  Box window_;
  PaintMode paint_mode_;
  int16_t cursor_x_;
  int16_t cursor_y_;
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
      : offscreen_(width, height, background, color_mode) {}

  FakeFilteringOffscreen(Box extents, Color background = color::Transparent,
                         ColorMode color_mode = ColorMode())
      : offscreen_(extents, background, color_mode) {}

  void setOrientation(Orientation orientation) {
    offscreen_.setOrientation(orientation);
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  PaintMode mode) override {
    window_ = Box(x0, y0, x1, y1);
    paint_mode_ = mode;
    cursor_x_ = x0;
    cursor_y_ = y0;
  }

  void write(Color* color, uint32_t pixel_count) override {
    while (pixel_count-- > 0) {
      writePixel(paint_mode_, cursor_x_, cursor_y_, *color++);
      ++cursor_x_;
      if (cursor_x_ > window_.xMax()) {
        cursor_x_ = window_.xMin();
        ++cursor_y_;
      }
    }
  }

  void writePixels(PaintMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    while (pixel_count-- > 0) {
      writePixel(mode, *x++, *y++, *color++);
    }
  }

  void fillPixels(PaintMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    while (pixel_count-- > 0) {
      writePixel(mode, *x++, *y++, color);
    }
  }

  void writeRects(PaintMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      writeRect(mode, *x0++, *y0++, *x1++, *y1++, *color++);
    }
  }

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      writeRect(mode, *x0++, *y0++, *x1++, *y1++, color);
    }
  }

  const FakeOffscreen<ColorMode>& offscreen() const { return offscreen_; }

  void writeRect(PaintMode mode, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                 Color color) {
    for (int16_t y = y0; y <= y1; ++y) {
      for (int16_t x = x0; x <= x1; ++x) {
        writePixel(mode, x, y, color);
      }
    }
  }

  void writePixel(PaintMode mode, int16_t x, int16_t y, Color color) {
    Filter filter;
    filter.writePixel(mode, x, y, color, &offscreen_);
  }

 private:
  FakeOffscreen<ColorMode> offscreen_;
  Box window_;
  PaintMode paint_mode_;
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
        filter_(FilterFactory::Create(offscreen_)) {}

  FilteredOutput(Box extents, Color background = color::Transparent,
                 ColorMode color_mode = ColorMode())
      : offscreen_(extents, background, color_mode),
        filter_(FilterFactory::Create(offscreen_)) {}

  void setOrientation(Orientation orientation) {
    offscreen_.setOrientation(orientation);
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  PaintMode mode) override {
    filter_->setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    filter_->write(color, pixel_count);
  }

  void writePixels(PaintMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    filter_->writePixels(mode, color, x, y, pixel_count);
  }

  void fillPixels(PaintMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    filter_->fillPixels(mode, color, x, y, pixel_count);
  }

  void writeRects(PaintMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    filter_->writeRects(mode, color, x0, y0, x1, y1, count);
  }

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    filter_->fillRects(mode, color, x0, y0, x1, y1, count);
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

// Ensures that drawTo uses CreateStream().
class ForcedStreamable : public Streamable {
 public:
  ForcedStreamable(const Streamable* delegate) : delegate_(delegate) {}

  Box extents() const override { return delegate_->extents(); }

  std::unique_ptr<PixelStream> CreateStream() const override {
    return delegate_->CreateStream();
  }

  TransparencyMode GetTransparencyMode() const override {
    return delegate_->GetTransparencyMode();
  }

 private:
  const Streamable* delegate_;
};

// Ensures that drawTo and CreateStream() use ReadPixels().
class ForcedRasterizable : public Rasterizable {
 public:
  ForcedRasterizable(const Rasterizable* delegate) : delegate_(delegate) {}

  Box extents() const override { return delegate_->extents(); }

  void ReadColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    delegate_->ReadColors(x, y, count, result);
  }

  TransparencyMode GetTransparencyMode() const override {
    return delegate_->GetTransparencyMode();
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

}  // namespace roo_display