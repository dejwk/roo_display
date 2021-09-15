#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"

namespace roo_display {

// Represents a binary clip mask. Takes a raw data region, where each byte
// represents a single pixel, and a rectangle that determines the position and
// size of the mask.
// The mask is always byte-aligned; i.e., each row always starts on a new byte.
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
    BufferedPixelWriter pwriter(output_, mode);
    BufferedRectWriter rwriter(output_, mode);
    while (count-- > 0) {
      BufferedPixelWriterFillAdapter<BufferedPixelWriter> pfiller(pwriter,
                                                                  *color);
      BufferedRectWriterFillAdapter<BufferedRectWriter> rfiller(rwriter,
                                                                *color);
      fillRect(*x0++, *y0++, *x1++, *y1++, pfiller, rfiller);
      color++;
    }
  }

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    BufferedPixelFiller pfiller(output_, color, mode);
    BufferedRectFiller rfiller(output_, color, mode);
    while (count-- > 0) {
      fillRect(*x0++, *y0++, *x1++, *y1++, pfiller, rfiller);
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
  template <typename PixelFiller, typename RectFiller>
  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                PixelFiller& pfiller, RectFiller& rfiller) {
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