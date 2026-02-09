#pragma once

#include <algorithm>
#include <vector>

#include "roo_testing/transducers/ui/viewport/viewport.h"

namespace testing {

class TestViewport : public roo_testing_transducers::Viewport {
 public:
  void init(int16_t width, int16_t height) override {
    Viewport::init(width, height);
    buffer_.assign(static_cast<size_t>(width) * height, 0xFF000000);
  }

  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                uint32_t color_argb) override {
    clampToBounds(x0, y0, x1, y1);
    if (x0 > x1 || y0 > y1) return;
    for (int16_t y = y0; y <= y1; ++y) {
      for (int16_t x = x0; x <= x1; ++x) {
        buffer_[index(x, y)] = color_argb;
      }
    }
  }

  void drawRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                const uint32_t* color_argb) override {
    clampToBounds(x0, y0, x1, y1);
    if (x0 > x1 || y0 > y1) return;
    int16_t width = x1 - x0 + 1;
    int16_t height = y1 - y0 + 1;
    size_t idx = 0;
    for (int16_t y = 0; y < height; ++y) {
      for (int16_t x = 0; x < width; ++x) {
        buffer_[index(x0 + x, y0 + y)] = color_argb[idx++];
      }
    }
  }

  bool isMouseClicked(int16_t* x, int16_t* y) override {
    if (!mouse_pressed_) return false;
    *x = mouse_x_;
    *y = mouse_y_;
    return true;
  }

  void setMouse(int16_t x, int16_t y, bool pressed) {
    mouse_x_ = x;
    mouse_y_ = y;
    mouse_pressed_ = pressed;
  }

  uint32_t getPixel(int16_t x, int16_t y) const {
    if (x < 0 || y < 0 || x >= width() || y >= height()) return 0;
    return buffer_[index(x, y)];
  }

  void clear(uint32_t color_argb) {
    std::fill(buffer_.begin(), buffer_.end(), color_argb);
  }

 private:
  size_t index(int16_t x, int16_t y) const {
    return static_cast<size_t>(x) + static_cast<size_t>(y) * width();
  }

  void clampToBounds(int16_t& x0, int16_t& y0, int16_t& x1, int16_t& y1) const {
    x0 = std::max<int16_t>(0, std::min<int16_t>(x0, width() - 1));
    x1 = std::max<int16_t>(0, std::min<int16_t>(x1, width() - 1));
    y0 = std::max<int16_t>(0, std::min<int16_t>(y0, height() - 1));
    y1 = std::max<int16_t>(0, std::min<int16_t>(y1, height() - 1));
  }

  std::vector<uint32_t> buffer_;
  int16_t mouse_x_ = 0;
  int16_t mouse_y_ = 0;
  bool mouse_pressed_ = false;
};

}  // namespace testing
