#pragma once

#include <memory>
#include <vector>

#include "roo_display/color/color_modes.h"
#include "roo_display/core/device.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/core/rasterizable.h"
#include "roo_display/internal/color_format.h"
#include "roo_display/internal/color_io.h"
#include "roo_display/products/combo_device.h"
#include "roo_testing/transducers/ui/viewport/viewport.h"

namespace roo_display {

class ReferenceDisplayDevice : public DisplayDevice {
 public:
  enum class ColorMode { kRgb888, kRgb565, kGrayscale8, kGrayscale1 };

  ReferenceDisplayDevice(int w, int h,
                         roo_testing_transducers::Viewport& viewport);

  ReferenceDisplayDevice(int w, int h, ColorMode color_mode, bool blendable,
                         roo_testing_transducers::Viewport& viewport);

  ~ReferenceDisplayDevice() {}

  void init() override;
  void begin() override;
  void end() override;

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  roo_display::BlendingMode blending_mode) override;

  void write(roo_display::Color* colors, uint32_t pixel_count) override;

  void fill(roo_display::Color color, uint32_t pixel_count) override;

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

  const ColorFormat& getColorFormat() const override;

  const Capabilities& getCapabilities() const override;

  void drawDirectRect(const roo::byte* data, size_t row_width_bytes,
                      int16_t src_x0, int16_t src_y0, int16_t src_x1,
                      int16_t src_y1, int16_t dst_x0, int16_t dst_y0) override;

  void blitCopy(int16_t src_x0, int16_t src_y0, int16_t src_x1, int16_t src_y1,
                int16_t dst_x0, int16_t dst_y0) override;

  void orientationUpdated() override;

 private:
  template <typename Mode>
  void initBackbuffer(const Mode& mode, roo_display::Color initial);

  void blitRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1);

  roo_display::Color preblend(roo_display::BlendingMode mode,
                              roo_display::Color color) const;

  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                roo_display::BlendingMode mode, roo_display::Color color);

  void blitPixels(int16_t* xs, int16_t* ys, uint16_t pixel_count);

  void blitAdvancedRegion(int16_t start_x, int16_t start_y, int16_t end_x,
                          int16_t end_y);

  void advance(uint32_t pixel_count);

  roo_testing_transducers::Viewport& viewport_;
  roo_display::Color bgcolor_;
  int16_t addr_x0_, addr_y0_, addr_x1_, addr_y1_;
  int16_t cursor_x_, cursor_y_;
  roo_display::BlendingMode blending_mode_;
  ColorMode color_mode_;
  bool blendable_;
  std::unique_ptr<roo::byte[]> backbuffer_storage_;
  std::unique_ptr<Rasterizable> rasterizable_;
  std::unique_ptr<DisplayDevice> output_device_;
  internal::Orienter orienter_;
  internal::AddressWindow address_window_;
  std::vector<roo_display::Color> readback_color_tmp_;
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
      : ReferenceComboDevice(w, h, ReferenceDisplayDevice::ColorMode::kRgb565,
                             false, viewport) {}

  ReferenceComboDevice(int w, int h,
                       ReferenceDisplayDevice::ColorMode color_mode,
                       bool blendable,
                       roo_testing_transducers::Viewport& viewport)
      : display_(w, h, color_mode, blendable, viewport), touch_(viewport) {}

  roo_display::DisplayDevice& display() override { return display_; }

  roo_display::TouchDevice* touch() override { return &touch_; }

  void initTransport() {}

 private:
  ReferenceDisplayDevice display_;
  ReferenceTouchDevice touch_;
};

}  // namespace roo_display