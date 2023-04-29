#include "roo_display/composition/rasterizable_stack.h"

namespace roo_display {

namespace {
static const int kMaxBufSize = 64;
}

void RasterizableStack::readColors(const int16_t* x, const int16_t* y,
                                   uint32_t count, Color* result) const {
  FillColor(result, count, color::Transparent);
  int16_t newx[kMaxBufSize];
  int16_t newy[kMaxBufSize];
  Color newresult[kMaxBufSize];
  uint32_t offsets[kMaxBufSize];
  for (auto r = inputs_.begin(); r != inputs_.end(); r++) {
    Box bounds = r->extents();
    uint32_t offset = 0;
    while (offset < count) {
      int buf_size = 0;
      uint32_t start_offset = offset;
      do {
        if (bounds.contains(x[offset], y[offset])) {
          newx[buf_size] = x[offset] - r->dx();
          newy[buf_size] = y[offset] - r->dy();
          offsets[buf_size] = offset;
          ++buf_size;
        }
        offset++;
      } while (offset < count && buf_size < kMaxBufSize);
      r->source()->readColors(newx, newy, buf_size, newresult);
      int buf_idx = 0;
      for (uint32_t i = start_offset; i < offset; ++i) {
        if (buf_idx < buf_size && offsets[buf_idx] == i) {
          // Found point in the bounds for which we have the color.
          result[i] = AlphaBlend(result[i], newresult[buf_idx]);
          ++buf_idx;
        }
      }
    }
  }
}

bool RasterizableStack::readColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                                      int16_t yMax, Color* result) const {
  bool is_uniform_color = true;
  *result = color::Transparent;
  Box box(xMin, yMin, xMax, yMax);
  Color buffer[box.area()];
  for (auto r = inputs_.begin(); r != inputs_.end(); r++) {
    Box bounds = r->extents();
    Box clipped = Box::Intersect(bounds, box);
    if (clipped.empty()) {
      // This rect does not contribute to the outcome.
      continue;
    }
    if (is_uniform_color && !clipped.contains(box)) {
      // This rect does not fill the entire box; we can no longer use fast path.
      is_uniform_color = false;
      FillColor(&result[1], box.area() - 1, *result);
    }
    if (r->source()->readColorRect(
            clipped.xMin() - r->dx(), clipped.yMin() - r->dy(),
            clipped.xMax() - r->dx(), clipped.yMax() - r->dy(), buffer)) {
      if (is_uniform_color) {
        *result = AlphaBlend(*result, *buffer);
      } else {
        for (int16_t y = clipped.yMin(); y <= clipped.yMax(); ++y) {
          Color* row = &result[(y - yMin) * box.width()];
          for (int16_t x = clipped.xMin(); x <= clipped.xMax(); ++x) {
            row[x - xMin] = AlphaBlend(row[x - xMin], *buffer);
          }
        }
      }
    } else {
      if (is_uniform_color) {
        is_uniform_color = false;
        FillColor(&result[1], box.area() - 1, *result);
      }
      uint32_t i = 0;
      for (int16_t y = clipped.yMin(); y <= clipped.yMax(); ++y) {
        Color* row = &result[(y - yMin) * box.width()];
        for (int16_t x = clipped.xMin(); x <= clipped.xMax(); ++x) {
          row[x - xMin] = AlphaBlend(row[x - xMin], buffer[i++]);
        }
      }
    }
  }
  return is_uniform_color;
}

}  // namespace roo_display