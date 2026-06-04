#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"
#include "roo_display/core/rasterizable.h"

namespace roo_display {

namespace internal {

// Presents a clipped raster stream over a larger address window, synthesizing
// transparent pixels outside the delegate stream extents while preserving
// row-major iteration order.
class WindowedPixelStream {
 public:
  static constexpr uint32_t kUnlimitedRunLength = 0xFFFFFFFFu;

  WindowedPixelStream(const Rasterizable* raster) : raster_(raster) {}

  /// Returns true if non-empty.
  void reset(const Box& addr_window) {
    bounds_ = Box::Intersect(addr_window, raster_->extents());
    if (bounds_.empty()) {
      delegate_ = nullptr;
      skip_ = 0;
      remaining_lines_ = -1;
      inner_width_ = 0;
      gap_ = 0;
      current_run_ = 0;
      current_run_is_delegate_ = false;
      return;
    }
    if (bounds_.height() > 8 && bounds_.width() > 8) {
      delegate_ = raster_->createStream(bounds_);
    } else {
      // Skip creating the stream to avoid dynamic memory allocation. Will serve
      // data directly from the raster using readColors, which is more efficient
      // for small areas.
      delegate_ = nullptr;
      x_ = bounds_.xMin();
      y_ = bounds_.yMin();
    }

    Box local_inner =
        bounds_.translate(-addr_window.xMin(), -addr_window.yMin());
    skip_ = local_inner.xMin() +
            static_cast<int32_t>(addr_window.width()) * local_inner.yMin();
    remaining_lines_ =
        bounds_.width() == addr_window.width() ? 0 : bounds_.height() - 1;
    inner_width_ =
        (bounds_.width() == addr_window.width()
             ? static_cast<int32_t>(addr_window.width()) * bounds_.height()
             : bounds_.width());
    gap_ = addr_window.width() - bounds_.width();
    current_run_ = skip_ > 0 ? skip_ : inner_width_;
    current_run_is_delegate_ = skip_ == 0 && inner_width_ > 0;
  }

  void read(Color* buf, uint32_t count, uint32_t& run_length) {
    run_length = 0;
    if (count == 0) return;
    bool first_batch = true;
    while (true) {
      if (current_run_ == 0) {
        FillColor(buf, count, color::Transparent);
        if (first_batch) {
          run_length = kUnlimitedRunLength;
        }
        return;
      }

      uint32_t run = std::min<uint32_t>(count, current_run_);
      if (current_run_is_delegate_) {
        readDelegate(buf, run, run_length);
      } else {
        FillColor(buf, run, color::Transparent);
        run_length = current_run_;
      }
      buf += run;
      count -= run;
      advance(run);
      if (count == 0) {
        if (!first_batch) run_length = 0;
        return;
      }
      first_batch = false;
    }
  }

  void skip(uint32_t count) {
    while (count > 0) {
      if (current_run_ == 0) {
        return;
      }

      uint32_t run = std::min<uint32_t>(count, current_run_);
      if (current_run_is_delegate_) {
        skipDelegate(run);
      }
      count -= run;
      advance(run);
    }
  }

 private:
  void readDelegate(Color* buf, uint32_t count, uint32_t& run_length) {
    DCHECK(count <= 0xFFFFu);
    if (delegate_ != nullptr) {
      delegate_->read(buf, static_cast<uint16_t>(count), run_length);
    } else {
      if (x_ == bounds_.xMin() && count % bounds_.width() == 0) {
        // Fast path: we're at the start of a row and reading an integral number
        // of rows, so we can use readColorRect which may be optimized for
        // this case.
        int16_t y_end = y_ + count / bounds_.width() - 1;
        if (raster_->readColorRect(x_, y_, bounds_.xMax(), y_end, buf)) {
          // All pixels are the same, so we can report a run length.
          run_length = count;
          if (count > 1) {
            FillColor(&buf[1], count - 1, buf[0]);
          }
        } else {
          run_length = 0;
        }
        y_ = y_end + 1;
        return;
      }
      if (x_ + (int32_t)count - 1 <= bounds_.xMax()) {
        // Fast path: all pixels are on the same row, so we can use
        // readColorRect which may be optimized for this case.
        if (raster_->readColorRect(x_, y_, x_ + count - 1, y_, buf)) {
          // All pixels are the same, so we can report a run length.
          run_length = count;
          if (count > 1) {
            FillColor(&buf[1], count - 1, buf[0]);
          }
        } else {
          run_length = 0;
        }
        x_ += count;
        if (x_ > bounds_.xMax()) {
          x_ = bounds_.xMin();
          ++y_;
        }
        return;
      }
      // Slow path: general case.
      int16_t x[count];
      int16_t y[count];
      for (uint32_t i = 0; i < count; ++i) {
        x[i] = x_;
        y[i] = y_;
        if (x_ < bounds_.xMax()) {
          ++x_;
        } else {
          x_ = bounds_.xMin();
          ++y_;
        }
      }
      raster_->readColors(x, y, count, buf);
      run_length = 0;
    }
  }

  void skipDelegate(uint32_t count) {
    if (delegate_ != nullptr) {
      delegate_->skip(count);
    } else {
      auto w = bounds_.width();
      y_ += count / w;
      x_ += count % w;
      if (x_ > bounds_.xMax()) {
        x_ -= w;
        ++y_;
      }
    }
  }

  void advance(uint32_t count) {
    current_run_ -= count;
    if (current_run_ != 0) return;

    if (current_run_is_delegate_) {
      if (remaining_lines_ == 0) {
        current_run_is_delegate_ = false;
        return;
      }
      current_run_is_delegate_ = false;
      current_run_ = gap_;
      --remaining_lines_;
      if (current_run_ == 0) {
        current_run_is_delegate_ = true;
        current_run_ = inner_width_;
      }
      return;
    }

    current_run_is_delegate_ = true;
    current_run_ = inner_width_;
  }

  const Rasterizable* raster_;
  std::unique_ptr<PixelStream> delegate_;
  int32_t skip_;
  int16_t remaining_lines_;
  int32_t inner_width_;
  int16_t gap_;
  int32_t current_run_;
  bool current_run_is_delegate_;
  // Used when we skip delegate creation due to tiny window size.
  Box bounds_;
  int16_t x_, y_;
};

inline __attribute__((always_inline)) void BlendWithBackground(
    Color* color, uint32_t pixel_count, Color bgcolor) {
  if (bgcolor.a() == color::Transparent) return;
  if (bgcolor.isOpaque()) {
    for (uint32_t i = 0; i < pixel_count; ++i) {
      color[i] = AlphaBlendOverOpaque(bgcolor, color[i]);
    }
  } else {
    for (uint32_t i = 0; i < pixel_count; ++i) {
      color[i] = AlphaBlend(bgcolor, color[i]);
    }
  }
}

}  // namespace internal

/// Filtering device that blends with a rasterizable image.
///
/// The Blender template matches `BlendOp`:
/// ```
/// struct T {
///   Color blend(Color dst, Color src) const;
///   Color blendTransparentSrc(Color dst, Color src) const;
///   Color blendTransparentDst(Color dst, Color src) const;
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
        capabilities_(output.getCapabilities().supportsBlending(),
                      /*supports_blit_copy=*/false),
        blender_(std::move(blender)),
        raster_(raster),
        addr_stream_(raster_),
        address_window_(0, 0, 0, 0),
        cursor_x_(0),
        cursor_y_(0),
        dx_(dx),
        dy_(dy),
        bgcolor_(bgcolor) {}

  virtual ~BlendingFilter() {}

  /// Replace the underlying output.
  void setOutput(DisplayOutput& output) {
    output_ = &output;
    capabilities_ = Capabilities(output.getCapabilities().supportsBlending(),
                                 /*supports_blit_copy=*/false);
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    address_window_ = Box(x0 - dx_, y0 - dy_, x1 - dx_, y1 - dy_);
    cursor_x_ = x0 - dx_;
    cursor_y_ = y0 - dy_;

    addr_stream_.reset(address_window_);
    output_->setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    if (pixel_count == 0) return;
    Color raster_color[pixel_count];

    // if (addr_stream_.has_stream()) {
    uint32_t run_length = 0;
    addr_stream_.read(raster_color, pixel_count, run_length);
    uint32_t run = std::min(run_length, pixel_count);
    if (run > 0) {
      // Blend the run.
      blendWithUniformRasterColorInPlace(color, run, raster_color[0]);
    }
    // Blend the remaining pixels.
    for (uint32_t i = run; i < pixel_count; ++i) {
      color[i] = blender_.blend(raster_color[i], color[i]);
    }
    internal::BlendWithBackground(color, pixel_count, bgcolor_);
    output_->write(color, pixel_count);
    advanceCursor(pixel_count);
  }

  void fill(Color color, uint32_t pixel_count) override {
    if (pixel_count == 0) return;
    Color newcolor[kPixelWritingBufferSize];
    uint32_t processed = pixel_count;

    // if (addr_stream_.has_stream()) {
    while (pixel_count > 0) {
      uint16_t batch = pixel_count > kPixelWritingBufferSize
                           ? kPixelWritingBufferSize
                           : pixel_count;
      uint32_t run_length = 0;
      addr_stream_.read(newcolor, batch, run_length);
      uint32_t run = std::min<uint32_t>(run_length, pixel_count);
      if (run > 0) {
        Color blended = blender_.blend(newcolor[0], color);
        blended = AlphaBlend(bgcolor_, blended);
        if (run >= batch) {
          output_->fill(blended, run);
          addr_stream_.skip(run - batch);
          pixel_count -= run;
          continue;
        }
        FillColor(newcolor, run, blended);
      }
      blendUniformSourceColorInPlace(newcolor + run, batch - run, color);
      internal::BlendWithBackground(newcolor + run, batch - run, bgcolor_);
      output_->write(newcolor, batch);
      pixel_count -= batch;
    }
    advanceCursor(processed);
  }

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
          output_->fillRects(
              mode, AlphaBlend(bgcolor_, blender_.blend(uniform, color)), x0,
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
      newcolor[i] = blender_.blend(newcolor[i], color[i]);
    }
    internal::BlendWithBackground(newcolor, pixel_count, bgcolor_);
    output_->writePixels(mode, newcolor, x, y, pixel_count);
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    if (pixel_count == 0) return;
    bool transparent_src = color.a() == 0;
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
          output_->fillPixels(
              mode, AlphaBlend(bgcolor_, blender_.blend(uniform, color)), x, y,
              pixel_count);
          return;
        }
      }
    }
    Color newcolor[pixel_count];
    read(x, y, pixel_count, newcolor);
    if (transparent_src) {
      for (uint32_t i = 0; i < pixel_count; ++i) {
        newcolor[i] = blender_.blendTransparentSrc(newcolor[i], color);
      }
    } else {
      for (uint32_t i = 0; i < pixel_count; ++i) {
        newcolor[i] = blender_.blend(newcolor[i], color);
      }
    }
    internal::BlendWithBackground(newcolor, pixel_count, bgcolor_);
    output_->writePixels(mode, newcolor, x, y, pixel_count);
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
                          AlphaBlend(bgcolor_, blender_.blend(uniform, color)));
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
    bool transparent_src = color.a() == 0;
    Color newcolor[pixel_count];
    bool same = raster_->readColorRect(xMin - dx_, yMin - dy_, xMax - dx_,
                                       yMax - dy_, newcolor);
    if (same) {
      output_->fillRect(
          mode, Box(xMin, yMin, xMax, yMax),
          AlphaBlend(bgcolor_, blender_.blend(newcolor[0], color)));
    } else {
      if (transparent_src) {
        for (uint16_t i = 0; i < pixel_count; ++i) {
          newcolor[i] = blender_.blendTransparentSrc(newcolor[i], color);
        }
      } else {
        for (uint16_t i = 0; i < pixel_count; ++i) {
          newcolor[i] = blender_.blend(newcolor[i], color);
        }
      }
      internal::BlendWithBackground(newcolor, pixel_count, bgcolor_);
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

  inline __attribute__((always_inline)) void blendWithUniformRasterColorInPlace(
      Color* buf, uint32_t pixel_count, Color raster_color) {
    if (raster_color.a() == 0) {
      for (uint32_t i = 0; i < pixel_count; ++i) {
        buf[i] = blender_.blendTransparentDst(raster_color, buf[i]);
      }
    } else {
      for (uint32_t i = 0; i < pixel_count; ++i) {
        buf[i] = blender_.blend(raster_color, buf[i]);
      }
    }
  }

  inline __attribute__((always_inline)) void blendUniformSourceColorInPlace(
      Color* buf, uint32_t pixel_count, Color source_color) {
    if (source_color.a() == 0) {
      for (uint32_t i = 0; i < pixel_count; ++i) {
        buf[i] = blender_.blendTransparentSrc(buf[i], source_color);
      }
    } else {
      for (uint32_t i = 0; i < pixel_count; ++i) {
        buf[i] = blender_.blend(source_color, buf[i]);
      }
    }
  }

  DisplayOutput* output_;
  Capabilities capabilities_;
  Blender blender_;
  const Rasterizable* raster_;
  internal::WindowedPixelStream addr_stream_;
  Box address_window_;
  int16_t cursor_x_;
  int16_t cursor_y_;
  int16_t dx_;
  int16_t dy_;
  Color bgcolor_;
};

}  // namespace roo_display
