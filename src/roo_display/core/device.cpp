#include "roo_display/core/device.h"

#include "roo_io/memory/fill.h"

namespace roo_display {

void DisplayOutput::fill(Color color, uint32_t pixel_count) {
  if (pixel_count == 0) return;
  static constexpr uint32_t kChunkSize = 64;
  Color chunk[kChunkSize];

  if (pixel_count < kChunkSize) {
    roo_io::PatternFill<4>((roo::byte *)chunk, pixel_count,
                           (const roo::byte *)(&color));
    write(chunk, pixel_count);
    return;
  }

  roo_io::PatternFill<4>((roo::byte *)chunk, kChunkSize,
                         (const roo::byte *)(&color));

  const uint32_t remainder = pixel_count % kChunkSize;
  if (remainder > 0) {
    write(chunk, remainder);
  }

  uint32_t full_blocks = pixel_count / kChunkSize;
  while (full_blocks-- > 0) {
    write(chunk, kChunkSize);
  }
}

void DisplayOutput::drawDirectRect(const roo::byte *data,
                                   size_t row_width_bytes, int16_t src_x0,
                                   int16_t src_y0, int16_t src_x1,
                                   int16_t src_y1, int16_t dst_x0,
                                   int16_t dst_y0) {
  const ColorFormat &color_format = getColorFormat();
  if (src_x1 < src_x0 || src_y1 < src_y0) return;

  static constexpr int16_t kTileSize = 8;
  Color buffer[kTileSize * kTileSize];

  for (int16_t y = src_y0; y <= src_y1; y += kTileSize) {
    int16_t tile_h = src_y1 - y + 1;
    if (tile_h > kTileSize) tile_h = kTileSize;
    for (int16_t x = src_x0; x <= src_x1; x += kTileSize) {
      int16_t tile_w = src_x1 - x + 1;
      if (tile_w > kTileSize) tile_w = kTileSize;
      color_format.decode(data, row_width_bytes, x, y, x + tile_w - 1,
                          y + tile_h - 1, buffer);
      int16_t dst_tile_x0 = dst_x0 + (x - src_x0);
      int16_t dst_tile_y0 = dst_y0 + (y - src_y0);
      int16_t dst_tile_x1 = dst_tile_x0 + tile_w - 1;
      int16_t dst_tile_y1 = dst_tile_y0 + tile_h - 1;

      setAddress(dst_tile_x0, dst_tile_y0, dst_tile_x1, dst_tile_y1,
                 BlendingMode::kSource);
      write(buffer, tile_w * tile_h);
    }
  }
}

}  // namespace roo_display