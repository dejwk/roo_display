#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"

namespace roo_display {

// Represents a binary clip mask. Takes a raw data region, where each byte
// represents a single pixel, and a rectangle that determines the position and
// size of the mask.
// The mask is always byte-aligned; i.e., each row always starts on a new byte.
// It is recommended that the clip mask is aligned within 8-pixels to the
// underlying device coordinates. It may help avoid needless redraws in case
// when some low-res framebuf is used under the clip mask.
class ClipMask {
 public:
  ClipMask(const uint8_t* data, Box bounds)
      : data_(data),
        bounds_(std::move(bounds)),
        line_width_bytes_((bounds.width() + 7) / 8) {}

  const uint8_t* data() const { return data_; }
  const Box& bounds() const { return bounds_; }

  inline bool isSet(int16_t x, int16_t y) const {
    if (!bounds_.contains(x, y)) return true;
    x -= bounds_.xMin();
    y -= bounds_.yMin();
    uint32_t byte_offset = x / 8 + y * line_width_bytes_;
    uint32_t pixel_index = x % 8;
    uint8_t byte = data_[byte_offset];
    return byte & (0x80 >> pixel_index);
  }

  inline bool isAllSet(int16_t x, int16_t y, uint8_t mask,
                       uint8_t lines) const {
    const uint8_t* ptr = data_ + (x - bounds_.xMin()) / 8 +
                         (y - bounds_.yMin()) * line_width_bytes_;
    while (lines-- > 0) {
      if ((*ptr & mask) != mask) return false;
      ptr += line_width_bytes_;
    }
    return true;
  }

  inline bool isAllUnset(int16_t x, int16_t y, uint8_t mask,
                         uint8_t lines) const {
    const uint8_t* ptr = data_ + (x - bounds_.xMin()) / 8 +
                         (y - bounds_.yMin()) * line_width_bytes_;
    while (lines-- > 0) {
      if ((*ptr & mask) != 0) return false;
      ptr += line_width_bytes_;
    }
    return true;
  }

 private:
  const uint8_t* data_;
  Box bounds_;
  uint32_t line_width_bytes_;
};

// A 'filtering' device, which delegates the actual drawing to another device,
// but only passes through the pixels that are not blocked by a specified clip
// mask.
class ClipMaskFilter : public DisplayOutput {
 public:
  ClipMaskFilter(DisplayOutput* output, const ClipMask* clip_mask)
      : output_(output),
        clip_mask_(clip_mask),
        address_window_(0, 0, 0, 0),
        cursor_x_(0),
        cursor_y_(0) {}

  virtual ~ClipMaskFilter() {}

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  PaintMode mode) override {
    address_window_ = Box(x0, y0, x1, y1);
    paint_mode_ = mode;
    cursor_x_ = x0;
    cursor_y_ = y0;
  }

  void write(Color* color, uint32_t pixel_count) override {
    // Naive implementation, for now.
    uint32_t i = 0;
    BufferedPixelWriter writer(output_, paint_mode_);
    while (i < pixel_count) {
      if (clip_mask_->isSet(cursor_x_, cursor_y_)) {
        writer.writePixel(cursor_x_, cursor_y_, color[i]);
      }
      if (++cursor_x_ > address_window_.xMax()) {
        ++cursor_y_;
        cursor_x_ = address_window_.xMin();
      }
      ++i;
    }
  }

  void writeRects(PaintMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      fillRect(mode, *color++, *x0++, *y0++, *x1++, *y1++);
    }
  }

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      fillRect(mode, color, *x0++, *y0++, *x1++, *y1++);
    }
  }

  void writePixels(PaintMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    int16_t* x_out = x;
    int16_t* y_out = y;
    Color* color_out = color;
    uint16_t new_pixel_count = 0;
    for (uint16_t i = 0; i < pixel_count; ++i) {
      if (clip_mask_->isSet(x[i], y[i])) {
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

  void fillPixels(PaintMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    int16_t* x_out = x;
    int16_t* y_out = y;
    uint16_t new_pixel_count = 0;
    for (uint16_t i = 0; i < pixel_count; ++i) {
      if (clip_mask_->isSet(x[i], y[i])) {
        *x_out++ = x[i];
        *y_out++ = y[i];
        new_pixel_count++;
      }
    }
    if (new_pixel_count > 0) {
      output_->fillPixels(mode, color, x, y, new_pixel_count);
    }
  }

 private:
  void fillRect(PaintMode mode, Color color, int16_t x0, int16_t y0, int16_t x1,
                int16_t y1) {
    // Note: we need to flush these every rect, because the rectangles may
    // be overlapping. (A possible alternative would be to only use
    // rectfiller).
    BufferedPixelFiller pfiller(output_, color, mode);
    BufferedRectFiller rfiller(output_, color, mode);
    const Box& bounds = clip_mask_->bounds();
    if (y0 < bounds.yMin()) {
      if (y1 < bounds.yMin()) {
        rfiller.fillRect(x0, y0, x1, y1);
        return;
      }
      rfiller.fillRect(x0, y0, x1, bounds.yMin() - 1);
      y0 = bounds.yMin();
    }
    if (x0 < bounds.xMin()) {
      if (x1 < bounds.xMin()) {
        rfiller.fillRect(x0, y0, x1, y1);
        return;
      }
      rfiller.fillRect(x0, y0, bounds.xMin() - 1, y1);
      x0 = bounds.xMin();
    }
    if (x1 > bounds.xMax()) {
      if (x0 > bounds.xMax()) {
        rfiller.fillRect(x0, y0, x1, y1);
        return;
      }
      rfiller.fillRect(bounds.xMax() + 1, y0, x1, y1);
      x1 = bounds.xMax();
    }
    if (y1 > bounds.yMax()) {
      if (y0 > bounds.yMax()) {
        rfiller.fillRect(x0, y0, x1, y1);
        return;
      }
      rfiller.fillRect(x0, bounds.yMax() + 1, x1, y1);
      y1 = bounds.yMax();
    }
    // Now, [x0, y0, x1, y1] is entirely within bounds.
    // Iterate in 8x8 blocks, filling (or skipping) the entire block when at all
    // possible.
    uint8_t yshift = (y0 - bounds.yMin()) % 8;
    for (int16_t yc0 = y0; yc0 <= y1;) {
      int16_t yc1 = yc0 - yshift + 7;
      if (yc1 > y1) yc1 = y1;
      uint8_t xshift = (x0 - bounds.xMin()) % 8;
      for (int16_t xc0 = x0; xc0 <= x1;) {
        int16_t xc1 = xc0 - xshift + 7;
        if (xc1 > x1) xc1 = x1;
        fillConfinedRect(xc0, yc0, xc1, yc1, pfiller, rfiller);
        xc0 = xc0 - xshift + 8;
        xshift = 0;
      }
      yc0 = yc0 - yshift + 8;
      yshift = 0;
    }
  }

  // The rect must be within a single 8x8 cell, aligned with the bitmask.
  void fillConfinedRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                        BufferedPixelFiller& pfiller, BufferedRectFiller& rfiller) {
    uint8_t mask = (0xFF >> ((x0 - clip_mask_->bounds().xMin()) % 8)) &
                   (0xFF << (7 - ((x1 - clip_mask_->bounds().xMin()) % 8)));
    uint8_t lines = y1 - y0 + 1;
    if (clip_mask_->isAllUnset(x0, y0, mask, lines)) return;
    if (clip_mask_->isAllSet(x0, y0, mask, lines)) {
      rfiller.fillRect(x0, y0, x1, y1);
      return;
    }
    // Degenerate to the slow version.
    for (int16_t y = y0; y <= y1; ++y) {
      for (int16_t x = x0; x <= x1; ++x) {
        if (clip_mask_->isSet(x, y)) {
          pfiller.fillPixel(x, y);
        }
      }
    }
  }

  const Box& bounds() const { return clip_mask_->bounds(); }

  DisplayOutput* output_;
  const ClipMask* clip_mask_;
  Box address_window_;
  PaintMode paint_mode_;
  int16_t cursor_x_;
  int16_t cursor_y_;
};

}  // namespace roo_display