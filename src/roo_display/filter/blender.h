#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"
#include "roo_display/core/rasterizable.h"

namespace roo_display {

/// Filtering device that blends with a rasterizable image.
///
/// The Blender template matches `BlendOp`:
/// ```
/// struct T {
///   Color operator()(Color dst, Color src) const;
/// };
/// ```
/// where `dst` is the raster color and `src` is the drawable color.
template <typename Blender>
class BlendingFilter : public DisplayOutput {
 public:
  /// Create a blending filter with default blender and no offset.
  BlendingFilter(DisplayOutput& output, const Rasterizable* raster,
                 Color bgcolor = color::Transparent)
      : BlendingFilter(output, Blender(), raster, bgcolor) {}

  /// Create a blending filter with an offset.
  BlendingFilter(DisplayOutput& output, const Rasterizable* raster, int16_t dx,
                 int16_t dy, Color bgcolor = color::Transparent)
      : BlendingFilter(output, Blender(), raster, dx, dy, bgcolor) {}

  /// Create a blending filter with a custom blender.
  BlendingFilter(DisplayOutput& output, Blender blender,
                 const Rasterizable* raster, Color bgcolor = color::Transparent)
      : BlendingFilter(output, std::move(blender), raster, 0, 0, bgcolor) {}

  /// Create a blending filter with a custom blender and offset.
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

  /// Replace the underlying output.
  void setOutput(DisplayOutput& output) { output_ = &output; }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    address_window_ = Box(x0 - dx_, y0 - dy_, x1 - dx_, y1 - dy_);
    cursor_x_ = x0 - dx_;
    cursor_y_ = y0 - dy_;
    addr_uniform_ = raster_->readUniformColorRect(
        address_window_.xMin(), address_window_.yMin(), address_window_.xMax(),
        address_window_.yMax(), &addr_uniform_color_);
    output_->setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    if (pixel_count == 0) return;

    // Fast path: uniform raster detected in setAddress.
    if (addr_uniform_) {
      Color newcolor[pixel_count];
      for (uint32_t i = 0; i < pixel_count; ++i) {
        newcolor[i] = blender_(addr_uniform_color_, color[i]);
      }
      if (bgcolor_.a() != 0) {
        for (uint32_t i = 0; i < pixel_count; ++i) {
          newcolor[i] = AlphaBlend(bgcolor_, newcolor[i]);
        }
      }
      output_->write(newcolor, pixel_count);
      advanceCursor(pixel_count);
      return;
    }

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

  void fill(Color color, uint32_t pixel_count) override {
    if (pixel_count == 0) return;

    // Fast path: uniform raster detected in setAddress.
    if (addr_uniform_) {
      Color blended =
          AlphaBlend(bgcolor_, blender_(addr_uniform_color_, color));
      output_->fill(blended, pixel_count);
      advanceCursor(pixel_count);
      return;
    }

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
    raster_->readColorsMaybeOutOfBounds(x, y, pixel_count, newcolor);
    for (uint32_t i = 0; i < pixel_count; ++i) {
      newcolor[i] = blender_(newcolor[i], color);
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
    if (count == 0) return;
    {
      // Fast-path: if the bounding box of all rects is uniform in the raster,
      // we can blend once and fill.
      int16_t bbx0 = x0[0], bby0 = y0[0], bbx1 = x1[0], bby1 = y1[0];
      for (uint16_t i = 1; i < count; ++i) {
        if (x0[i] < bbx0) bbx0 = x0[i];
        if (y0[i] < bby0) bby0 = y0[i];
        if (x1[i] > bbx1) bbx1 = x1[i];
        if (y1[i] > bby1) bby1 = y1[i];
      }
      Box bb(bbx0 - dx_, bby0 - dy_, bbx1 - dx_, bby1 - dy_);
      if (raster_->extents().contains(bb)) {
        Color uniform;
        if (raster_->readUniformColorRect(bb.xMin(), bb.yMin(), bb.xMax(),
                                          bb.yMax(), &uniform)) {
          output_->fillRects(mode,
                             AlphaBlend(bgcolor_, blender_(uniform, color)), x0,
                             y0, x1, y1, count);
          return;
        }
      }
    }
    while (count-- > 0) {
      fillRect(mode, *x0++, *y0++, *x1++, *y1++, color);
    }
  }

  void writePixels(BlendingMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    if (pixel_count == 0) return;
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
    if (pixel_count == 0) return;
    {
      // Fast-path: if the bounding box of all rects is uniform in the raster,
      // we can blend once and fill.
      int16_t bbx0 = x[0], bby0 = y[0], bbx1 = x[0], bby1 = y[0];
      for (uint16_t i = 1; i < pixel_count; ++i) {
        if (x[i] < bbx0) bbx0 = x[i];
        if (y[i] < bby0) bby0 = y[i];
        if (x[i] > bbx1) bbx1 = x[i];
        if (y[i] > bby1) bby1 = y[i];
      }
      Box bb(bbx0 - dx_, bby0 - dy_, bbx1 - dx_, bby1 - dy_);
      if (raster_->extents().contains(bb)) {
        Color uniform;
        if (raster_->readUniformColorRect(bb.xMin(), bb.yMin(), bb.xMax(),
                                          bb.yMax(), &uniform)) {
          output_->fillPixels(mode,
                              AlphaBlend(bgcolor_, blender_(uniform, color)), x,
                              y, pixel_count);
          return;
        }
      }
    }
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

  const ColorFormat& getColorFormat() const override {
    return output_->getColorFormat();
  }

  void drawDirectRect(const roo::byte* data, size_t row_width_bytes,
                      int16_t src_x0, int16_t src_y0, int16_t src_x1,
                      int16_t src_y1, int16_t dst_x0, int16_t dst_y0) override {
    DisplayOutput::drawDirectRect(data, row_width_bytes, src_x0, src_y0, src_x1,
                                  src_y1, dst_x0, dst_y0);
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
    Box trimmed = Box::Intersect(Box(xMin, yMin, xMax, yMax),
                                 raster_->extents().translate(dx_, dy_));
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
      Color uniform;
      if (raster_->readUniformColorRect(xMin - dx_, yMin - dy_, xMax - dx_,
                                        yMax - dy_, &uniform)) {
        output_->fillRect(mode, Box(xMin, yMin, xMax, yMax),
                          AlphaBlend(bgcolor_, blender_(uniform, color)));
        return;
      }
    }
    {
      uint32_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
      if (pixel_count <= 64) {
        fillRectInternal(mode, xMin, yMin, xMax, yMax, color);
        return;
      }
    }
    // Adaptive subdivision: prefer quad-split (both axes), fall back to
    // binary split if one axis would go below 8 pixels.
    int16_t w = xMax - xMin + 1;
    int16_t h = yMax - yMin + 1;
    bool can_split_x = (w >= 16);
    bool can_split_y = (h >= 16);
    if (can_split_x && can_split_y) {
      int16_t xMid = (int16_t)((((int32_t)xMin + xMax) / 2) | 3);
      if (xMid >= xMax) xMid = xMax - 1;
      int16_t yMid = (int16_t)((((int32_t)yMin + yMax) / 2) | 3);
      if (yMid >= yMax) yMid = yMax - 1;
      fillRectIntersectingRaster(mode, xMin, yMin, xMid, yMid, color);
      fillRectIntersectingRaster(mode, xMid + 1, yMin, xMax, yMid, color);
      fillRectIntersectingRaster(mode, xMin, yMid + 1, xMid, yMax, color);
      fillRectIntersectingRaster(mode, xMid + 1, yMid + 1, xMax, yMax, color);
    } else if (can_split_x) {
      int16_t xMid = (int16_t)((((int32_t)xMin + xMax) / 2) | 3);
      if (xMid >= xMax) xMid = xMax - 1;
      fillRectIntersectingRaster(mode, xMin, yMin, xMid, yMax, color);
      fillRectIntersectingRaster(mode, xMid + 1, yMin, xMax, yMax, color);
    } else if (can_split_y) {
      int16_t yMid = (int16_t)((((int32_t)yMin + yMax) / 2) | 3);
      if (yMid >= yMax) yMid = yMax - 1;
      fillRectIntersectingRaster(mode, xMin, yMin, xMax, yMid, color);
      fillRectIntersectingRaster(mode, xMin, yMid + 1, xMax, yMax, color);
    } else {
      if (w >= h) {
        int16_t xMid = (int16_t)((((int32_t)xMin + xMax) / 2) | 3);
        if (xMid >= xMax) xMid = xMax - 1;
        fillRectIntersectingRaster(mode, xMin, yMin, xMid, yMax, color);
        fillRectIntersectingRaster(mode, xMid + 1, yMin, xMax, yMax, color);
      } else {
        int16_t yMid = (int16_t)((((int32_t)yMin + yMax) / 2) | 3);
        if (yMid >= yMax) yMid = yMax - 1;
        fillRectIntersectingRaster(mode, xMin, yMin, xMax, yMid, color);
        fillRectIntersectingRaster(mode, xMin, yMid + 1, xMax, yMax, color);
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

  void advanceCursor(uint32_t pixel_count) {
    for (uint32_t i = 0; i < pixel_count; ++i) {
      cursor_x_++;
      if (cursor_x_ > address_window_.xMax()) {
        cursor_y_++;
        cursor_x_ = address_window_.xMin();
      }
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
  bool addr_uniform_ = false;
  Color addr_uniform_color_;
};

}  // namespace roo_display