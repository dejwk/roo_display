#include "roo_display/font/font.h"

#include "roo_logging.h"
#include "roo_io/text/unicode.h"

namespace roo_display {

void Font::drawGlyph(const Surface& s, char32_t code, FontLayout layout,
                     Color color) const {
  DCHECK(layout == FontLayout::kHorizontal);
  char encoded[4];
  int encoded_len = roo_io::WriteUtf8Char(encoded, code);
  drawHorizontalString(s, encoded, static_cast<uint32_t>(encoded_len), color);
}

roo_logging::Stream& operator<<(roo_logging::Stream& stream,
                                FontLayout layout) {
  switch (layout) {
    case FontLayout::kHorizontal:
      stream << "FontLayout::kHorizontal";
      break;
    case FontLayout::kVertical:
      stream << "FontLayout::kVertical";
      break;
    default:
      stream << "FontLayout::(unknown)";
      break;
  }
  return stream;
}

roo_logging::Stream& operator<<(roo_logging::Stream& stream,
                                FontProperties::Charset charset) {
  switch (charset) {
    case FontProperties::Charset::kAscii:
      stream << "FontProperties::Charset::kAscii";
      break;
    case FontProperties::Charset::kUnicodeBmp:
      stream << "FontProperties::Charset::kUnicodeBmp";
      break;
    default:
      stream << "FontProperties::Charset::(unknown)";
      break;
  }
  return stream;
}

roo_logging::Stream& operator<<(roo_logging::Stream& stream,
                                FontProperties::Spacing spacing) {
  switch (spacing) {
    case FontProperties::Spacing::kProportional:
      stream << "FontProperties::Spacing::kProportional";
      break;
    case FontProperties::Spacing::kMonospace:
      stream << "FontProperties::Spacing::kMonospace";
      break;
    default:
      stream << "FontProperties::Spacing::(unknown)";
      break;
  }
  return stream;
}

roo_logging::Stream& operator<<(roo_logging::Stream& stream,
                                FontProperties::Smoothing smoothing) {
  switch (smoothing) {
    case FontProperties::Smoothing::kNone:
      stream << "FontProperties::Smoothing::kNone";
      break;
    case FontProperties::Smoothing::kGrayscale:
      stream << "FontProperties::Smoothing::kGrayscale";
      break;
    default:
      stream << "FontProperties::Smoothing::(unknown)";
      break;
  }
  return stream;
}

roo_logging::Stream& operator<<(roo_logging::Stream& stream,
                                FontProperties::Kerning kerning) {
  switch (kerning) {
    case FontProperties::Kerning::kNone:
      stream << "FontProperties::Kerning::kNone";
      break;
    case FontProperties::Kerning::kPairs:
      stream << "FontProperties::Kerning::kPairs";
      break;
    default:
      stream << "FontProperties::Kerning::(unknown)";
      break;
  }
  return stream;
}

}  // namespace roo_display