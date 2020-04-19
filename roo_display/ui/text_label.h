#pragma once

#include <inttypes.h>

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
        metrics_(font_->getHorizontalStringMetrics(
            (const uint8_t*)label_.c_str(), label_.length())),
        extents_(metrics_.glyphXMin(), -font_->metrics().glyphYMax(),
                 metrics_.glyphXMax(), -font_->metrics().glyphYMin()) {}

  void drawTo(const Surface& s) const override {
    Surface news(s);
    if (fill_mode_ == FILL_MODE_RECTANGLE) {
      news.fill_mode = FILL_MODE_RECTANGLE;
    }
    font_->drawHorizontalString(news, (const uint8_t*)label_.c_str(),
                                label_.length(), color_);
  }

  Box extents() const override { return extents_; }

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
  Box extents_;
};

// A single-line, single-colored. text label, with extents equal to the
// bounding box of the rendered text; i.e. the smallest rectangle that fits
// all pixels. The origin is at the text's reference point; i.e. at the text
// baseline, and horizontally affected by the text's bearing.
class ClippedTextLabel : public Drawable {
 public:
  template <typename String>
  ClippedTextLabel(const Font& font, const String& label, Color color)
      : ClippedTextLabel(font, std::string(std::move(label)), color) {}

  ClippedTextLabel(const Font& font, std::string label, Color color)
      : font_(&font),
        label_(std::move(label)),
        color_(color),
        metrics_(font_->getHorizontalStringMetrics(
            (const uint8_t*)label_.c_str(), label_.length())) {}

  void drawTo(const Surface& s) const override {
    Surface news(s);
    news.clipToExtents(extents());
    font_->drawHorizontalString(news, (const uint8_t*)label_.c_str(),
                                label_.length(), color_);
  }

  Box extents() const override { return metrics_.screen_extents(); }

  const Font& font() const { return *font_; }
  const GlyphMetrics& metrics() const { return metrics_; }
  const std::string& label() const { return label_; }
  const Color color() const { return color_; }

 private:
  const Font* font_;
  std::string label_;
  Color color_;
  GlyphMetrics metrics_;
  Box extents_;
};

}  // namespace roo_display