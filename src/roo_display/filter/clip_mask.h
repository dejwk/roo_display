#pragma once

#include <type_traits>

#include "roo_backport.h"
#include "roo_backport/byte.h"
#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"

namespace roo_display {

/// Binary clip mask using a packed bit buffer.
///
/// Each bit represents a pixel within `bounds`. Rows are byte-aligned. For
/// best performance, align the mask to 8-pixel boundaries in device
/// coordinates.
class ClipMask {
 public:
  /// Construct a clip mask.
  ClipMask(const roo::byte* data, Box bounds, bool inverted = false)
      : data_(data),
        bounds_(std::move(bounds)),
        inverted_(inverted),
        line_width_bytes_((bounds.width() + 7) / 8) {}

  /// Deprecated: use the `roo::byte*` constructor instead.
  template <typename U = uint8_t,
            typename std::enable_if<!std::is_same<U, roo::byte>::value,
                                    int>::type = 0>
  ClipMask(const U* data, Box bounds)
      : ClipMask((const roo::byte*)data, bounds) {}

  /// Return raw data buffer.
  const roo::byte* data() const { return data_; }
  /// Return mask bounds.
  const Box& bounds() const { return bounds_; }
  /// Return inversion flag.
  bool inverted() const { return inverted_; }

  /// Return whether a point is masked out.
  inline bool isMasked(int16_t x, int16_t y) const {
    if (!bounds_.contains(x, y)) return inverted_;
    x -= bounds_.xMin();
    y -= bounds_.yMin();
    uint32_t byte_offset = x / 8 + y * line_width_bytes_;
    uint32_t pixel_index = x % 8;
    roo::byte b = data_[byte_offset];
    return (bool)(b & (roo::byte{0x80} >> pixel_index)) ^ inverted_;
  }

  /// Return whether all bits in a block are masked.
  inline bool isAllMasked(int16_t x, int16_t y, roo::byte mask,
                          uint8_t lines) const {
    return inverted_ ? isAllUnset(x, y, mask, ~lines)
                     : isAllSet(x, y, mask, lines);
  }

  /// Return whether all bits in a block are unmasked.
  inline bool isAllUnmasked(int16_t x, int16_t y, roo::byte mask,
                            uint8_t lines) const {
    return inverted_ ? isAllSet(x, y, mask, ~lines)
                     : isAllUnset(x, y, mask, lines);
  }

  /// Returns the length of a same-state prefix on one row, bounded by @p count.
  /// The prefix has the same masking state as `(x, y)`, including outside
  /// bounds.
  uint32_t sameStateCount(int16_t x, int16_t y, uint32_t count) const {
    if (count == 0) return 0;
    bool masked = isMasked(x, y);
    int32_t cursor = x;
    uint32_t remaining = count;
    while (remaining > 0) {
      if (y < bounds_.yMin() || y > bounds_.yMax() || cursor > bounds_.xMax()) {
        return masked == inverted_ ? count : count - remaining;
      }
      if (cursor < bounds_.xMin()) {
        if (masked != inverted_) break;
        uint32_t batch = std::min<uint32_t>(remaining, bounds_.xMin() - cursor);
        cursor += batch;
        remaining -= batch;
        continue;
      }
      uint32_t offset = cursor - bounds_.xMin();
      uint32_t batch = std::min<uint32_t>(remaining, 8 - offset % 8);
      batch = std::min<uint32_t>(batch, bounds_.xMax() - cursor + 1);
      uint8_t bits = static_cast<uint8_t>(
          data_[(y - bounds_.yMin()) * line_width_bytes_ + offset / 8]);
      if (masked != inverted_) bits = ~bits;
      uint8_t mask =
          (0xFFu >> (offset % 8)) & (0xFFu << (8 - offset % 8 - batch));
      if ((bits & mask) != 0) {
        for (uint32_t i = 0; i < batch; ++i) {
          if ((bits & (0x80u >> (offset % 8 + i))) != 0) {
            return count - remaining + i;
          }
        }
      }
      cursor += batch;
      remaining -= batch;
    }
    return count - remaining;
  }

  /// Set inversion behavior.
  void setInverted(bool inverted) { inverted_ = inverted; }

 private:
  inline bool isAllSet(int16_t x, int16_t y, roo::byte mask,
                       uint8_t lines) const {
    const roo::byte* ptr = data_ + (x - bounds_.xMin()) / 8 +
                           (y - bounds_.yMin()) * line_width_bytes_;
    while (lines-- > 0) {
      if ((*ptr & mask) != mask) return false;
      ptr += line_width_bytes_;
    }
    return true;
  }

  inline bool isAllUnset(int16_t x, int16_t y, roo::byte mask,
                         uint8_t lines) const {
    const roo::byte* ptr = data_ + (x - bounds_.xMin()) / 8 +
                           (y - bounds_.yMin()) * line_width_bytes_;
    while (lines-- > 0) {
      if ((*ptr & mask) != roo::byte{0}) return false;
      ptr += line_width_bytes_;
    }
    return true;
  }

  const roo::byte* data_;

  Box bounds_;

  // If true, the meaning of 'set' and 'unset' is inverted.
  // Normally, 'set' = 'masked out'. If inverted, 'set' = 'unmasked'.
  bool inverted_;

  uint32_t line_width_bytes_;
};

/// Filtering device that applies a clip mask.
class ClipMaskFilter : public DisplayOutput {
 public:
  ClipMaskFilter(DisplayOutput& output, const ClipMask* clip_mask,
                 int16_t dx = 0, int16_t dy = 0)
      : output_(output),
        clip_mask_(clip_mask->data(), clip_mask->bounds().translate(dx, dy),
                   clip_mask->inverted()),
        address_window_(0, 0, 0, 0),
        cursor_x_(0),
        cursor_y_(0),
        capabilities_(output.getCapabilities().supportsBlending(),
                      /*supports_blit_copy=*/false) {}

  virtual ~ClipMaskFilter() {}

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    address_window_ = Box(x0, y0, x1, y1);
    blending_mode_ = mode;
    cursor_x_ = x0;
    cursor_y_ = y0;
    output_remaining_ = 0;
  }

  void write(Color* color, uint32_t pixel_count) override {
    while (pixel_count > 0) {
      bool masked = clip_mask_.isMasked(cursor_x_, cursor_y_);
      uint32_t run = sameStateCount(pixel_count, masked);
      if (masked) {
        output_remaining_ = 0;
      } else {
        openOutputWindow();
        run = std::min(run, output_remaining_);
        output_.write(color, run);
        output_remaining_ -= run;
      }
      advanceCursor(run);
      color += run;
      pixel_count -= run;
    }
  }

  void fill(Color color, uint32_t pixel_count) override {
    while (pixel_count > 0) {
      bool masked = clip_mask_.isMasked(cursor_x_, cursor_y_);
      uint32_t run = sameStateCount(pixel_count, masked);
      if (masked) {
        output_remaining_ = 0;
      } else {
        openOutputWindow();
        run = std::min(run, output_remaining_);
        output_.fill(color, run);
        output_remaining_ -= run;
      }
      advanceCursor(run);
      pixel_count -= run;
    }
  }

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    output_remaining_ = 0;
    while (count-- > 0) {
      fillSingleRect(mode, *color++, *x0++, *y0++, *x1++, *y1++);
    }
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    output_remaining_ = 0;
    while (count-- > 0) {
      fillSingleRect(mode, color, *x0++, *y0++, *x1++, *y1++);
    }
  }

  void writePixels(BlendingMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    output_remaining_ = 0;
    int16_t* x_out = x;
    int16_t* y_out = y;
    Color* color_out = color;
    uint16_t new_pixel_count = 0;
    for (uint16_t i = 0; i < pixel_count; ++i) {
      if (!clip_mask_.isMasked(x[i], y[i])) {
        *x_out++ = x[i];
        *y_out++ = y[i];
        *color_out++ = color[i];
        new_pixel_count++;
      }
    }
    if (new_pixel_count > 0) {
      output_.writePixels(mode, color, x, y, new_pixel_count);
    }
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    output_remaining_ = 0;
    int16_t* x_out = x;
    int16_t* y_out = y;
    uint16_t new_pixel_count = 0;
    for (uint16_t i = 0; i < pixel_count; ++i) {
      if (!clip_mask_.isMasked(x[i], y[i])) {
        *x_out++ = x[i];
        *y_out++ = y[i];
        new_pixel_count++;
      }
    }
    if (new_pixel_count > 0) {
      output_.fillPixels(mode, color, x, y, new_pixel_count);
    }
  }

  const ColorFormat& getColorFormat() const override {
    return output_.getColorFormat();
  }

  const Capabilities& getCapabilities() const override { return capabilities_; }

  void drawDirectRect(const roo::byte* data, size_t row_width_bytes,
                      int16_t src_x0, int16_t src_y0, int16_t src_x1,
                      int16_t src_y1, int16_t dst_x0, int16_t dst_y0) override {
    DisplayOutput::drawDirectRect(data, row_width_bytes, src_x0, src_y0, src_x1,
                                  src_y1, dst_x0, dst_y0);
  }

 private:
  friend class FrontToBackWriter;

  void invalidateOutputWindow() { output_remaining_ = 0; }

  // Reinspect mask data on every call: front-to-back writers can mutate it.
  uint32_t sameStateCount(uint32_t count, bool masked) const {
    int16_t x = cursor_x_;
    int32_t y = cursor_y_;
    uint32_t total = 0;
    while (total < count) {
      if (clip_mask_.isMasked(x, y) != masked) break;
      uint32_t row =
          std::min<uint32_t>(count - total, address_window_.xMax() - x + 1);
      uint32_t run = clip_mask_.sameStateCount(x, y, row);
      total += run;
      if (run < row) break;
      x = address_window_.xMin();
      ++y;
    }
    return total;
  }

  // A window may extend beyond the current visible run; no masked pixel is
  // sent.
  void openOutputWindow() {
    if (output_remaining_ != 0) return;
    int16_t y_max = cursor_x_ == address_window_.xMin() ? address_window_.yMax()
                                                        : cursor_y_;
    output_.setAddress(cursor_x_, cursor_y_, address_window_.xMax(), y_max,
                       blending_mode_);
    output_remaining_ =
        (address_window_.xMax() - cursor_x_ + 1) * (y_max - cursor_y_ + 1);
  }

  void advanceCursor(uint32_t count) {
    uint32_t offset = cursor_x_ - address_window_.xMin() + count;
    cursor_y_ += offset / address_window_.width();
    cursor_x_ = address_window_.xMin() + offset % address_window_.width();
  }

  void fillSingleRect(BlendingMode mode, Color color, int16_t x0, int16_t y0,
                      int16_t x1, int16_t y1) {
    // Note: we need to flush these every rect, because the rectangles may
    // be overlapping. (A possible alternative would be to only use
    // rectfiller).
    BufferedPixelFiller pfiller(output_, color, mode);
    BufferedRectFiller rfiller(output_, color, mode);
    const Box& bounds = clip_mask_.bounds();
    if (y0 < bounds.yMin()) {
      if (y1 < bounds.yMin()) {
        if (!clip_mask_.inverted()) {
          rfiller.fillRect(x0, y0, x1, y1);
        }
        return;
      }
      if (!clip_mask_.inverted()) {
        rfiller.fillRect(x0, y0, x1, bounds.yMin() - 1);
      }
      y0 = bounds.yMin();
    }
    if (x0 < bounds.xMin()) {
      if (x1 < bounds.xMin()) {
        if (!clip_mask_.inverted()) {
          rfiller.fillRect(x0, y0, x1, y1);
        }
        return;
      }
      if (!clip_mask_.inverted()) {
        rfiller.fillRect(x0, y0, bounds.xMin() - 1, y1);
      }
      x0 = bounds.xMin();
    }
    if (x1 > bounds.xMax()) {
      if (x0 > bounds.xMax()) {
        if (!clip_mask_.inverted()) {
          rfiller.fillRect(x0, y0, x1, y1);
        }
        return;
      }
      if (!clip_mask_.inverted()) {
        rfiller.fillRect(bounds.xMax() + 1, y0, x1, y1);
      }
      x1 = bounds.xMax();
    }
    if (y1 > bounds.yMax()) {
      if (y0 > bounds.yMax()) {
        if (!clip_mask_.inverted()) {
          rfiller.fillRect(x0, y0, x1, y1);
        }
        return;
      }
      if (!clip_mask_.inverted()) {
        rfiller.fillRect(x0, bounds.yMax() + 1, x1, y1);
      }
      y1 = bounds.yMax();
    }
    // Now, [x0, y0, x1, y1] is entirely within bounds.
    // Iterate in 8x8 blocks, filling (or skipping) the entire block when at all
    // possible.
    uint8_t yshift = (y0 - bounds.yMin()) % 8;
    for (int16_t yc0 = y0; yc0 <= y1;) {
      int16_t yc1 = yc0 - yshift + 7;
      if (yc1 > y1) yc1 = y1;
      uint8_t lines = yc1 - yc0 + 1;
      uint8_t xshift = (x0 - bounds.xMin()) % 8;
      for (int16_t xc0 = x0; xc0 <= x1;) {
        int16_t xc1 = xc0 - xshift + 7;
        roo::byte mask = roo::byte{0xFF} >> xshift;
        if (xc1 > x1) {
          mask &= (roo::byte{0xFF} << (xc1 - x1));
          xc1 = x1;
        }
        if (!clip_mask_.isAllMasked(xc0, yc0, mask, lines)) {
          if (clip_mask_.isAllUnmasked(xc0, yc0, mask, lines)) {
            rfiller.fillRect(xc0, yc0, xc1, yc1);
          } else {
            // Degenerate to the slow version.
            for (int16_t y = yc0; y <= yc1; ++y) {
              for (int16_t x = xc0; x <= xc1; ++x) {
                if (!clip_mask_.isMasked(x, y)) {
                  pfiller.fillPixel(x, y);
                }
              }
            }
          }
        }

        xc0 = xc0 - xshift + 8;
        xshift = 0;
      }
      yc0 = yc0 - yshift + 8;
      yshift = 0;
    }
  }

  const Box& bounds() const { return clip_mask_.bounds(); }

  DisplayOutput& output_;
  ClipMask clip_mask_;
  Box address_window_;
  BlendingMode blending_mode_;
  int16_t cursor_x_;
  int16_t cursor_y_;
  uint32_t output_remaining_ = 0;
  Capabilities capabilities_;
};

}  // namespace roo_display