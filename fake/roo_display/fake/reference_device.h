#pragma once

#include <memory>

#include "roo_display/core/device.h"
#include "roo_display/products/combo_device.h"
#include "roo_testing/transducers/ui/viewport/viewport.h"

namespace roo_display {

class ReferenceDisplayDevice : public DisplayDevice {
 public:
  ReferenceDisplayDevice(int w, int h,
                         roo_testing_transducers::Viewport& viewport);

  ~ReferenceDisplayDevice() {}

  void init() override;
  void begin() override;
  void end() override;

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  roo_display::BlendingMode blending_mode) override {
    addr_x0_ = x0;
    addr_y0_ = y0;
    addr_x1_ = x1;
    addr_y1_ = y1;
    blending_mode_ = blending_mode;
    cursor_x_ = addr_x0_;
    cursor_y_ = addr_y0_;
  }

  void write(roo_display::Color* colors, uint32_t pixel_count) override;

  //  void fill(roo_display::PaintMode mode, roo_display::Color color, uint32_t
  //  pixel_count) override;

  void writeRects(roo_display::BlendingMode mode, roo_display::Color* color,
                  int16_t* x0, int16_t* y0, int16_t* x1, int16_t* y1,
                  uint16_t count) override;

  void fillRects(roo_display::BlendingMode mode, roo_display::Color color,
                 int16_t* x0, int16_t* y0, int16_t* x1, int16_t* y1,
                 uint16_t count) override;

  void writePixels(roo_display::BlendingMode mode, roo_display::Color* colors,
                   int16_t* xs, int16_t* ys, uint16_t pixel_count) override;

  void fillPixels(roo_display::BlendingMode mode, roo_display::Color color,
                  int16_t* xs, int16_t* ys, uint16_t pixel_count) override;

  void setBgColorHint(roo_display::Color bgcolor) override {
    bgcolor_ = bgcolor;
  }

 private:
  static roo_display::Color effective_color(roo_display::BlendingMode mode,
                                            roo_display::Color color,
                                            roo_display::Color bgcolor);

  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                roo_display::Color color);

  void advance();

  roo_testing_transducers::Viewport& viewport_;
  roo_display::Color bgcolor_;
  int16_t addr_x0_, addr_y0_, addr_x1_, addr_y1_;
  roo_display::BlendingMode blending_mode_;
  int16_t cursor_x_, cursor_y_;
};

class ReferenceTouchDevice : public roo_display::TouchDevice {
 public:
  ReferenceTouchDevice(roo_testing_transducers::Viewport& viewport);

  roo_display::TouchResult getTouch(roo_display::TouchPoint* points,
                                    int max_points) override;

 private:
  roo_testing_transducers::Viewport& viewport_;
};

class ReferenceComboDevice : public roo_display::ComboDevice {
 public:
  ReferenceComboDevice(int w, int h,
                       roo_testing_transducers::Viewport& viewport)
      : display_(w, h, viewport), touch_(viewport) {}

  roo_display::DisplayDevice& display() override { return display_; }

  roo_display::TouchDevice* touch() override { return &touch_; }

  void initTransport() {}

 private:
  ReferenceDisplayDevice display_;
  ReferenceTouchDevice touch_;
};

}  // namespace roo_display