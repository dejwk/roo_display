#pragma once

#include <assert.h>
#include <inttypes.h>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"

namespace roo_display {

typedef uint16_t unicode_t;

// Writes the UTF-8 representation of the rune to buf. The `buf` must have
// sufficient size (4 is always safe). Returns the number of bytes actually
// written.
inline int EncodeRuneAsUtf8(uint32_t rune, uint8_t *buf) {
  if (rune <= 0x7F) {
    buf[0] = rune;
    return 1;
  }
  if (rune <= 0x7FF) {
    buf[1] = (rune & 0x3F) | 0x80;
    rune >>= 6;
    buf[0] = rune | 0xC0;
    return 2;
  }
  if (rune <= 0xFFFF) {
    buf[2] = (rune & 0x3F) | 0x80;
    rune >>= 6;
    buf[1] = (rune & 0x3F) | 0x80;
    rune >>= 6;
    buf[0] = rune | 0xE0;
    return 3;
  }
  buf[3] = (rune & 0x3F) | 0x80;
  rune >>= 6;
  buf[2] = (rune & 0x3F) | 0x80;
  rune >>= 6;
  buf[1] = (rune & 0x3F) | 0x80;
  rune >>= 6;
  buf[0] = rune | 0xF0;
  return 4;
}

class Utf8Decoder {
 public:
  Utf8Decoder(const uint8_t *data, uint32_t size)
      : data_(data), remaining_(size) {}
  Utf8Decoder(const char *data, uint32_t size)
      : data_((const uint8_t *)data), remaining_(size) {}

  bool has_next() const { return remaining_ > 0; }
  const uint8_t *data() const { return data_; }
  uint32_t remaining() const { return remaining_; }

  unicode_t next() {
    --remaining_;
    uint8_t first = *data_++;
    // 7 bit Unicode
    if ((first & 0x80) == 0x00) {
      return first;
    }

    // 11 bit Unicode
    if (((first & 0xE0) == 0xC0) && (remaining_ >= 1)) {
      --remaining_;
      uint8_t second = *data_++;
      return ((first & 0x1F) << 6) | (second & 0x3F);
    }

    // 16 bit Unicode
    if (((first & 0xF0) == 0xE0) && (remaining_ >= 2)) {
      remaining_ -= 2;
      uint8_t second = *data_++;
      uint8_t third = *data_++;
      return ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | ((third & 0x3F));
    }

    // 21 bit Unicode not supported so fall-back to extended ASCII
    if ((first & 0xF8) == 0xF0) {
    }
    // fall-back to extended ASCII
    return first;
  }

 private:
  const uint8_t *data_;
  uint32_t remaining_;
};

// Helper Utf8 iterator that allows to peek at up to 8 subsequent
// characters before consuming the next one. Useful for fonts supporting
// kerning and ligatures.
class Utf8LookAheadDecoder {
 public:
  Utf8LookAheadDecoder(const uint8_t *data, uint32_t size)
      : decoder_(data, size), buffer_offset_(0), buffer_size_(0) {
    // Fill in the lookahead buffer.
    while (decoder_.has_next() && lookahead_buffer_size() < 8) {
      push(decoder_.next());
    }
  }

  int lookahead_buffer_size() const { return buffer_size_; }

  bool has_next() const { return lookahead_buffer_size() > 0; }

  unicode_t peek_next(int index) const {
    assert(index < lookahead_buffer_size());
    return buffer_[(buffer_offset_ + index) % 8];
  }

  unicode_t next() {
    assert(has_next());
    unicode_t result = buffer_[buffer_offset_];
    ++buffer_offset_;
    buffer_offset_ %= 8;
    --buffer_size_;
    if (decoder_.has_next()) {
      push(decoder_.next());
    }

    return result;
  }

 private:
  void push(unicode_t c) {
    assert(lookahead_buffer_size() < 8);
    buffer_[(buffer_offset_ + buffer_size_) % 8] = c;
    ++buffer_size_;
  }

  Utf8Decoder decoder_;
  unicode_t buffer_[8];
  int buffer_offset_;
  int buffer_size_;
};

enum FontLayout { FONT_LAYOUT_HORIZONTAL, FONT_LAYOUT_VERTICAL };

class FontMetrics {
 public:
  FontMetrics(int ascent, int descent, int linegap, int16_t xMin, int16_t yMin,
              int16_t xMax, int16_t yMax, int max_right_overhang)
      : ascent_(ascent),
        descent_(descent),
        linegap_(linegap),
        bbox_(xMin, yMin, xMax, yMax),
        max_right_overhang_(max_right_overhang) {}

  FontMetrics() = default;
  FontMetrics(FontMetrics &&) = default;
  FontMetrics &operator=(FontMetrics &&) = default;

  int16_t ascent() const { return ascent_; }
  int16_t descent() const { return descent_; }  // Usually a negative value.
  int16_t linegap() const { return linegap_; }
  int16_t linespace() const { return ascent() - descent() + linegap(); }

  // Return maximum glyph extents in freeType coordinates.
  int16_t glyphXMin() const { return bbox_.xMin(); }
  int16_t glyphYMin() const { return bbox_.yMin(); }
  int16_t glyphXMax() const { return bbox_.xMax(); }
  int16_t glyphYMax() const { return bbox_.yMax(); }

  int maxWidth() const { return bbox_.width(); }
  int maxHeight() const { return bbox_.height(); }

  int minLsb() const { return bbox_.xMin(); }
  int minRsb() const { return -max_right_overhang_; }

 private:
  int16_t ascent_;
  int16_t descent_;
  int16_t linegap_;
  Box bbox_;  // In freeType coordinates; i.e. Y grows up the screen
  int16_t max_right_overhang_;
};

class FontProperties {
 public:
  enum Charset {
    CHARSET_ASCII,       // 7-bit
    CHARSET_UNICODE_BMP  // 16-bit codes, usually UTF-encoded.
  };

  enum Spacing { SPACING_PROPORTIONAL, SPACING_MONOSPACE };

  enum Smoothing { SMOOTHING_NONE, SMOOTHING_GRAYSCALE };

  enum Kerning { KERNING_NONE, KERNING_PAIRS };

  FontProperties() = default;
  FontProperties(FontProperties &&) = default;
  FontProperties &operator=(FontProperties &&) = default;

  FontProperties(Charset charset, Spacing spacing, Smoothing smoothing,
                 Kerning kerning)
      : charset_(charset),
        spacing_(spacing),
        smoothing_(smoothing),
        kerning_(kerning) {}

  Charset charset() const { return charset_; }
  Spacing spacing() const { return spacing_; }
  Smoothing smoothing() const { return smoothing_; }
  Kerning kerning() const { return kerning_; }

 private:
  Charset charset_;
  Spacing spacing_;
  Smoothing smoothing_;
  Kerning kerning_;
};

class GlyphMetrics {
 public:
  // Parameters are in freeType glyph coordinates, i.e. positive Y up
  GlyphMetrics(int16_t glyphXMin, int16_t glyphYMin, int16_t glyphXMax,
               int16_t glyphYMax, int advance)
      : bbox_(glyphXMin, -glyphYMax, glyphXMax, -glyphYMin),
        advance_(advance) {}

  GlyphMetrics() = default;
  GlyphMetrics(const GlyphMetrics &) = default;
  GlyphMetrics(GlyphMetrics &&) = default;
  GlyphMetrics &operator=(GlyphMetrics &&) = default;
  GlyphMetrics &operator=(const GlyphMetrics &) = default;

  const Box &screen_extents() const { return bbox_; }

  // Returns the bounding box coordinates in freeType convention (positive Y up)

  int glyphXMin() const { return bbox_.xMin(); }
  int glyphXMax() const { return bbox_.xMax(); }
  int glyphYMin() const { return -bbox_.yMax(); }
  int glyphYMax() const { return -bbox_.yMin(); }

  // Other freetype conventional accessors.
  int bearingX() const { return bbox_.xMin(); }
  int bearingY() const { return -bbox_.yMin(); }
  int rsb() const { return advance() - (bbox_.xMax() + 1); }
  int lsb() const { return bbox_.xMin(); }
  int width() const { return bbox_.width(); }
  int height() const { return bbox_.height(); }

  int advance() const { return advance_; }

 private:
  Box bbox_;  // In screen coordinates; i.e. positive Y down.
  int advance_;
};

class Font {
 public:
  const FontMetrics &metrics() const { return metrics_; }
  const FontProperties &properties() const { return properties_; }

  virtual bool getGlyphMetrics(unicode_t code, FontLayout layout,
                               GlyphMetrics *result) const = 0;

  // See https://www.freetype.org/freetype2/docs/glyphs/glyphs-3.html

  virtual void drawHorizontalString(const Surface &s, const uint8_t *utf8_data,
                                    uint32_t size, Color color) const = 0;

  // Returns metrics of the specified string, as if it was a single glyph.
  virtual GlyphMetrics getHorizontalStringMetrics(const uint8_t *utf8_data,
                                                  uint32_t size) const = 0;

  // Returns metrics of the consecutive glyphs of the specified string,
  // beginning at the specified `offset`, and stores them in the `result`. The
  // glyphs may be overlapping due to kerning. The number of glyphs in the
  // result is limited by `max_count`. Returns the number of glyphs actually
  // measured, which may be smaller than `max_count` if the input string is
  // shorter.
  virtual uint32_t getHorizontalStringGlyphMetrics(
      const uint8_t *utf8_data, uint32_t size, GlyphMetrics *result,
      uint32_t offset, uint32_t max_count) const = 0;

  virtual ~Font() {}

 protected:
  void init(FontMetrics metrics, FontProperties properties) {
    metrics_ = std::move(metrics);
    properties_ = std::move(properties);
  }

 private:
  FontMetrics metrics_;
  FontProperties properties_;
};

}  // namespace roo_display