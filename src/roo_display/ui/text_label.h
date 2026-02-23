#pragma once

#include <inttypes.h>

#include <string>

#include "roo_backport/string_view.h"
#include "roo_display/core/drawable.h"
#include "roo_display/filter/transformation.h"
#include "roo_display/font/font.h"
#include "roo_display/ui/tile.h"

namespace roo_display {

/// Single-line, single-color text label.
///
/// Extents have height based on the font size, not the specific text. The
/// origin is at the text baseline, and horizontally affected by the text
/// bearing.
///
/// When using smooth fonts, you should generally specify `bgcolor` when
/// drawing text labels, even over pre-filled solid backgrounds. This is
/// because many device drivers don't support alpha blending, which is required
/// for smooth font edges. Exceptions: offscreen/double-buffered devices, or
/// drivers that do support alpha blending, when you want to preserve the
/// existing non-solid background.
///
/// For changing text fields, wrap `TextLabel` in a `Tile` to reduce flicker.
///
/// See also `StringViewLabel`.
class TextLabel : public Drawable {
 public:
  /// Construct from a string-like value (moves into internal storage).
  template <typename String>
  TextLabel(const String& label, const Font& font, Color color,
            FillMode fill_mode = kFillVisible)
      : TextLabel(std::string(std::move(label)), font, color, fill_mode) {}

  /// Construct from an owned string.
  TextLabel(std::string label, const Font& font, Color color,
            FillMode fill_mode = kFillVisible)
      : font_(&font),
        label_(std::move(label)),
        color_(color),
        fill_mode_(fill_mode),
        metrics_(font.getHorizontalStringMetrics(label_)) {}

  void drawTo(const Surface& s) const override {
    Surface news = s;
    if (fill_mode() == kFillRectangle) {
      news.set_fill_mode(kFillRectangle);
    }
    font().drawHorizontalString(news, label(), color());
  }

  Box extents() const override { return metrics_.screen_extents(); }

  Box anchorExtents() const override {
    return Box(0, -font().metrics().ascent() - font().metrics().linegap(),
               metrics_.advance() - 1, -font().metrics().descent());
  }

  /// Return the font used by the label.
  const Font& font() const { return *font_; }
  /// Return cached string metrics.
  const GlyphMetrics& metrics() const { return metrics_; }
  /// Return the label text.
  const std::string& label() const { return label_; }
  /// Return the label color.
  const Color color() const { return color_; }
  /// Return the fill mode.
  const FillMode fill_mode() const { return fill_mode_; }

  /// Set the label color.
  void setColor(Color color) { color_ = color; }
  /// Set the fill mode.
  void setFillMode(FillMode fill_mode) { fill_mode_ = fill_mode; }

 private:
  const Font* font_;
  std::string label_;
  Color color_;
  FillMode fill_mode_;
  GlyphMetrics metrics_;
};

/// Single-line, single-color label with tight extents.
///
/// Extents are the smallest rectangle that fits all rendered pixels. The
/// origin is at the text baseline and horizontally affected by the bearing.
class ClippedTextLabel : public TextLabel {
 public:
  using TextLabel::TextLabel;

  void drawTo(const Surface& s) const override {
    Surface news(s);
    if (fill_mode() == kFillRectangle) {
      news.set_fill_mode(kFillRectangle);
    }
    // news.clipToExtents(metrics().screen_extents());
    font().drawHorizontalString(news, label(), color());
  }

  Box anchorExtents() const override { return metrics().screen_extents(); }
};

/// Like `TextLabel`, but does not own the text content.
///
/// Uses no dynamic allocation. Ideal for string literals or temporary labels.
class StringViewLabel : public Drawable {
 public:
  /// Construct from a string-like value without copying.
  template <typename String>
  StringViewLabel(String& label, const Font& font, const Color color,
                  FillMode fill_mode = kFillVisible)
      : StringViewLabel(roo::string_view(std::move(label)), font, color,
                        fill_mode) {}

  /// Construct from a `string_view`.
  StringViewLabel(roo::string_view label, const Font& font, Color color,
                  FillMode fill_mode = kFillVisible)
      : font_(&font),
        label_(std::move(label)),
        color_(color),
        fill_mode_(fill_mode),
        metrics_(font.getHorizontalStringMetrics(label)) {}

  void drawTo(const Surface& s) const override {
    Surface news = s;
    if (fill_mode() == kFillRectangle) {
      news.set_fill_mode(kFillRectangle);
    }
    font().drawHorizontalString(news, label(), color());
  }

  Box extents() const override { return metrics_.screen_extents(); }

  Box anchorExtents() const override {
    return Box(0, -font().metrics().ascent() - font().metrics().linegap(),
               metrics_.advance() - 1, -font().metrics().descent());
  }

  /// Return the font used by the label.
  const Font& font() const { return *font_; }
  /// Return cached string metrics.
  const GlyphMetrics& metrics() const { return metrics_; }
  /// Return the label text view.
  const roo::string_view label() const { return label_; }
  /// Return the label color.
  const Color color() const { return color_; }
  /// Return the fill mode.
  const FillMode fill_mode() const { return fill_mode_; }

  /// Set the label color.
  void setColor(Color color) { color_ = color; }
  /// Set the fill mode.
  void setFillMode(FillMode fill_mode) { fill_mode_ = fill_mode; }

 private:
  const Font* font_;
  roo::string_view label_;
  Color color_;
  FillMode fill_mode_;
  GlyphMetrics metrics_;
};

/// Like `ClippedTextLabel`, but does not own the text content.
///
/// Uses no dynamic allocation. Ideal for string literals or temporary labels.
class ClippedStringViewLabel : public StringViewLabel {
 public:
  using StringViewLabel::StringViewLabel;

  void drawTo(const Surface& s) const override {
    Surface news(s);
    if (fill_mode() == kFillRectangle) {
      news.set_fill_mode(kFillRectangle);
    }
    font().drawHorizontalString(news, label(), color());
  }

  Box extents() const override { return metrics().screen_extents(); }
};

}  // namespace roo_display