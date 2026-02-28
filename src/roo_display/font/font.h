#pragma once

#include <assert.h>
#include <inttypes.h>

#include "roo_backport/string_view.h"
#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"

#include "roo_logging.h"

namespace roo_display {

/// Glyph layout direction.
enum class FontLayout { kHorizontal, kVertical };

[[deprecated("Use `FontLayout::kHorizontal` instead.")]]
constexpr FontLayout FONT_LAYOUT_HORIZONTAL = FontLayout::kHorizontal;
[[deprecated("Use `FontLayout::kVertical` instead.")]]
constexpr FontLayout FONT_LAYOUT_VERTICAL = FontLayout::kVertical;

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
  enum class Charset {
    kAscii,       // 7-bit
    kUnicodeBmp  // 16-bit codes, usually UTF-encoded.
  };

  /// Spacing behavior for glyph advances.
  enum class Spacing { kProportional, kMonospace };

  /// Smoothing/anti-aliasing mode.
  enum class Smoothing { kNone, kGrayscale };

  /// Kerning information availability.
  enum class Kerning { kNone, kPairs };

  [[deprecated("Use `FontProperties::Charset::kAscii` instead.")]]
  static constexpr Charset CHARSET_ASCII = Charset::kAscii;
  [[deprecated("Use `FontProperties::Charset::kUnicodeBmp` instead.")]]
  static constexpr Charset CHARSET_UNICODE_BMP = Charset::kUnicodeBmp;

  [[deprecated("Use `FontProperties::Spacing::kProportional` instead.")]]
  static constexpr Spacing SPACING_PROPORTIONAL = Spacing::kProportional;
  [[deprecated("Use `FontProperties::Spacing::kMonospace` instead.")]]
  static constexpr Spacing SPACING_MONOSPACE = Spacing::kMonospace;

  [[deprecated("Use `FontProperties::Smoothing::kNone` instead.")]]
  static constexpr Smoothing SMOOTHING_NONE = Smoothing::kNone;
  [[deprecated("Use `FontProperties::Smoothing::kGrayscale` instead.")]]
  static constexpr Smoothing SMOOTHING_GRAYSCALE = Smoothing::kGrayscale;

  [[deprecated("Use `FontProperties::Kerning::kNone` instead.")]]
  static constexpr Kerning KERNING_NONE = Kerning::kNone;
  [[deprecated("Use `FontProperties::Kerning::kPairs` instead.")]]
  static constexpr Kerning KERNING_PAIRS = Kerning::kPairs;

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

 private:
  Box bbox_;  // In screen coordinates; i.e. positive Y down.
  int advance_;
};

/// Abstract font interface.
class Font {
 public:
  /// Return font metrics.
  const FontMetrics &metrics() const { return metrics_; }
  /// Return font properties.
  const FontProperties &properties() const { return properties_; }

  /// Retrieve glyph metrics for a code point and layout.
  virtual bool getGlyphMetrics(char32_t code, FontLayout layout,
                               GlyphMetrics *result) const = 0;

  /// Draw a UTF-8 string horizontally using a string view.
  ///
  /// See https://www.freetype.org/freetype2/docs/glyphs/glyphs-3.html
  void drawHorizontalString(const Surface &s, roo::string_view text,
                            Color color) const {
    drawHorizontalString(s, text.data(), text.size(), color);
  }

  /// Draw a UTF-8 string horizontally.
  virtual void drawHorizontalString(const Surface &s, const char *utf8_data,
                                    uint32_t size, Color color) const = 0;

  /// Return metrics of the specified UTF-8 string as if it were a single
  /// glyph.
  GlyphMetrics getHorizontalStringMetrics(roo::string_view text) const {
    return getHorizontalStringMetrics(text.data(), text.size());
  }

  /// Return metrics of the specified UTF-8 string as if it were a single
  /// glyph.
  virtual GlyphMetrics getHorizontalStringMetrics(const char *utf8_data,
                                                  uint32_t size) const = 0;

  /// Return metrics for consecutive glyphs in the UTF-8 string.
  ///
  /// Glyphs may overlap due to kerning. The number of glyphs written is
  /// limited by `max_count`. Returns the number of glyphs measured, which
  /// may be smaller than `max_count` if the input string is shorter.
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

roo_logging::Stream& operator<<(roo_logging::Stream& stream, FontLayout layout);
roo_logging::Stream& operator<<(roo_logging::Stream& stream, FontProperties::Charset charset);
roo_logging::Stream& operator<<(roo_logging::Stream& stream, FontProperties::Spacing spacing);
roo_logging::Stream& operator<<(roo_logging::Stream& stream, FontProperties::Smoothing smoothing);
roo_logging::Stream& operator<<(roo_logging::Stream& stream, FontProperties::Kerning kerning);

}  // namespace roo_display
