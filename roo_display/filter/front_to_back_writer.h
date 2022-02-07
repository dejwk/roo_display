#pragma once

#include "roo_display/core/device.h"
#include "roo_display/core/offscreen.h"

namespace roo_display {

class FrontToBackWriter : public DisplayOutput {
 public:
  // The caller must guarantee that bounds are within the area accepted by the
  // output, and that there will be no write out ouf bounds.
  FrontToBackWriter(DisplayOutput& output, Box bounds)
      : offscreen_(bounds, color::Transparent),
        mask_(offscreen_.buffer(), bounds),
        mask_filter_(output, &mask_) {}

  ~FrontToBackWriter() {
    mask_filter_.fillRect(PAINT_MODE_REPLACE, mask_.bounds(), color::White);
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  PaintMode mode) override {
    mask_filter_.setAddress(x0, y0, x1, y1, mode);
    offscreen_.output().setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    mask_filter_.write(color, pixel_count);
    Color::Fill(color, pixel_count, color::Black);
    offscreen_.output().write(color, pixel_count);
  }

  void writeRects(PaintMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    for (int i = 0; i < count; i++) {
      Box clipped =
          Box::intersect(offscreen_.extents(), Box(x0[i], y0[i], x1[i], y1[i]));
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

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    for (int i = 0; i < count; i++) {
      Box clipped =
          Box::intersect(offscreen_.extents(), Box(x0[i], y0[i], x1[i], y1[i]));
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

  void writePixels(PaintMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    for (int i = 0; i < pixel_count; ++i) {
      int16_t cx = x[i];
      int16_t cy = y[i];
      mask_filter_.fillPixels(mode, color[i], &x[i], &y[i], 1);
      if (offscreen_.extents().contains(cx, cy)) {
        cx -= offscreen_.extents().xMin();
        cy -= offscreen_.extents().yMin();
        offscreen_.output().fillPixels(mode, color::Black, &cx, &cy, 1);
      }
    }
    // // TODO: remove duplicates.
    // int16_t x_copy[pixel_count];
    // std::copy(x, x + pixel_count, x_copy);
    // int16_t y_copy[pixel_count];
    // std::copy(y, y + pixel_count, y_copy);
    // mask_filter_.writePixels(mode, color, x, y, pixel_count);
    // // offscreen_.output().fillPixels(mode, color::Black, x_copy, y_copy,
    // //                                pixel_count);
  }

  void fillPixels(PaintMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    for (int i = 0; i < pixel_count; ++i) {
      int16_t cx = x[i];
      int16_t cy = y[i];
      mask_filter_.fillPixels(mode, color, &x[i], &y[i], 1);
      if (offscreen_.extents().contains(cx, cy)) {
        cx -= offscreen_.extents().xMin();
        cy -= offscreen_.extents().yMin();
        offscreen_.output().fillPixels(mode, color::Black, &cx, &cy, 1);
      }
    }

    // int16_t x_copy[pixel_count];
    // std::copy(x, x + pixel_count, x_copy);
    // int16_t y_copy[pixel_count];
    // std::copy(y, y + pixel_count, y_copy);
    // mask_filter_.fillPixels(mode, color, x, y, pixel_count);
    // // offscreen_.output().fillPixels(mode, color::Black, x_copy, y_copy,
    // //                                pixel_count);
  }

 private:
  BitMaskOffscreen offscreen_;
  ClipMask mask_;
  ClipMaskFilter mask_filter_;
};

}  // namespace roo_display
