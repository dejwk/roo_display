#include "roo_display/core/device.h"

namespace roo_display {

void DisplayOutput::drawDirectRect(const roo::byte *data,
                                   size_t row_width_bytes, int16_t src_x0,
                                   int16_t src_y0, int16_t src_x1,
                                   int16_t src_y1, int16_t dst_x0,
                                   int16_t dst_y0) {
  if (src_x1 < src_x0 || src_y1 < src_y0) return;

  static constexpr int16_t kTileSize = 8;
  Color buffer[kTileSize * kTileSize];

  for (int16_t y = src_y0; y <= src_y1; y += kTileSize) {
    int16_t tile_h = src_y1 - y + 1;
    if (tile_h > kTileSize) tile_h = kTileSize;
    for (int16_t x = src_x0; x <= src_x1; x += kTileSize) {
      int16_t tile_w = src_x1 - x + 1;
      if (tile_w > kTileSize) tile_w = kTileSize;
      interpretRect(data, row_width_bytes, x, y, x + tile_w - 1, y + tile_h - 1,
                    buffer);
      int16_t dst_tile_x0 = dst_x0 + (x - src_x0);
      int16_t dst_tile_y0 = dst_y0 + (y - src_y0);
      int16_t dst_tile_x1 = dst_tile_x0 + tile_w - 1;
      int16_t dst_tile_y1 = dst_tile_y0 + tile_h - 1;

      setAddress(dst_tile_x0, dst_tile_y0, dst_tile_x1, dst_tile_y1,
                 BLENDING_MODE_SOURCE);
      write(buffer, tile_w * tile_h);
    }
  }
}

}  // namespace roo_display