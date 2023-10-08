#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"
#include "roo_display/core/rasterizable.h"

namespace roo_display {

// A 'filtering' device, which delegates the actual drawing to another device,
// but blends pixels with the specified 'rasterizable' image according to
// the algorithm supplied in the template.
// The Blender template agrees with BlendOp, i.e.:
//
// struct T {
//   Color operator()(Color dst, Color src) const;
// };
//
// where `dst` is the raster color, and `src` is a drawable color.
//
template <typename Blender>
class BlendingFilter : public DisplayOutput {
 public:
  BlendingFilter(DisplayOutput& output, const Rasterizable* raster,
                 Color bgcolor = color::Transparent)
      : BlendingFilter(output, Blender(), raster, bgcolor) {}

  BlendingFilter(DisplayOutput& output, const Rasterizable* raster, int16_t dx,
                 int16_t dy, Color bgcolor = color::Transparent)
      : BlendingFilter(output, Blender(), raster, dx, dy, bgcolor) {}

  BlendingFilter(DisplayOutput& output, Blender blender,
                 const Rasterizable* raster, Color bgcolor = color::Transparent)
      : BlendingFilter(output, std::move(blender), raster, 0, 0, bgcolor) {}

  BlendingFilter(DisplayOutput& output, Blender blender,
                 const Rasterizable* raster, int16_t dx, int16_t dy,
                 Color bgcolor = color::Transparent)
      : output_(&output),
        blender_(std::move(blender)),
        raster_(raster),
        address_window_(0, 0, 0, 0),
        cursor_x_(0),
        cursor_y_(0),
        dx_(dx),
        dy_(dy),
        bgcolor_(bgcolor) {}

  virtual ~BlendingFilter() {}

  void setOutput(DisplayOutput& output) { output_ = &output; }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    address_window_ = Box(x0 - dx_, y0 - dy_, x1 - dx_, y1 - dy_);
    cursor_x_ = x0 - dx_;
    cursor_y_ = y0 - dy_;
    output_->setAddress(x0, y0, x1, y1, mode);
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
    // TODO: detect (common) cases when all pixels are out of bounds.
    raster_->readColorsMaybeOutOfBounds(x, y, pixel_count, newcolor);
    for (uint32_t i = 0; i < pixel_count; ++i) {
      newcolor[i] = blender_(newcolor[i], color[i]);
    }
    if (bgcolor_ != color::Transparent) {
      for (uint32_t i = 0; i < pixel_count; ++i) {
        newcolor[i] = AlphaBlend(bgcolor_, newcolor[i]);
      }
    }
    output_->write(newcolor, pixel_count);
  }

  // void fill(BlendingMode mode, Color color, uint32_t pixel_count) override {
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

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      fillRect(mode, *x0++, *y0++, *x1++, *y1++, *color++);
    }
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      fillRect(mode, *x0++, *y0++, *x1++, *y1++, color);
    }
  }

  void writePixels(BlendingMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    Color newcolor[pixel_count];
    read(x, y, pixel_count, newcolor);
    for (uint32_t i = 0; i < pixel_count; ++i) {
      newcolor[i] = blender_(newcolor[i], color[i]);
    }
    if (bgcolor_ != color::Transparent) {
      for (uint32_t i = 0; i < pixel_count; ++i) {
        newcolor[i] = AlphaBlend(bgcolor_, newcolor[i]);
      }
    }
    output_->writePixels(mode, newcolor, x, y, pixel_count);
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    Color newcolor[pixel_count];
    read(x, y, pixel_count, newcolor);
    for (uint32_t i = 0; i < pixel_count; ++i) {
      newcolor[i] = blender_(newcolor[i], color);
    }
    if (bgcolor_ != color::Transparent) {
      for (uint32_t i = 0; i < pixel_count; ++i) {
        newcolor[i] = AlphaBlend(bgcolor_, newcolor[i]);
      }
    }
    output_->writePixels(mode, newcolor, x, y, pixel_count);
  }

 private:
  void read(int16_t* x, int16_t* y, uint16_t pixel_count, Color* result) {
    if (dx_ == 0 && dy_ == 0) {
      raster_->readColorsMaybeOutOfBounds(x, y, pixel_count, result);
    } else {
      // NOTE(dawidk): to conserve stack, this could be done in-place and then
      // undone, although it would take 2x longer.
      int16_t xp[pixel_count];
      int16_t yp[pixel_count];
      for (uint32_t i = 0; i < pixel_count; ++i) {
        xp[i] = x[i] - dx_;
        yp[i] = y[i] - dy_;
      }
      raster_->readColorsMaybeOutOfBounds(xp, yp, pixel_count, result);
    }
  }

  void fillRect(BlendingMode mode, int16_t xMin, int16_t yMin, int16_t xMax,
                int16_t yMax, Color color) {
    Color out_of_bounds_color = AlphaBlend(bgcolor_, color);
    Box trimmed =
        Box::Intersect(Box(xMin, yMin, xMax, yMax), raster_->extents().translate(dx_, dy_));
    if (trimmed.empty()) {
      output_->fillRect(mode, xMin, yMin, xMax, yMax, out_of_bounds_color);
      return;
    }
    if (yMin < trimmed.yMin()) {
      // Draw top bar of the border.
      output_->fillRect(mode, xMin, yMin, xMax, trimmed.yMin() - 1,
                        out_of_bounds_color);
    }
    if (xMin < trimmed.xMin()) {
      // Draw left bar of the border.
      output_->fillRect(mode, xMin, trimmed.yMin(), trimmed.xMin() - 1,
                        trimmed.yMax(), out_of_bounds_color);
    }
    fillRectIntersectingRaster(mode, trimmed.xMin(), trimmed.yMin(),
                               trimmed.xMax(), trimmed.yMax(), color);
    if (xMax > trimmed.xMax()) {
      // Draw right bar of the border.
      output_->fillRect(mode, trimmed.xMax() + 1, trimmed.yMin(), xMax,
                        trimmed.yMax(), out_of_bounds_color);
    }
    if (yMax > trimmed.yMax()) {
      // Draw bottom bar of the border.
      output_->fillRect(mode, xMin, trimmed.yMax() + 1, xMax, yMax,
                        out_of_bounds_color);
    }
  }

  void fillRectIntersectingRaster(BlendingMode mode, int16_t xMin, int16_t yMin,
                                  int16_t xMax, int16_t yMax, Color color) {
    {
      uint32_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
      if (pixel_count <= 64) {
        fillRectInternal(mode, xMin, yMin, xMax, yMax, color);
        return;
      }
    }
    const int16_t xMinOuter = (xMin / 8) * 8;
    const int16_t yMinOuter = (yMin / 8) * 8;
    const int16_t xMaxOuter = (xMax / 8) * 8 + 7;
    const int16_t yMaxOuter = (yMax / 8) * 8 + 7;
    for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
      for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
        fillRectInternal(mode, std::max(x, xMin), std::max(y, yMin),
                         std::min((int16_t)(x + 7), xMax),
                         std::min((int16_t)(y + 7), yMax), color);
      }
    }
  }

  // Called for rectangles with area <= 64 pixels.
  void fillRectInternal(BlendingMode mode, int16_t xMin, int16_t yMin,
                        int16_t xMax, int16_t yMax, Color color) {
    uint16_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
    Color newcolor[pixel_count];
    bool same = raster_->readColorRect(xMin - dx_, yMin - dy_, xMax - dx_,
                                       yMax - dy_, newcolor);
    if (same) {
      output_->fillRect(mode, Box(xMin, yMin, xMax, yMax),
                        AlphaBlend(bgcolor_, blender_(newcolor[0], color)));
    } else {
      for (uint16_t i = 0; i < pixel_count; ++i) {
        newcolor[i] = blender_(newcolor[i], color);
      }
      if (bgcolor_ != color::Transparent) {
        for (uint16_t i = 0; i < pixel_count; ++i) {
          newcolor[i] = AlphaBlend(bgcolor_, newcolor[i]);
        }
      }
      output_->setAddress(Box(xMin, yMin, xMax, yMax), mode);
      output_->write(newcolor, pixel_count);
    }
  }

  DisplayOutput* output_;
  Blender blender_;
  const Rasterizable* raster_;
  Box address_window_;
  int16_t cursor_x_;
  int16_t cursor_y_;
  int16_t dx_;
  int16_t dy_;
  Color bgcolor_;
};

}  // namespace roo_display