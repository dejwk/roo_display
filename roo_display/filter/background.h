#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"
#include "roo_display/core/synthetic.h"

namespace roo_display {

// A 'filtering' device, which delegates the actual drawing to another device,
// but applies the specified 'synthetic' background.
class BackgroundFilter : public DisplayOutput {
 public:
  BackgroundFilter(DisplayOutput* output, const Synthetic* background)
      : output_(output),
        background_(background),
        address_window_(0, 0, 0, 0),
        cursor_x_(0),
        cursor_y_(0) {}

  virtual ~BackgroundFilter() {}

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) override {
    address_window_ = Box(x0, y0, x1, y1);
    cursor_x_ = x0;
    cursor_y_ = y0;
    output_->setAddress(x0, y0, x1, y1);
  }

  void write(PaintMode mode, Color* color, uint32_t pixel_count) override {
    int16_t x[pixel_count];
    int16_t y[pixel_count];
    Color newcolor[pixel_count];

    for (uint32_t i = 0; i < pixel_count; ++i) {
      x[i] = cursor_x_++;
      y[i] = cursor_y_;
      if (cursor_x_ > address_window_.xMax()) {
        cursor_y_++;
        cursor_x_ = address_window_.xMin();
      }
      ++i;
    }
    background_->ReadColors(x, y, pixel_count, newcolor);
    for (uint32_t i = 0; i < pixel_count; ++i) {
      newcolor[i] = alphaBlend(newcolor[i], color[i]);
    }
    output_->write(mode, newcolor, pixel_count);
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
      fillRect(mode, *x0++, *y0++, *x1++, *y1++, *color++);
    }
  }

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    BufferedPixelFiller filler(output_, color, mode);
    while (count-- > 0) {
      fillRect(mode, *x0++, *y0++, *x1++, *y1++, color);
    }
  }

  void writePixels(PaintMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    Color newcolor[pixel_count];
    background_->ReadColors(x, y, pixel_count, newcolor);
    for (uint32_t i = 0; i < pixel_count; ++i) {
      newcolor[i] = alphaBlend(newcolor[i], color[i]);
    }
    output_->writePixels(mode, newcolor, x, y, pixel_count);
  }

  void fillPixels(PaintMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    Color newcolor[pixel_count];
    background_->ReadColors(x, y, pixel_count, newcolor);
    for (uint32_t i = 0; i < pixel_count; ++i) {
      newcolor[i] = alphaBlend(newcolor[i], color);
    }
    output_->writePixels(mode, newcolor, x, y, pixel_count);
  }

 private:
  void fillRect(PaintMode mode, int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                Color color) {
    BackgroundFilter::setAddress(xMin, yMin, xMax, yMax);
    int16_t x[64];
    int16_t y[64];
    Color newcolor[64];
    int16_t i = 0;
    uint32_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
    while (pixel_count-- > 0) {
      x[i] = cursor_x_++;
      y[i] = cursor_y_;
      i++;
      if (i == 64) {
        background_->ReadColors(x, y, 64, newcolor);
        for (uint16_t i = 0; i < 64; ++i) {
          newcolor[i] = alphaBlend(newcolor[i], color);
        }
        output_->write(mode, newcolor, 64);
        i = 0;
      }
      if (cursor_x_ > address_window_.xMax()) {
        cursor_y_++;
        cursor_x_ = address_window_.xMin();
      }
    }
    int16_t remaining = i;
    if (remaining > 0) {
      background_->ReadColors(x, y, remaining, newcolor);
      for (uint16_t i = 0; i < remaining; ++i) {
        newcolor[i] = alphaBlend(newcolor[i], color);
      }
      output_->write(mode, newcolor, remaining);
    }
  }

  DisplayOutput* output_;
  const Synthetic* background_;
  Box address_window_;
  int16_t cursor_x_;
  int16_t cursor_y_;
};

}  // namespace roo_display