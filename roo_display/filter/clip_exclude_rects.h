#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"

namespace roo_display {

class RectUnion {
 public:
  RectUnion(const Box* begin, const Box* end) : begin_(begin), end_(end) {}

  inline bool isSet(int16_t x, int16_t y) const {
    for (const Box* box  = begin_; box != end_; ++box) {
      if (box->contains(x, y)) return false;
    }
    return true;
  }

 private:
  const Box* begin_;
  const Box* end_;
};

// A 'filtering' device, which delegates the actual drawing to another device,
// but only passes through the pixels that are not blocked by a specified clip
// mask.
class RectUnionFilter : public DisplayOutput {
 public:
  RectUnionFilter(DisplayOutput* output, const RectUnion* mask)
      : output_(output),
        mask_(mask),
        address_window_(0, 0, 0, 0),
        cursor_x_(0),
        cursor_y_(0) {}

  virtual ~RectUnionFilter() {}

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
      if (mask_->isSet(cursor_x_, cursor_y_)) {
        writer.writePixel(cursor_x_, cursor_y_, color[i]);
      }
      if (++cursor_x_ > address_window_.xMax()) {
        ++cursor_y_;
        cursor_x_ = address_window_.xMin();
      }
      ++i;
    }
  }

  // void fill(PaintMode mode, Color color, uint32_t pixel_count) override {
  //   // Naive implementation, for now.
  //   uint32_t i = 0;
  //   BufferedPixelFiller filler(output_, color, mode);
  //   while (i < pixel_count) {
  //     if (clip_mask_->isSet(cursor_x_, cursor_y_)) {
  //       filler.fillPixel(cursor_x_, cursor_y_);
  //     }
  //     if (++cursor_x_ > address_window_.xMax()) {
  //       ++cursor_y_;
  //       cursor_x_ = address_window_.xMin();
  //     }
  //     ++i;
  //   }
  // }

  void writeRects(PaintMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    BufferedPixelWriter writer(output_, mode);
    while (count-- > 0) {
      for (int16_t y = *y0; y <= *y1; ++y) {
        for (int16_t x = *x0; x <= *x1; ++x) {
          if (mask_->isSet(x, y)) {
            writer.writePixel(x, y, *color);
          }
        }
      }
      x0++;
      y0++;
      x1++;
      y1++;
      color++;
    }
  }

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    BufferedPixelFiller filler(output_, color, mode);
    while (count-- > 0) {
      for (int16_t y = *y0; y <= *y1; ++y) {
        for (int16_t x = *x0; x <= *x1; ++x) {
          if (mask_->isSet(x, y)) {
            filler.fillPixel(x, y);
          }
        }
      }
      x0++;
      y0++;
      x1++;
      y1++;
    }
  }

  void writePixels(PaintMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    int16_t* x_out = x;
    int16_t* y_out = y;
    Color* color_out = color;
    uint16_t new_pixel_count = 0;
    for (uint16_t i = 0; i < pixel_count; ++i) {
      if (mask_->isSet(x[i], y[i])) {
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
      if (mask_->isSet(x[i], y[i])) {
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
  DisplayOutput* output_;
  const RectUnion* mask_;
  Box address_window_;
  PaintMode paint_mode_;
  int16_t cursor_x_;
  int16_t cursor_y_;
};

}  // namespace roo_display