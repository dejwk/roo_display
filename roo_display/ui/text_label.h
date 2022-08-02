#pragma once

#include <inttypes.h>

#include <string>

#include "roo_display/core/drawable.h"
#include "roo_display/filter/transformed.h"
#include "roo_display/font/font.h"
#include "roo_display/shape/basic_shapes.h"
#include "roo_display/ui/tile.h"

namespace roo_display {

// A single-line, single-colored. text label, with extents that have height
// based only on the font size, but not on the specific text. The origin is at
// the text's reference point; i.e. at the text baseline, and horizontally
// affected by the text's bearing.
//
// When using smooth fonts, you should generally specify bgcolor when drawing
// text labels, even over pre-filled solid backgrounds. This is because device
// drivers often do not support alpha-blending, which is required for properly
// rendering smooth font edges. The exception is when you draw to Offscreen, or
// double-buffered devices, or devices whose drivers that do actually support
// alpha-blending, AND when you want to preserve the existing non-solid
// background.
//
// When using for text fields whose values may be changing on the screen,
// you want to enclose TextLabel in a Tile, which supports flicker-less
// redrawing.
//
// See also StringViewLabel.
class TextLabel : public Drawable {
 public:
  template <typename String>
  TextLabel(const Font& font, const String& label, Color color,
            FillMode fill_mode = FILL_MODE_VISIBLE)
      : TextLabel(font, std::string(std::move(label)), color, fill_mode) {}

  TextLabel(const Font& font, std::string label, Color color,
            FillMode fill_mode = FILL_MODE_VISIBLE)
      : font_(&font),
        label_(std::move(label)),
        color_(color),
        fill_mode_(fill_mode),
        metrics_(font.getHorizontalStringMetrics(label)) {}

  void drawTo(const Surface& s) const override {
    Surface news = s;
    if (fill_mode() == FILL_MODE_RECTANGLE) {
      news.set_fill_mode(FILL_MODE_RECTANGLE);
    }
    font().drawHorizontalString(news, label(), color());
  }

  Box extents() const override { return metrics_.screen_extents(); }

  Box anchorExtents() const override {
    return Box(0, -font().metrics().ascent(), metrics_.advance() - 1,
               -font().metrics().descent());
  }

  const Font& font() const { return *font_; }
  const GlyphMetrics& metrics() const { return metrics_; }
  const std::string& label() const { return label_; }
  const Color color() const { return color_; }
  const FillMode fill_mode() const { return fill_mode_; }

  void setColor(Color color) { color_ = color; }
  void setFillMode(FillMode fill_mode) { fill_mode_ = fill_mode; }

 private:
  const Font* font_;
  std::string label_;
  Color color_;
  FillMode fill_mode_;
  GlyphMetrics metrics_;
};

// A single-line, single-colored. text label, with extents equal to the
// bounding box of the rendered text; i.e. the smallest rectangle that fits
// all pixels. The origin is at the text's reference point; i.e. at the text
// baseline, and horizontally affected by the text's bearing.
class ClippedTextLabel : public TextLabel {
 public:
  using TextLabel::TextLabel;

  void drawTo(const Surface& s) const override {
    Surface news(s);
    if (fill_mode() == FILL_MODE_RECTANGLE) {
      news.set_fill_mode(FILL_MODE_RECTANGLE);
    }
    // news.clipToExtents(metrics().screen_extents());
    font().drawHorizontalString(news, label(), color());
  }

  Box anchorExtents() const override { return metrics().screen_extents(); }
};

// Similar to TextLabel, but the text content is not owned. Does not use any
// dynamic memory allocation. Perfect when the labels are const literals, or if
// the label is created temporarily during drawing.
class StringViewLabel : public Drawable {
 public:
  template <typename String>
  StringViewLabel(const Font& font, const String& label, Color color,
                  FillMode fill_mode = FILL_MODE_VISIBLE)
      : StringViewLabel(font, StringView(std::move(label)), color, fill_mode) {}

  StringViewLabel(const Font& font, StringView label, Color color,
                  FillMode fill_mode = FILL_MODE_VISIBLE)
      : font_(&font),
        label_(std::move(label)),
        color_(color),
        fill_mode_(fill_mode),
        metrics_(font.getHorizontalStringMetrics(label)) {}

  void drawTo(const Surface& s) const override {
    Surface news = s;
    if (fill_mode() == FILL_MODE_RECTANGLE) {
      news.set_fill_mode(FILL_MODE_RECTANGLE);
    }
    font().drawHorizontalString(news, label(), color());
  }

  Box extents() const override { return metrics_.screen_extents(); }

  Box anchorExtents() const override {
    return Box(0, -font().metrics().ascent(), metrics_.advance() - 1,
               -font().metrics().descent());
  }

  const Font& font() const { return *font_; }
  const GlyphMetrics& metrics() const { return metrics_; }
  const StringView label() const { return label_; }
  const Color color() const { return color_; }
  const FillMode fill_mode() const { return fill_mode_; }

  void setColor(Color color) { color_ = color; }
  void setFillMode(FillMode fill_mode) { fill_mode_ = fill_mode; }

 private:
  const Font* font_;
  StringView label_;
  Color color_;
  FillMode fill_mode_;
  GlyphMetrics metrics_;
};

// Similar to ClippedTextLabel, but the text content is not owned. Does not use
// any dynamic memory allocation. Perfect when the labels are const literals, or
// if the label is created temporarily during drawing.
class ClippedStringViewLabel : public StringViewLabel {
 public:
  using StringViewLabel::StringViewLabel;

  void drawTo(const Surface& s) const override {
    Surface news(s);
    if (fill_mode() == FILL_MODE_RECTANGLE) {
      news.set_fill_mode(FILL_MODE_RECTANGLE);
    }
    font().drawHorizontalString(news, label(), color());
  }

  Box extents() const override { return metrics().screen_extents(); }
};

}  // namespace roo_display