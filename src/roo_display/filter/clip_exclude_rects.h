#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"

namespace roo_display {

/// Union of rectangles used as an exclusion mask.
class RectUnion {
 public:
  /// Construct a rect union from a range.
  RectUnion(const Box* begin, const Box* end) : begin_(begin), end_(end) {}

  /// Reset to a new range.
  void reset(const Box* begin, const Box* end) {
    begin_ = begin;
    end_ = end;
  }

  /// Return whether the union contains a point.
  inline bool contains(int16_t x, int16_t y) const {
    for (const Box* box = begin_; box != end_; ++box) {
      if (box->contains(x, y)) return true;
    }
    return false;
  }

  /// Return whether the union intersects a rectangle.
  inline bool intersects(const Box& rect) const {
    for (const Box* box = begin_; box != end_; ++box) {
      if (box->intersects(rect)) return true;
    }
    return false;
  }

  /// Return the number of rectangles in the union.
  size_t size() const { return end_ - begin_; }

  /// Return the rectangle at index `idx`.
  const Box& at(int idx) const { return *(begin_ + idx); }

 private:
  const Box* begin_;
  const Box* end_;
};

/// Filtering device that excludes a union of rectangles.
class RectUnionFilter : public DisplayOutput {
 public:
  /// Create a filter with an exclusion union.
  RectUnionFilter(DisplayOutput& output, const RectUnion* exclusion)
      : output_(&output),
        exclusion_(exclusion),
        address_window_(0, 0, 0, 0),
        cursor_x_(0),
        cursor_y_(0) {}

  virtual ~RectUnionFilter() {}

  /// Replace the underlying output.
  void setOutput(DisplayOutput& output) { output_ = &output; }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    address_window_ = Box(x0, y0, x1, y1);
    blending_mode_ = mode;
    cursor_x_ = x0;
    cursor_y_ = y0;
  }

  void write(Color* color, uint32_t pixel_count) override {
    // Naive implementation, for now.
    uint32_t i = 0;
    BufferedPixelWriter writer(*output_, blending_mode_);
    while (i < pixel_count) {
      if (!exclusion_->contains(cursor_x_, cursor_y_)) {
        writer.writePixel(cursor_x_, cursor_y_, color[i]);
      }
      if (++cursor_x_ > address_window_.xMax()) {
        ++cursor_y_;
        cursor_x_ = address_window_.xMin();
      }
      ++i;
    }
  }

  void fill(Color color, uint32_t pixel_count) override {
    uint32_t i = 0;
    BufferedPixelFiller filler(*output_, color, blending_mode_);
    while (i < pixel_count) {
      if (!exclusion_->contains(cursor_x_, cursor_y_)) {
        filler.fillPixel(cursor_x_, cursor_y_);
      }
      if (++cursor_x_ > address_window_.xMax()) {
        ++cursor_y_;
        cursor_x_ = address_window_.xMin();
      }
      ++i;
    }
  }

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    BufferedRectWriter writer(*output_, mode);
    while (count-- > 0) {
      writeRect(*color++, *x0++, *y0++, *x1++, *y1++, 0, &writer);
    }
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    BufferedRectFiller filler(*output_, color, mode);
    while (count-- > 0) {
      fillRect(*x0++, *y0++, *x1++, *y1++, 0, &filler);
    }
  }

  void writePixels(BlendingMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    int16_t* x_out = x;
    int16_t* y_out = y;
    Color* color_out = color;
    uint16_t new_pixel_count = 0;
    for (uint16_t i = 0; i < pixel_count; ++i) {
      if (!exclusion_->contains(x[i], y[i])) {
        *x_out++ = x[i];
        *y_out++ = y[i];
        *color_out++ = color[i];
        new_pixel_count++;
      }
    }
    if (new_pixel_count > 0) {
      output_->writePixels(mode, color, x, y, new_pixel_count);
    }
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    int16_t* x_out = x;
    int16_t* y_out = y;
    uint16_t new_pixel_count = 0;
    for (uint16_t i = 0; i < pixel_count; ++i) {
      if (!exclusion_->contains(x[i], y[i])) {
        *x_out++ = x[i];
        *y_out++ = y[i];
        new_pixel_count++;
      }
    }
    if (new_pixel_count > 0) {
      output_->fillPixels(mode, color, x, y, new_pixel_count);
    }
  }

  const ColorFormat& getColorFormat() const override {
    return output_->getColorFormat();
  }

  void drawDirectRect(const roo::byte* data, size_t row_width_bytes,
                      int16_t src_x0, int16_t src_y0, int16_t src_x1,
                      int16_t src_y1, int16_t dst_x0, int16_t dst_y0) override {
    DisplayOutput::drawDirectRect(data, row_width_bytes, src_x0, src_y0, src_x1,
                                  src_y1, dst_x0, dst_y0);
  }

 private:
  void writeRect(Color color, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                 int mask_idx, BufferedRectWriter* writer) {
    Box rect(x0, y0, x1, y1);
    while (mask_idx < exclusion_->size() &&
           !exclusion_->at(mask_idx).intersects(rect)) {
      ++mask_idx;
    }
    if (mask_idx == exclusion_->size()) {
      writer->writeRect(x0, y0, x1, y1, color);
      return;
    }
    Box intruder = Box::Intersect(exclusion_->at(mask_idx), rect);
    if (intruder.yMin() > y0) {
      writeRect(color, x0, y0, x1, intruder.yMin() - 1, mask_idx + 1, writer);
      y0 = intruder.yMin();
    }
    if (intruder.xMin() > x0) {
      writeRect(color, x0, y0, intruder.xMin() - 1, intruder.yMax(),
                mask_idx + 1, writer);
    }
    if (intruder.xMax() < x1) {
      writeRect(color, intruder.xMax() + 1, y0, x1, intruder.yMax(),
                mask_idx + 1, writer);
    }
    if (intruder.yMax() < y1) {
      writeRect(color, x0, intruder.yMax() + 1, x1, y1, mask_idx + 1, writer);
    }
  }

  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int mask_idx,
                BufferedRectFiller* filler) {
    {
      Box rect(x0, y0, x1, y1);
      while (mask_idx < exclusion_->size() &&
             !exclusion_->at(mask_idx).intersects(rect)) {
        ++mask_idx;
      }
    }
    if (mask_idx == exclusion_->size()) {
      filler->fillRect(x0, y0, x1, y1);
      return;
    }
    Box intruder =
        Box::Intersect(exclusion_->at(mask_idx), Box(x0, y0, x1, y1));
    if (intruder.yMin() > y0) {
      fillRect(x0, y0, x1, intruder.yMin() - 1, mask_idx + 1, filler);
      y0 = intruder.yMin();
    }
    if (intruder.xMin() > x0) {
      fillRect(x0, y0, intruder.xMin() - 1, intruder.yMax(), mask_idx + 1,
               filler);
    }
    if (intruder.xMax() < x1) {
      fillRect(intruder.xMax() + 1, y0, x1, intruder.yMax(), mask_idx + 1,
               filler);
    }
    if (intruder.yMax() < y1) {
      fillRect(x0, intruder.yMax() + 1, x1, y1, mask_idx + 1, filler);
    }
  }

  DisplayOutput* output_;
  const RectUnion* exclusion_;
  Box address_window_;
  BlendingMode blending_mode_;
  int16_t cursor_x_;
  int16_t cursor_y_;
};

}  // namespace roo_display