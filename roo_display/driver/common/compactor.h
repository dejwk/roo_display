#pragma once

#include "roo_display/core/device.h"
#include "roo_display/core/memfill.h"

namespace roo_display {

// Helper class that can turn a streak of pixel writes that happen to be on a
// horizontal or vertical direction into a single write or fill request to the
// underlying device, minimizing address window changes.
//
// To use the compactor, call it from writePixels and fillPixels, and give it
// the following functor:
// void write(int16_t offset, int16_t x, int16_t y,
//            Compactor::WriteDirection direction, int16_t count);
// };
//
// When called, the functor should write or fill the specified count of pixels,
// starting at x, y, and in the specified direction. The offset argument
// indicates the absolute pixel index in the input array. It can be used to
// determine colors for writing / filling. See addr_window_device.h for an
// example.
class Compactor {
 public:
  Compactor() {}
  template <typename Writer>
  void drawPixels(int16_t* xs, int16_t* ys, uint16_t pixel_count,
                  Writer write = Writer()) {
    if (pixel_count == 0) return;

    int i = 0;
    int count = 0;
    uint16_t x;
    uint16_t y;

    x = xs[0];
    y = ys[0];
    uint16_t x_start;
    uint16_t y_start;

    while (i < pixel_count) {
      int start = i;
      ++i;
      if (i == pixel_count) {
        write(start, x, y, RIGHT, 1);
        return;
      }
      x_start = x;
      y_start = y;
      x = xs[i];
      y = ys[i];
      count = 1;
      if (y == y_start && x == x_start + 1) {
        // Horizontal streak.
        do {
          ++count;
          ++i;
          if (i == pixel_count) break;
          x = xs[i];
          y = ys[i];
        } while (y == y_start && x == x_start + count);
        write(start, x_start, y_start, RIGHT, count);
      } else if (x == x_start && y == y_start + 1) {
        // Vertical streak.
        do {
          ++count;
          ++i;
          if (i == pixel_count) break;
          x = xs[i];
          y = ys[i];
        } while (x == x_start && y == y_start + count);
        write(start, x_start, y_start, DOWN, count);
      } else if (y == y_start && x == x_start - 1) {
        // Inverse horizontal streak.
        do {
          ++count;
          ++i;
          if (i == pixel_count) break;
          x = xs[i];
          y = ys[i];
        } while (y == y_start && x == x_start - count);
        write(start, x_start, y_start, LEFT, count);
      } else if (x == x_start && y == y_start - 1) {
        // Vertical streak.
        do {
          ++count;
          ++i;
          if (i == pixel_count) break;
          x = xs[i];
          y = ys[i];
        } while (x == x_start && y == y_start - count);
        write(start, x_start, y_start, UP, count);
      } else {
        write(start, x_start, y_start, RIGHT, 1);
      }
    }
  }

  enum WriteDirection { RIGHT, DOWN, LEFT, UP };
};

}  // namespace roo_display
