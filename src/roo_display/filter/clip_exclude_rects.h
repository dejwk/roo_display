#pragma once

#include <limits>

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
  inline bool contains(int16_t x, int16_t y, size_t* same_count) const {
    int16_t next_x_min = std::numeric_limits<int16_t>::max();
    for (const Box* box = begin_; box != end_; ++box) {
      if (box->yMin() > y || box->yMax() < y) continue;
      if (box->xMin() <= x && box->xMax() >= x) {
        if (same_count != nullptr) {
          *same_count = static_cast<size_t>(box->xMax() - x + 1);
        }
        return true;
      }
      if (box->xMin() > x && box->xMin() < next_x_min) {
        next_x_min = box->xMin();
      }
    }
    if (same_count != nullptr) {
      *same_count = next_x_min == std::numeric_limits<int16_t>::max()
                        ? std::numeric_limits<size_t>::max()
                        : static_cast<size_t>(next_x_min - x);
    }
    return false;
  }

  inline bool contains(int16_t x, int16_t y) const {
    return contains(x, y, nullptr);
  }

  /// Return whether the union intersects a rectangle.
  inline bool intersects(const Box& rect) const {
    for (const Box* box = begin_; box != end_; ++box) {
      if (box->intersects(rect)) return true;
    }
    return false;
  }

  /// Return whether any single rectangle in the union fully contains `rect`.
  /// Note that this is a stronger condition than requiring the union as a whole
  /// to contain `rect`, since the union can consist of adjacent rectangles that
  /// cover `rect` but none of them individually contain it.
  inline bool contains(const Box& rect) const {
    for (const Box* box = begin_; box != end_; ++box) {
      if (box->contains(rect)) return true;
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
        cursor_y_(0),
        capabilities_(output.getCapabilities().supportsBlending(),
                      /*supports_blit_copy=*/false) {}

  virtual ~RectUnionFilter() {}

  /// Replace the underlying output.
  void setOutput(DisplayOutput& output) {
    output_ = &output;
    capabilities_ = Capabilities(output.getCapabilities().supportsBlending(),
                                 /*supports_blit_copy=*/false);
    resetPartialRunState();
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    address_window_ = Box(x0, y0, x1, y1);
    blending_mode_ = mode;
    cursor_x_ = x0;
    cursor_y_ = y0;
    resetPartialRunState();
    if (!exclusion_->intersects(address_window_)) {
      addr_excluded_ = kNone;
      output_->setAddress(x0, y0, x1, y1, mode);
    } else if (exclusion_->contains(address_window_)) {
      addr_excluded_ = kFull;
    } else {
      addr_excluded_ = kPartial;
    }
  }

  void write(Color* color, uint32_t pixel_count) override {
    if (addr_excluded_ == kNone) {
      output_->write(color, pixel_count);
      advanceCursor(pixel_count);
      return;
    }
    if (addr_excluded_ == kFull) {
      // Don't bother to advance cursor, since we're not using it anyway.
      return;
    }
    // Slow path: cache same-answer runs and forward visible runs through the
    // wrapped address-window interface.
    uint32_t i = 0;
    while (i < pixel_count) {
      primePartialRun();
      uint32_t run = pixel_count - i;
      if (run > partial_run_remaining_) run = partial_run_remaining_;

      if (!partial_run_excluded_) {
        output_->write(color + i, run);
      }
      consumePartialRun(run);
      i += run;
    }
  }

  void fill(Color color, uint32_t pixel_count) override {
    if (addr_excluded_ == kNone) {
      output_->fill(color, pixel_count);
      advanceCursor(pixel_count);
      return;
    }
    if (addr_excluded_ == kFull) {
      // Don't bother to advance cursor, since we're not using it anyway.
      return;
    }
    // Slow path: cache same-answer runs and forward visible runs through the
    // wrapped address-window interface.
    uint32_t i = 0;
    while (i < pixel_count) {
      primePartialRun();
      uint32_t run = pixel_count - i;
      if (run > partial_run_remaining_) run = partial_run_remaining_;

      if (!partial_run_excluded_) {
        output_->fill(color, run);
      }
      consumePartialRun(run);
      i += run;
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

  const Capabilities& getCapabilities() const override { return capabilities_; }

  void drawDirectRect(const roo::byte* data, size_t row_width_bytes,
                      int16_t src_x0, int16_t src_y0, int16_t src_x1,
                      int16_t src_y1, int16_t dst_x0, int16_t dst_y0) override {
    DisplayOutput::drawDirectRect(data, row_width_bytes, src_x0, src_y0, src_x1,
                                  src_y1, dst_x0, dst_y0);
  }

 private:
  void resetPartialRunState() {
    partial_run_remaining_ = 0;
    partial_run_excluded_ = false;
    partial_row_window_open_ = false;
  }

  uint32_t rowRemaining() const {
    return static_cast<uint32_t>(address_window_.xMax() - cursor_x_ + 1);
  }

  void primePartialRun() {
    uint32_t row_remaining = rowRemaining();
    if (partial_run_remaining_ == 0) {
      size_t same_count = 1;
      partial_run_excluded_ =
          exclusion_->contains(cursor_x_, cursor_y_, &same_count);
      partial_run_remaining_ = row_remaining;
      if (same_count < partial_run_remaining_) {
        partial_run_remaining_ = static_cast<uint32_t>(same_count);
      }
      if (partial_run_excluded_) {
        partial_row_window_open_ = false;
      }
    }
    if (!partial_run_excluded_ && !partial_row_window_open_) {
      output_->setAddress(cursor_x_, cursor_y_, address_window_.xMax(),
                          cursor_y_, blending_mode_);
      partial_row_window_open_ = true;
    }
  }

  void consumePartialRun(uint32_t pixel_count) {
    advanceCursor(pixel_count);
    partial_run_remaining_ -= pixel_count;
    if (cursor_x_ == address_window_.xMin()) {
      partial_run_remaining_ = 0;
      partial_row_window_open_ = false;
    }
  }

  void writeRect(Color color, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                 int mask_idx, BufferedRectWriter* writer) {
    Box rect(x0, y0, x1, y1);
    while (mask_idx < (int)exclusion_->size() &&
           !exclusion_->at(mask_idx).intersects(rect)) {
      ++mask_idx;
    }
    if (mask_idx == (int)exclusion_->size()) {
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
      while (mask_idx < (int)exclusion_->size() &&
             !exclusion_->at(mask_idx).intersects(rect)) {
        ++mask_idx;
      }
    }
    if (mask_idx == (int)exclusion_->size()) {
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

  void advanceCursor(uint32_t pixel_count) {
    int16_t w = address_window_.xMax() - address_window_.xMin() + 1;
    int32_t pos = (cursor_y_ - address_window_.yMin()) * (int32_t)w +
                  (cursor_x_ - address_window_.xMin()) + pixel_count;
    cursor_y_ = address_window_.yMin() + pos / w;
    cursor_x_ = address_window_.xMin() + pos % w;
  }

  enum AddrExclusion : uint8_t { kNone, kPartial, kFull };

  DisplayOutput* output_;
  const RectUnion* exclusion_;
  Box address_window_;
  BlendingMode blending_mode_;
  int16_t cursor_x_;
  int16_t cursor_y_;
  uint32_t partial_run_remaining_ = 0;
  bool partial_run_excluded_ = false;
  bool partial_row_window_open_ = false;
  AddrExclusion addr_excluded_ = kNone;
  Capabilities capabilities_;
};

}  // namespace roo_display