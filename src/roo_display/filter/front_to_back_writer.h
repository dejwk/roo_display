#pragma once

#include "roo_display/color/color.h"
#include "roo_display/color/named.h"
#include "roo_display/core/device.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/filter/clip_mask.h"

namespace roo_display {

/// Writer that ensures front-most pixels are written first.
///
/// Uses an offscreen mask to avoid overwriting already-drawn pixels.
class FrontToBackWriter : public DisplayOutput {
 public:
  /// Construct a front-to-back writer for a given bounds rectangle.
  ///
  /// The caller must guarantee bounds are within the output area and no writes
  /// go out of bounds.
  FrontToBackWriter(DisplayOutput& output, Box bounds)
      : output_(output),
        capabilities_(output.getCapabilities().supportsBlending(),
                      /*supports_blit_copy=*/false),
        offscreen_(bounds, color::Transparent),
        mask_(offscreen_.buffer(), bounds),
        mask_filter_(output, &mask_) {}

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    mask_filter_.setAddress(x0, y0, x1, y1, mode);
    int16_t dx = mask_.bounds().xMin();
    int16_t dy = mask_.bounds().yMin();
    offscreen_.output().setAddress(x0 - dx, y0 - dy, x1 - dx, y1 - dy, mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    mask_filter_.write(color, pixel_count);
    offscreen_.output().fill(color::Black, pixel_count);
  }

  void fill(Color color, uint32_t pixel_count) override {
    mask_filter_.fill(color, pixel_count);
    offscreen_.output().fill(color::Black, pixel_count);
  }

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    for (int i = 0; i < count; i++) {
      Box clipped =
          Box::Intersect(offscreen_.extents(), Box(x0[i], y0[i], x1[i], y1[i]));
      mask_filter_.fillRect(mode, x0[i], y0[i], x1[i], y1[i], color[i]);
      if (!clipped.empty()) {
        offscreen_.output().fillRect(
            mode,
            clipped.translate(-offscreen_.extents().xMin(),
                              -offscreen_.extents().yMin()),
            color::Black);
      }
    }
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    for (int i = 0; i < count; i++) {
      Box clipped =
          Box::Intersect(offscreen_.extents(), Box(x0[i], y0[i], x1[i], y1[i]));
      mask_filter_.fillRect(mode, x0[i], y0[i], x1[i], y1[i], color);
      if (!clipped.empty()) {
        offscreen_.output().fillRect(
            mode,
            clipped.translate(-offscreen_.extents().xMin(),
                              -offscreen_.extents().yMin()),
            color::Black);
      }
    }
  }

  void writePixels(BlendingMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    uint16_t visible = 0;
    for (uint16_t i = 0; i < pixel_count; ++i) {
      bool masked = mask_.isMasked(x[i], y[i]);
      markPixel(mode, x[i], y[i]);
      if (masked) continue;
      color[visible] = color[i];
      x[visible] = x[i];
      y[visible] = y[i];
      ++visible;
    }
    if (visible != 0) output_.writePixels(mode, color, x, y, visible);
    // Arbitrary writes may replace the output address window.
    mask_filter_.invalidateOutputWindow();
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    uint16_t visible = 0;
    for (uint16_t i = 0; i < pixel_count; ++i) {
      bool masked = mask_.isMasked(x[i], y[i]);
      markPixel(mode, x[i], y[i]);
      if (masked) continue;
      x[visible] = x[i];
      y[visible] = y[i];
      ++visible;
    }
    if (visible != 0) output_.fillPixels(mode, color, x, y, visible);
    mask_filter_.invalidateOutputWindow();
  }

  const ColorFormat& getColorFormat() const override {
    return output_.getColorFormat();
  }

  const Capabilities& getCapabilities() const override { return capabilities_; }

 private:
  // Mark before compacting so duplicates in this batch see earlier coverage.
  void markPixel(BlendingMode mode, int16_t x, int16_t y) {
    if (!mask_.bounds().contains(x, y)) return;
    x -= mask_.bounds().xMin();
    y -= mask_.bounds().yMin();
    offscreen_.output().fillPixels(mode, color::Black, &x, &y, 1);
  }

  DisplayOutput& output_;
  Capabilities capabilities_;
  BitMaskOffscreen offscreen_;
  ClipMask mask_;
  ClipMaskFilter mask_filter_;
};

}  // namespace roo_display
