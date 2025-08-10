#include "fake/roo_display/fake/reference_device.h"

#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_time.h"

namespace roo_display {

void ReferenceDisplayDevice::fillRect(int16_t x0, int16_t y0, int16_t x1,
                                      int16_t y1, Color color) {
  if (orientation().isXYswapped()) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  if (orientation().isBottomToTop()) {
    std::swap(y0, y1);
    y0 = raw_height() - y0 - 1;
    y1 = raw_height() - y1 - 1;
  }
  if (orientation().isRightToLeft()) {
    std::swap(x0, x1);
    x0 = raw_width() - x0 - 1;
    x1 = raw_width() - x1 - 1;
  }
  viewport_.fillRect(x0, y0, x1, y1, color.asArgb());
}

ReferenceDisplayDevice::ReferenceDisplayDevice(
    int w, int h, roo_testing_transducers::Viewport &viewport)
    : DisplayDevice(w, h), viewport_(viewport), bgcolor_(0xFF7F7F7F) {
  // viewport_.init(w, h);
}

void ReferenceDisplayDevice::begin() {}
void ReferenceDisplayDevice::end() { viewport_.flush(); }

Color ReferenceDisplayDevice::effective_color(roo_display::BlendingMode mode,
                                              Color color, Color bgcolor) {
  return color;  // roo_display::ApplyBlending(mode, bgcolor, color);
}

void ReferenceDisplayDevice::write(roo_display::Color *colors,
                                   uint32_t pixel_count) {
  for (uint32_t i = 0; i < pixel_count; ++i) {
    fillRect(cursor_x_, cursor_y_, cursor_x_, cursor_y_,
             effective_color(blending_mode_, colors[i], bgcolor_));
    advance();
  }
}

// void ReferenceDevice::fill(PaintMode mode, roo_display::Color color,
//                           uint32_t pixel_count) {
//   for (uint32_t i = 0; i < pixel_count; ++i) {
//     rectFill(cursor_x_, cursor_y_, 1, 1, effective_color(mode, color));
//     advance();
//   }
// }

void ReferenceDisplayDevice::writeRects(BlendingMode mode, Color *color,
                                        int16_t *x0, int16_t *y0, int16_t *x1,
                                        int16_t *y1, uint16_t count) {
  while (count-- > 0) {
    fillRect(*x0++, *y0++, *x1++, *y1++,
             effective_color(mode, *color++, bgcolor_));
  }
}

void ReferenceDisplayDevice::fillRects(BlendingMode mode, Color color,
                                       int16_t *x0, int16_t *y0, int16_t *x1,
                                       int16_t *y1, uint16_t count) {
  while (count-- > 0) {
    fillRect(*x0++, *y0++, *x1++, *y1++,
             effective_color(mode, color, bgcolor_));
  }
}

void ReferenceDisplayDevice::writePixels(BlendingMode mode,
                                         roo_display::Color *colors,
                                         int16_t *xs, int16_t *ys,
                                         uint16_t pixel_count) {
  for (int i = 0; i < pixel_count; ++i) {
    fillRect(xs[i], ys[i], xs[i], ys[i],
             effective_color(mode, colors[i], bgcolor_));
  }
}

void ReferenceDisplayDevice::fillPixels(BlendingMode mode,
                                        roo_display::Color color, int16_t *xs,
                                        int16_t *ys, uint16_t pixel_count) {
  for (int i = 0; i < pixel_count; ++i) {
    fillRect(xs[i], ys[i], xs[i], ys[i],
             effective_color(mode, color, bgcolor_));
  }
}

void ReferenceDisplayDevice::advance() {
  ++cursor_x_;
  if (cursor_x_ > addr_x1_) {
    cursor_x_ = addr_x0_;
    ++cursor_y_;
  }
}

void ReferenceDisplayDevice::init() {
  viewport_.init(raw_width(), raw_height());
  viewport_.fillRect(0, 0, raw_width(), raw_height(), 0xFFF08080);
}

ReferenceTouchDevice::ReferenceTouchDevice(
    roo_testing_transducers::Viewport &viewport)
    : TouchDevice(), viewport_(viewport) {}

roo_display::TouchResult ReferenceTouchDevice::getTouch(
    roo_display::TouchPoint *points, int max_points) {
  int16_t w = viewport_.width();
  int16_t h = viewport_.height();
  if (w < 0 || h < 0) {
    // Viewport not yet initialized / visible.
    return roo_display::TouchResult();
  }
  int16_t x_display, y_display;
  bool result = viewport_.isMouseClicked(&x_display, &y_display);
  if (result) {
    roo_display::TouchPoint &tp = points[0];
    tp.id = 1;
    tp.x = (int32_t)4096 * x_display / w;
    tp.y = (int32_t)4096 * y_display / h;
    tp.z = 100;
    return TouchResult(roo_time::Uptime::Now().inMillis(), 1);
  }
  return TouchResult();
}

}  // namespace roo_display
