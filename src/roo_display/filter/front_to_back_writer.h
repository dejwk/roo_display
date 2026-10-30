#pragma once

#include <cstring>

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
      mask_filter_.fillRect(mode, x0[i], y0[i], x1[i], y1[i], color[i]);
      markRect(mode, Box(x0[i], y0[i], x1[i], y1[i]));
    }
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    for (int i = 0; i < count; i++) {
      mask_filter_.fillRect(mode, x0[i], y0[i], x1[i], y1[i], color);
      markRect(mode, Box(x0[i], y0[i], x1[i], y1[i]));
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
    if (mode == BlendingMode::kSource || mode == BlendingMode::kSourceOver) {
      offscreen_.buffer()[y * ((mask_.bounds().width() + 7) / 8) + x / 8] |=
          roo::byte{0x80} >> (x % 8);
    } else {
      offscreen_.output().fillPixels(mode, color::Black, &x, &y, 1);
    }
  }

  // Common paints set opaque coverage. Mark whole bytes rather than sending
  // every mask pixel through the generic offscreen color/blending pipeline.
  void markRect(BlendingMode mode, const Box& rect) {
    Box clipped = Box::Intersect(mask_.bounds(), rect);
    if (clipped.empty()) return;
    clipped = clipped.translate(-mask_.bounds().xMin(), -mask_.bounds().yMin());
    // Preserve the existing mask-blending semantics for uncommon modes.
    if (mode != BlendingMode::kSource && mode != BlendingMode::kSourceOver) {
      offscreen_.output().fillRect(mode, clipped, color::Black);
      return;
    }
    const int stride = (mask_.bounds().width() + 7) / 8;
    const int first = clipped.xMin() / 8;
    const int last = clipped.xMax() / 8;
    const roo::byte head = roo::byte{0xFF} >> (clipped.xMin() % 8);
    const roo::byte tail = roo::byte{0xFF} << (7 - clipped.xMax() % 8);
    for (int y = clipped.yMin(); y <= clipped.yMax(); ++y) {
      roo::byte* row = offscreen_.buffer() + y * stride;
      if (first == last) {
        row[first] |= head & tail;
      } else {
        row[first] |= head;
        std::memset(row + first + 1, 0xFF, last - first - 1);
        row[last] |= tail;
      }
    }
  }

  DisplayOutput& output_;
  Capabilities capabilities_;
  BitMaskOffscreen offscreen_;
  ClipMask mask_;
  ClipMaskFilter mask_filter_;
};

}  // namespace roo_display
