#pragma once

#include <assert.h>
#include <inttypes.h>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/utf8.h"
#include "roo_io/base/string_view.h"

namespace roo_display {

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

  void drawHorizontalString(const Surface &s, roo_io::string_view text,
                            Color color) const {
    drawHorizontalString(s, text.data(), text.size(), color);
  }

  virtual void drawHorizontalString(const Surface &s, const char *utf8_data,
                                    uint32_t size, Color color) const = 0;

  // Returns metrics of the specified string, as if it was a single glyph.
  GlyphMetrics getHorizontalStringMetrics(roo_io::string_view text) const {
    return getHorizontalStringMetrics(text.data(), text.size());
  }

  virtual GlyphMetrics getHorizontalStringMetrics(const char *utf8_data,
                                                  uint32_t size) const = 0;

  // Returns metrics of the consecutive glyphs of the specified string,
  // beginning at the specified `offset`, and stores them in the `result`. The
  // glyphs may be overlapping due to kerning. The number of glyphs in the
  // result is limited by `max_count`. Returns the number of glyphs actually
  // measured, which may be smaller than `max_count` if the input string is
  // shorter.
  uint32_t getHorizontalStringGlyphMetrics(roo_io::string_view text,
                                           GlyphMetrics *result,
                                           uint32_t offset,
                                           uint32_t max_count) const {
    return getHorizontalStringGlyphMetrics(text.data(), text.size(), result,
                                           offset, max_count);
  }

  virtual uint32_t getHorizontalStringGlyphMetrics(
      const char *utf8_data, uint32_t size, GlyphMetrics *result,
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