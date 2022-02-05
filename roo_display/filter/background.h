#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"
#include "roo_display/core/rasterizable.h"

namespace roo_display {

// A 'filtering' device, which delegates the actual drawing to another device,
// but applies the specified 'rasterizable' background.
class BackgroundFilter : public DisplayOutput {
 public:
  BackgroundFilter(DisplayOutput& output, const Rasterizable* background)
      : BackgroundFilter(output, background, 0, 0) {}

  BackgroundFilter(DisplayOutput& output, const Rasterizable* background,
                   int16_t dx, int16_t dy)
      : output_(output),
        background_(background),
        address_window_(0, 0, 0, 0),
        cursor_x_(0),
        cursor_y_(0),
        dx_(dx),
        dy_(dy) {}

  virtual ~BackgroundFilter() {}

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
    background_->ReadColorsMaybeOutOfBounds(x, y, pixel_count, newcolor);
    for (uint32_t i = 0; i < pixel_count; ++i) {
      newcolor[i] = alphaBlend(newcolor[i], color[i]);
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
    read(x, y, pixel_count, newcolor);
    for (uint32_t i = 0; i < pixel_count; ++i) {
      newcolor[i] = alphaBlend(newcolor[i], color[i]);
    }
    output_.writePixels(mode, newcolor, x, y, pixel_count);
  }

  void fillPixels(PaintMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    Color newcolor[pixel_count];
    read(x, y, pixel_count, newcolor);
    for (uint32_t i = 0; i < pixel_count; ++i) {
      newcolor[i] = alphaBlend(newcolor[i], color);
    }
    output_.writePixels(mode, newcolor, x, y, pixel_count);
  }

 private:
  void read(int16_t* x, int16_t* y, uint16_t pixel_count, Color* result) {
    if (dx_ == 0 && dy_ == 0) {
      background_->ReadColorsMaybeOutOfBounds(x, y, pixel_count, result);
    } else {
      // NOTE(dawidk): to conserve stack, this could be done in-place and then
      // undone, although it would take 2x longer.
      int16_t xp[pixel_count];
      int16_t yp[pixel_count];
      for (uint32_t i = 0; i < pixel_count; ++i) {
        xp[i] = x[i] - dx_;
        yp[i] = y[i] - dy_;
      }
      background_->ReadColorsMaybeOutOfBounds(xp, yp, pixel_count, result);
    }
  }

  void fillRect(PaintMode mode, int16_t xMin, int16_t yMin, int16_t xMax,
                int16_t yMax, Color color) {
    Box full(xMin, yMin, xMax, yMax);
    Box bgbounds = background_->extents();
    Box trimmed = Box::intersect(full, bgbounds);
    if (full.yMin() < trimmed.yMin()) {
      // Draw top bar of the border.
      output_.fillRect(mode, full.xMin(), full.yMin(), full.xMax(),
                       trimmed.yMin() - 1, color);
    }
    if (full.xMin() < trimmed.xMin()) {
      // Draw left bar of the border.
      output_.fillRect(mode, full.xMin(), trimmed.yMin(), trimmed.xMin() - 1,
                       trimmed.yMax(), color);
    }
    fillRectIntersectingBackground(mode, trimmed.xMin(), trimmed.yMin(),
                                   trimmed.xMax(), trimmed.yMax(), color);
    if (full.xMax() > trimmed.xMax()) {
      // Draw right bar of the border.
      output_.fillRect(mode, trimmed.xMax() + 1, trimmed.yMin(), full.xMax(),
                       trimmed.yMax(), color);
    }
    if (full.yMax() > trimmed.yMax()) {
      // Draw bottom bar of the border.
      output_.fillRect(mode, full.xMin(), trimmed.yMax() + 1, full.xMax(),
                       full.yMax(), color);
    }
  }

  void fillRectIntersectingBackground(PaintMode mode, int16_t xMin,
                                      int16_t yMin, int16_t xMax, int16_t yMax,
                                      Color color) {
    BackgroundFilter::setAddress(xMin, yMin, xMax, yMax, mode);
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
        output_.write(newcolor, 64);
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
      output_.write(newcolor, remaining);
    }
  }

  DisplayOutput& output_;
  const Rasterizable* background_;
  Box address_window_;
  int16_t cursor_x_;
  int16_t cursor_y_;
  int16_t dx_;
  int16_t dy_;
};

}  // namespace roo_display