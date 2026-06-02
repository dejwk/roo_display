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

  /// Return a best-effort lower bound on visible pixels in raster order
  /// starting at `(bounds.xMin(), y)`.
  ///
  /// The caller must only use this when the current row is already known to be
  /// visible from `bounds.xMin()` through `bounds.xMax()`. The returned count
  /// includes that full row plus any later fully visible rows and the visible
  /// prefix of the first row where an exclusion appears.
  inline uint32_t visiblePixelsFromRowStart(const Box& bounds,
                                            int16_t y) const {
    const int16_t x0 = bounds.xMin();
    const int16_t x1 = bounds.xMax();
    const uint32_t width = static_cast<uint32_t>(bounds.width());
    int32_t next_blocked_y = static_cast<int32_t>(bounds.yMax()) + 1;
    for (const Box* box = begin_; box != end_; ++box) {
      if (box->xMin() > x1 || box->xMax() < x0) continue;
      if (box->yMax() < y || box->yMin() > bounds.yMax()) continue;
      int32_t candidate_y = box->yMin();
      if (candidate_y < y) candidate_y = y;
      if (candidate_y < next_blocked_y) {
        next_blocked_y = candidate_y;
      }
    }
    if (next_blocked_y > bounds.yMax()) {
      return width * static_cast<uint32_t>(bounds.yMax() - y + 1);
    }
    uint32_t pixels = width * static_cast<uint32_t>(next_blocked_y - y);
    size_t same_count = 0;
    if (!contains(x0, static_cast<int16_t>(next_blocked_y), &same_count)) {
      if (same_count > width) same_count = width;
      pixels += static_cast<uint32_t>(same_count);
    }
    return pixels;
  }

  /// Return a best-effort lower bound on excluded pixels in raster order
  /// starting at `(bounds.xMin(), y)`.
  ///
  /// The caller must only use this when the current row is already known to be
  /// excluded from `bounds.xMin()` through `bounds.xMax()`. The returned count
  /// includes every full-width covered row proven by a single containing box,
  /// plus the excluded prefix of the following row when that state continues.
  inline uint32_t excludedPixelsFromRowStart(const Box& bounds,
                                             int16_t y) const {
    const int16_t x0 = bounds.xMin();
    const int16_t x1 = bounds.xMax();
    const uint32_t width = static_cast<uint32_t>(bounds.width());
    int16_t max_full_y = y - 1;
    for (const Box* box = begin_; box != end_; ++box) {
      if (box->xMin() > x0 || box->xMax() < x1) continue;
      if (box->yMin() > y || box->yMax() < y) continue;
      if (box->yMax() > max_full_y) {
        max_full_y = box->yMax();
      }
    }
    if (max_full_y < y) return 0;
    if (max_full_y > bounds.yMax()) max_full_y = bounds.yMax();

    uint32_t pixels = width * static_cast<uint32_t>(max_full_y - y + 1);
    int32_t next_y = static_cast<int32_t>(max_full_y) + 1;
    if (next_y <= bounds.yMax()) {
      size_t same_count = 0;
      if (contains(x0, static_cast<int16_t>(next_y), &same_count)) {
        if (same_count > width) same_count = width;
        pixels += static_cast<uint32_t>(same_count);
      }
    }
    return pixels;
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
    resetRunState();
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    address_window_ = Box(x0, y0, x1, y1);
    blending_mode_ = mode;
    cursor_x_ = x0;
    cursor_y_ = y0;
    resetRunState();
    if (!exclusion_->intersects(address_window_)) {
      run_excluded_ = false;
      run_remaining_ = static_cast<uint32_t>(address_window_.area());
    } else if (exclusion_->contains(address_window_)) {
      run_excluded_ = true;
      run_remaining_ = static_cast<uint32_t>(address_window_.area());
    }
  }

  void write(Color* color, uint32_t pixel_count) override {
    // Route the address window through cached runs. Row starts may promote a
    // row-local answer to a multi-line batch when the same state persists.
    uint32_t i = 0;
    while (i < pixel_count) {
      primeRun();
      uint32_t run = pixel_count - i;
      if (run > run_remaining_) run = run_remaining_;

      if (!run_excluded_) {
        output_->write(color + i, run);
      }
      consumeRun(run);
      i += run;
    }
  }

  void fill(Color color, uint32_t pixel_count) override {
    // Route the address window through cached runs. Row starts may promote a
    // row-local answer to a multi-line batch when the same state persists.
    uint32_t i = 0;
    while (i < pixel_count) {
      primeRun();
      uint32_t run = pixel_count - i;
      if (run > run_remaining_) run = run_remaining_;

      if (!run_excluded_) {
        output_->fill(color, run);
      }
      consumeRun(run);
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
  void resetRunState() {
    run_remaining_ = 0;
    run_excluded_ = false;
    visible_window_open_ = false;
  }

  uint32_t rowRemaining() const {
    return static_cast<uint32_t>(address_window_.xMax() - cursor_x_ + 1);
  }

  /// Ensure a cached run is available and, for visible runs, open the widest
  /// safe address window on the wrapped output.
  void primeRun() {
    uint32_t row_remaining = rowRemaining();
    if (run_remaining_ == 0) {
      size_t same_count = 1;
      run_excluded_ = exclusion_->contains(cursor_x_, cursor_y_, &same_count);
      run_remaining_ = row_remaining;
      if (same_count < run_remaining_) {
        run_remaining_ = static_cast<uint32_t>(same_count);
      } else if (cursor_x_ == address_window_.xMin()) {
        uint32_t batch = run_excluded_ ? exclusion_->excludedPixelsFromRowStart(
                                             address_window_, cursor_y_)
                                       : exclusion_->visiblePixelsFromRowStart(
                                             address_window_, cursor_y_);
        if (batch > run_remaining_) {
          run_remaining_ = batch;
        }
      }
      if (run_excluded_) {
        visible_window_open_ = false;
      }
    }
    if (!run_excluded_ && !visible_window_open_) {
      uint16_t y1 = cursor_y_;
      if (cursor_x_ == address_window_.xMin() &&
          run_remaining_ > row_remaining) {
        uint32_t width = static_cast<uint32_t>(address_window_.width());
        uint32_t rows_spanned = (run_remaining_ + width - 1) / width;
        y1 = static_cast<uint16_t>(cursor_y_ + rows_spanned - 1);
      }
      output_->setAddress(cursor_x_, cursor_y_, address_window_.xMax(), y1,
                          blending_mode_);
      visible_window_open_ = true;
    }
  }

  void consumeRun(uint32_t pixel_count) {
    advanceCursor(pixel_count);
    run_remaining_ -= pixel_count;
    if (run_remaining_ == 0) {
      visible_window_open_ = false;
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

  DisplayOutput* output_;
  const RectUnion* exclusion_;
  Box address_window_;
  BlendingMode blending_mode_;
  int16_t cursor_x_;
  int16_t cursor_y_;
  uint32_t run_remaining_ = 0;
  bool run_excluded_ = false;
  bool visible_window_open_ = false;
  Capabilities capabilities_;
};

}  // namespace roo_display