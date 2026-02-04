#pragma once

#include <assert.h>
#include <inttypes.h>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_backport/string_view.h"

namespace roo_display {

/// Glyph layout direction.
enum FontLayout { FONT_LAYOUT_HORIZONTAL, FONT_LAYOUT_VERTICAL };

/// Basic font metrics (ascent, descent, bounding box, and line spacing).
class FontMetrics {
 public:
  /// Construct font metrics.
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

  /// Font ascent (positive).
  int16_t ascent() const { return ascent_; }
  /// Font descent (usually negative).
  int16_t descent() const { return descent_; }  // Usually a negative value.
  /// Additional gap between lines.
  int16_t linegap() const { return linegap_; }
  /// Line advance in pixels.
  int16_t linespace() const { return ascent() - descent() + linegap(); }

  /// Return maximum glyph extents in FreeType coordinates (Y up).
  int16_t glyphXMin() const { return bbox_.xMin(); }
  int16_t glyphYMin() const { return bbox_.yMin(); }
  int16_t glyphXMax() const { return bbox_.xMax(); }
  int16_t glyphYMax() const { return bbox_.yMax(); }

  /// Maximum glyph width.
  int maxWidth() const { return bbox_.width(); }
  /// Maximum glyph height.
  int maxHeight() const { return bbox_.height(); }

  /// Minimum left side bearing across glyphs.
  int minLsb() const { return bbox_.xMin(); }
  /// Minimum right side bearing across glyphs.
  int minRsb() const { return -max_right_overhang_; }

 private:
  int16_t ascent_;
  int16_t descent_;
  int16_t linegap_;
  Box bbox_;  // In freeType coordinates; i.e. Y grows up the screen
  int16_t max_right_overhang_;
};

/// Metadata describing a font's encoding and spacing behavior.
class FontProperties {
 public:
  /// Character set supported by the font.
  enum Charset {
    CHARSET_ASCII,       // 7-bit
    CHARSET_UNICODE_BMP  // 16-bit codes, usually UTF-encoded.
  };

  /// Spacing behavior for glyph advances.
  enum Spacing { SPACING_PROPORTIONAL, SPACING_MONOSPACE };

  /// Smoothing/anti-aliasing mode.
  enum Smoothing { SMOOTHING_NONE, SMOOTHING_GRAYSCALE };

  /// Kerning information availability.
  enum Kerning { KERNING_NONE, KERNING_PAIRS };

  FontProperties() = default;
  FontProperties(FontProperties &&) = default;
  FontProperties &operator=(FontProperties &&) = default;

  /// Construct font properties.
  FontProperties(Charset charset, Spacing spacing, Smoothing smoothing,
                 Kerning kerning)
      : charset_(charset),
        spacing_(spacing),
        smoothing_(smoothing),
        kerning_(kerning) {}

  /// Character set supported.
  Charset charset() const { return charset_; }
  /// Spacing behavior.
  Spacing spacing() const { return spacing_; }
  /// Smoothing/anti-aliasing mode.
  Smoothing smoothing() const { return smoothing_; }
  /// Kerning mode.
  Kerning kerning() const { return kerning_; }

 private:
  Charset charset_;
  Spacing spacing_;
  Smoothing smoothing_;
  Kerning kerning_;
};

/// Per-glyph metrics (bounding box and advance).
class GlyphMetrics {
 public:
  /// Construct metrics from FreeType coordinates (Y up).
  GlyphMetrics(int16_t glyphXMin, int16_t glyphYMin, int16_t glyphXMax,
               int16_t glyphYMax, int advance)
      : bbox_(glyphXMin, -glyphYMax, glyphXMax, -glyphYMin),
        advance_(advance) {}

  GlyphMetrics() = default;
  GlyphMetrics(const GlyphMetrics &) = default;
  GlyphMetrics(GlyphMetrics &&) = default;
  GlyphMetrics &operator=(GlyphMetrics &&) = default;
  GlyphMetrics &operator=(const GlyphMetrics &) = default;

  /// Bounding box in screen coordinates (Y down).
  const Box &screen_extents() const { return bbox_; }

  /// Bounding box in FreeType coordinates (Y up).

  int glyphXMin() const { return bbox_.xMin(); }
  int glyphXMax() const { return bbox_.xMax(); }
  int glyphYMin() const { return -bbox_.yMax(); }
  int glyphYMax() const { return -bbox_.yMin(); }

  /// Left side bearing.
  int bearingX() const { return bbox_.xMin(); }
  /// Top side bearing.
  int bearingY() const { return -bbox_.yMin(); }
  /// Right side bearing.
  int rsb() const { return advance() - (bbox_.xMax() + 1); }
  /// Left side bearing (alias).
  int lsb() const { return bbox_.xMin(); }
  /// Glyph width.
  int width() const { return bbox_.width(); }
  /// Glyph height.
  int height() const { return bbox_.height(); }

  /// Advance in pixels.
  int advance() const { return advance_; }

/// Abstract font interface.
class Font {
  Box bbox_;  // In screen coordinates; i.e. positive Y down.
  /// Return font metrics.
  int advance_;
  /// Return font properties.
};

  /// Retrieve glyph metrics for a code point and layout.
class Font {
 public:
  const FontMetrics &metrics() const { return metrics_; }
  /// Draw a UTF-8 string horizontally using a string view.
  ///
  /// See https://www.freetype.org/freetype2/docs/glyphs/glyphs-3.html

  virtual bool getGlyphMetrics(char32_t code, FontLayout layout,
                               GlyphMetrics *result) const = 0;

  // See https://www.freetype.org/freetype2/docs/glyphs/glyphs-3.html
  /// Draw a UTF-8 string horizontally.

  void drawHorizontalString(const Surface &s, roo::string_view text,
                            Color color) const {
  /// Return metrics of the specified string as if it were a single glyph.
  }

  virtual void drawHorizontalString(const Surface &s, const char *utf8_data,
                                    uint32_t size, Color color) const = 0;
  /// Return metrics of the specified UTF-8 string as if it were a single glyph.

  // Returns metrics of the specified string, as if it was a single glyph.
  GlyphMetrics getHorizontalStringMetrics(roo::string_view text) const {
  /// Return metrics for consecutive glyphs in the UTF-8 string.
  ///
  /// Glyphs may overlap due to kerning. The number of glyphs written is
  /// limited by `max_count`. Returns the number of glyphs measured, which
  /// may be smaller than `max_count` if the input string is shorter.
  // Returns metrics of the consecutive glyphs of the specified string,
  // beginning at the specified `offset`, and stores them in the `result`. The
  // glyphs may be overlapping due to kerning. The number of glyphs in the
  // result is limited by `max_count`. Returns the number of glyphs actually
  // measured, which may be smaller than `max_count` if the input string is
  // shorter.
  uint32_t getHorizontalStringGlyphMetrics(roo::string_view text,
                                           GlyphMetrics *result,
                                           uint32_t offset,
                                           uint32_t max_count) const {
    return getHorizontalStringGlyphMetrics(text.data(), text.size(), result,
                                           offset, max_count);
  }

  /// Return metrics for consecutive glyphs in the UTF-8 string.
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
