#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"
#include "roo_display/core/rasterizable.h"

namespace roo_display {

// A 'filtering' device, which delegates the actual drawing to another device,
// but applies the specified 'rasterizable' foreground.
class ForegroundFilter : public DisplayOutput {
 public:
  ForegroundFilter(DisplayOutput& output, const Rasterizable* foreground)
      : ForegroundFilter(output, foreground, 0, 0) {}

  ForegroundFilter(DisplayOutput& output, const Rasterizable* foreground,
                   int16_t dx, int16_t dy)
      : output_(output),
        foreground_(foreground),
        address_window_(0, 0, 0, 0),
        cursor_x_(0),
        cursor_y_(0),
        dx_(dx),
        dy_(dy) {}

  virtual ~ForegroundFilter() {}

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  PaintMode mode) override {
    address_window_ = Box(x0 - dx_, y0 - dy_, x1 - dx_, y1 - dy_);
    cursor_x_ = x0 - dx_;
    cursor_y_ = y0 - dy_;
    output_.setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
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
    }
    foreground_->ReadColors(x, y, pixel_count, newcolor);
    for (uint32_t i = 0; i < pixel_count; ++i) {
      newcolor[i] = alphaBlend(color[i], newcolor[i]);
    }
    output_.write(newcolor, pixel_count);
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
    while (count-- > 0) {
      fillRect(mode, *x0++, *y0++, *x1++, *y1++, *color++);
    }
  }

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      fillRect(mode, *x0++, *y0++, *x1++, *y1++, color);
    }
  }

  void writePixels(PaintMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    Color newcolor[pixel_count];
    read(x, y, pixel_count, newcolor);
    for (uint32_t i = 0; i < pixel_count; ++i) {
      newcolor[i] = alphaBlend(color[i], newcolor[i]);
    }
    output_.writePixels(mode, newcolor, x, y, pixel_count);
  }

  void fillPixels(PaintMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    Color newcolor[pixel_count];
    read(x, y, pixel_count, newcolor);
    for (uint32_t i = 0; i < pixel_count; ++i) {
      newcolor[i] = alphaBlend(color, newcolor[i]);
    }
    output_.writePixels(mode, newcolor, x, y, pixel_count);
  }

 private:
  void read(int16_t* x, int16_t* y, uint16_t pixel_count, Color* result) {
    if (dx_ == 0 && dy_ == 0) {
      foreground_->ReadColors(x, y, pixel_count, result);
    } else {
      // NOTE(dawidk): to conserve stack, this could be done in-place and then
      // undone, although it would take 2x longer.
      int16_t xp[pixel_count];
      int16_t yp[pixel_count];
      for (uint32_t i = 0; i < pixel_count; ++i) {
        xp[i] = x[i] - dx_;
        yp[i] = y[i] - dy_;
      }
      foreground_->ReadColors(xp, yp, pixel_count, result);
    }
  }

  void fillRect(PaintMode mode, int16_t xMin, int16_t yMin, int16_t xMax,
                int16_t yMax, Color color) {
    uint32_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
    if (pixel_count <= 64) {
      fillRectInternal(mode, xMin, yMin, xMax, yMax, color);
      return;
    }
    int16_t xMinOuter = (xMin / 8) * 8;
    int16_t yMinOuter = (yMin / 8) * 8;
    int16_t xMaxOuter = (xMax / 8) * 8 + 7;
    int16_t yMaxOuter = (yMax / 8) * 8 + 7;
    for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
      for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
        fillRectInternal(mode, std::max(x, xMin), std::max(y, yMin),
                         std::min((int16_t)(x + 7), xMax),
                         std::min((int16_t)(y + 7), yMax), color);
      }
    }
  }

  // Called for rectangles with area <= 64 pixels.
  void fillRectInternal(PaintMode mode, int16_t xMin, int16_t yMin,
                        int16_t xMax, int16_t yMax, Color color) {
    uint16_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
    Color newcolor[pixel_count];
    bool same = foreground_->ReadColorRect(xMin, yMin, xMax, yMax, newcolor);
    if (same) {
      output_.fillRect(mode, Box(xMin, yMin, xMax, yMax),
                       alphaBlend(color, newcolor[0]));
    } else {
      for (uint16_t i = 0; i < pixel_count; ++i) {
        newcolor[i] = alphaBlend(color, newcolor[i]);
      }
      output_.setAddress(Box(xMin, yMin, xMax, yMax), mode);
      output_.write(newcolor, pixel_count);
    }
  }

  DisplayOutput& output_;
  const Rasterizable* foreground_;
  Box address_window_;
  int16_t cursor_x_;
  int16_t cursor_y_;
  int16_t dx_;
  int16_t dy_;
};

}  // namespace roo_display