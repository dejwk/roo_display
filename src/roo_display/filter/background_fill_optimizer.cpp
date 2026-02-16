#include "background_fill_optimizer.h"

namespace roo_display {

namespace {

// Adapter to use writer as filler, for a single rectangle.
struct RectFillWriter {
  Color color;
  BufferedRectWriter& writer;
  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1) const {
    writer.writeRect(x0, y0, x1, y1, color);
  }
};

inline uint8_t getIdxInPalette(Color color, const Color* palette,
                               uint8_t palette_size) {
  for (int i = 0; i < palette_size; ++i) {
    if (color == palette[i]) return i + 1;
  }
  return 0;
}

inline uint8_t tryAddIdxInPalette(Color color, Color* palette,
                                  uint8_t& palette_size) {
  if (palette_size < 15) {
    // Not found, but we have room to add it to the palette. Let's do it.
    // LOG(INFO) << "Adding color " << (int)color.r() << "," << (int)color.g()
    //           << "," << (int)color.b() << "," << (int)color.a()
    //           << " to palette at idx "
    //           << (int)(palette_size + 1);
    palette[palette_size] = color;
    return ++palette_size;
  }
  return 0;
}

}  // namespace

BackgroundFillOptimizer::FrameBuffer::FrameBuffer(int16_t width, int16_t height)
    : FrameBuffer(width, height,
                  new roo::byte[FrameBuffer::SizeForDimensions(width, height)],
                  true) {
  invalidate();
}

BackgroundFillOptimizer::FrameBuffer::FrameBuffer(int16_t width, int16_t height,
                                                  roo::byte* buffer)
    : FrameBuffer(width, height, buffer, false) {}

BackgroundFillOptimizer::FrameBuffer::FrameBuffer(int16_t width, int16_t height,
                                                  roo::byte* buffer,
                                                  bool owns_buffer)
    : background_mask_(
          buffer, ((((width - 1) / kBgFillOptimizerWindowSize + 1) + 1) / 2),
          ((((height - 1) / kBgFillOptimizerWindowSize + 1) + 1) / 2) * 2),
      palette_size_(0),
      swap_xy_(false),
      owned_buffer_(owns_buffer ? buffer : nullptr) {}

void BackgroundFillOptimizer::FrameBuffer::setPalette(const Color* palette,
                                                      uint8_t palette_size) {
  assert(palette_size <= 15);
  std::copy(palette, palette + palette_size, palette_);
  palette_size_ = palette_size;
}

void BackgroundFillOptimizer::FrameBuffer::setPalette(
    std::initializer_list<Color> palette) {
  assert(palette.size() <= 15);
  std::copy(palette.begin(), palette.end(), palette_);
  palette_size_ = palette.size();
}

void BackgroundFillOptimizer::FrameBuffer::setPrefilled(Color color) {
  prefilled(getIdxInPalette(color, palette_, palette_size_));
}

void BackgroundFillOptimizer::FrameBuffer::invalidate() { prefilled(0); }

void BackgroundFillOptimizer::FrameBuffer::prefilled(uint8_t idx_in_palette) {
  background_mask_.fillRect(
      Box(0, 0, background_mask_.width() - 1, background_mask_.height() - 1),
      idx_in_palette);
}

void BackgroundFillOptimizer::FrameBuffer::invalidateRect(const Box& rect) {
  background_mask_.fillRect(
      Box(rect.xMin() / roo_display::kBgFillOptimizerWindowSize,
          rect.yMin() / roo_display::kBgFillOptimizerWindowSize,
          rect.xMax() / roo_display::kBgFillOptimizerWindowSize,
          rect.yMax() / roo_display::kBgFillOptimizerWindowSize),
      0);
}

void BackgroundFillOptimizer::FrameBuffer::setSwapXY(bool swap) {
  if (swap == swap_xy_) return;
  swap_xy_ = swap;
  background_mask_ = internal::NibbleRect(background_mask_.buffer(),
                                          background_mask_.height() / 2,
                                          background_mask_.width());
  invalidate();
}

BackgroundFillOptimizer::BackgroundFillOptimizer(DisplayOutput& output,
                                                 FrameBuffer& buffer)
    : output_(output),
      background_mask_(&buffer.background_mask_),
      palette_(buffer.palette_),
      palette_size_(&buffer.palette_size_),
      address_window_(0, 0, 0, 0),
      background_mask_window_(0, 0, 0, 0),
      cursor_x_(0),
      cursor_y_(0),
      cursor_ord_(0),
      has_pending_dynamic_palette_color_(false),
      pending_dynamic_palette_color_(color::Transparent) {}

uint8_t BackgroundFillOptimizer::tryAddIdxInPaletteOnSecondConsecutiveColor(
    Color color) {
  if (has_pending_dynamic_palette_color_ &&
      pending_dynamic_palette_color_ == color) {
    has_pending_dynamic_palette_color_ = false;
    return tryAddIdxInPalette(color, palette_, *palette_size_);
  }
  pending_dynamic_palette_color_ = color;
  has_pending_dynamic_palette_color_ = true;
  return 0;
}

void BackgroundFillOptimizer::resetPendingDynamicPaletteColor() {
  has_pending_dynamic_palette_color_ = false;
}

void BackgroundFillOptimizer::setAddress(uint16_t x0, uint16_t y0, uint16_t x1,
                                         uint16_t y1, BlendingMode mode) {
  address_window_ = Box(x0, y0, x1, y1);
  background_mask_window_ =
      Box(x0 / kBgFillOptimizerWindowSize, y0 / kBgFillOptimizerWindowSize,
          (x1 + kBgFillOptimizerWindowSize - 1) / kBgFillOptimizerWindowSize,
          (y1 + kBgFillOptimizerWindowSize - 1) / kBgFillOptimizerWindowSize);
  blending_mode_ = mode;
  cursor_x_ = x0;
  cursor_y_ = y0;
  cursor_ord_ = 0;
}

void BackgroundFillOptimizer::write(Color* color, uint32_t pixel_count) {
  if (pixel_count == 0) return;

  const int16_t aw_width = address_window_.width();
  const uint32_t end_ord = cursor_ord_ + pixel_count - 1;
  const int16_t end_x =
      (address_window_.xMin() + static_cast<int16_t>(end_ord % aw_width));
  const int16_t end_y =
      address_window_.yMin() + static_cast<int16_t>(end_ord / aw_width);

  const int16_t end_block_y =
      (end_y + kBgFillOptimizerWindowSize - 1) / kBgFillOptimizerWindowSize;

  BufferedPixelWriter writer(output_, blending_mode_);

  const int16_t start_x = cursor_x_;
  const int16_t start_y = cursor_y_;
  const int16_t aw_x0 = address_window_.xMin();
  const int16_t aw_x1 = address_window_.xMax();

  // Process all mask blocks intersecting [x0..x1] on a given block-row.
  auto process_x_interval = [&](int16_t y_block, int16_t x0, int16_t x1,
                                int16_t first_row, int16_t last_row) {
    if (x1 < x0) return;
    int16_t bx0 = x0 / kBgFillOptimizerWindowSize;
    int16_t bx1 = x1 / kBgFillOptimizerWindowSize;
    for (int16_t bx = bx0; bx <= bx1; ++bx) {
      processWriteBlock(color, writer, cursor_ord_, end_ord, bx, y_block,
                        first_row, last_row);
    }
  };

  // IMPORTANT: streamed writes are row-major and may wrap inside the same
  // 4-pixel block-row. In that case touched blocks can be split into two
  // disjoint x-intervals (suffix of the first row + prefix of the last row).
  // We therefore derive touched x-interval(s) from pixel geometry per block-row
  // rather than linearly walking mask blocks from start to end.
  for (int16_t by = start_y / kBgFillOptimizerWindowSize; by <= end_block_y;
       ++by) {
    const int16_t first_row =
        std::max<int16_t>(start_y, by * kBgFillOptimizerWindowSize);
    const int16_t last_row =
        std::min<int16_t>(end_y, by * kBgFillOptimizerWindowSize +
                                     kBgFillOptimizerWindowSize - 1);
    if (first_row > last_row) continue;
    const int16_t row_count = last_row - first_row + 1;

    // #rows == 1: single-row fragment.
    if (row_count == 1) {
      const int16_t x0 = (first_row == start_y) ? start_x : aw_x0;
      const int16_t x1 = (first_row == end_y) ? end_x : aw_x1;
      process_x_interval(by, x0, x1, first_row, last_row);
      continue;
    }

    // #rows > 2: there is at least one full middle row, so the union of
    // touched pixels spans the full address-window width on this block-row.
    if (row_count > 2) {
      process_x_interval(by, aw_x0, aw_x1, first_row, last_row);
      continue;
    }

    // #rows == 2: only first and last row contribute; may be disjoint.
    // We still process full width if the two intervals overlap/touch.
    // Otherwise, process both intervals separately.
    //
    // first-row interval: [first_x0, aw_x1]
    // last-row interval : [aw_x0, last_x1]
    //
    // Disjointness criterion is first_x0 > last_x1 + 1.

    const int16_t first_x0 = (first_row == start_y) ? start_x : aw_x0;
    const int16_t first_x1 = aw_x1;
    const int16_t last_x0 = aw_x0;
    const int16_t last_x1 = (last_row == end_y) ? end_x : aw_x1;

    const bool contiguous = first_x0 <= last_x1 + 1;
    if (contiguous) {
      process_x_interval(by, aw_x0, aw_x1, first_row, last_row);
    } else {
      process_x_interval(by, first_x0, first_x1, first_row, last_row);
      process_x_interval(by, last_x0, last_x1, first_row, last_row);
    }
  }

  // Set to last touched pixel, then advance once in row-major order.
  cursor_x_ = end_x;
  cursor_y_ = end_y;
  if (cursor_x_ == address_window_.xMax()) {
    cursor_x_ = address_window_.xMin();
    ++cursor_y_;
  } else {
    ++cursor_x_;
  }
  cursor_ord_ = end_ord + 1;
}

void BackgroundFillOptimizer::processWriteBlock(
    Color* color, BufferedPixelWriter& writer, uint32_t start_ord,
    uint32_t end_ord, int16_t current_block_x, int16_t current_block_y,
    int16_t first_row, int16_t last_row) {
  // This method is called for each block of pixels corresponding to a single
  // nibble in the background mask, represented by the current_block_x and
  // current_block_y coordinates. The input is an array of colors, which is a
  // subsequence of the address window, indicated by start_ord and end_ord. The
  // caller must ensure that the color array intersects the specified block.
  // This method processes all pixels intersecting the block, ignoring all
  // others. It ensures that the pixels are drawn to the underlying device,
  // using background mask for optimization, i.e. skipping writes when possible.

  // This method:
  // * determines if all corresponding pixels in the block are equal, and if
  // they correspond to a single palette color,
  // * if not, clears the corresponding nibble in the background mask, and
  // write all pixels in the block to the underlying device using `writer`.
  // * if so, checks if the corresponding bit-mask rectangle is already marked
  // as covered by that palette color in the background mask; if it is, skip
  // writing the block to the underlying device;
  // * otherwise, checks if the block is fully covered by these pixels, and if
  // so, marks the corresponding nibble in the background mask as covered by
  // that palette color and writes the block to the underlying device using
  // fillRect;
  // * otherwise, falls back to writing all pixels in the block to the
  // underlying device using `writer`, and marks the corresponding nibble in the
  // background mask as no longer all-background.

  const int16_t aw_x0 = address_window_.xMin();
  const int16_t aw_y0 = address_window_.yMin();
  const int16_t aw_width = address_window_.width();

  const int16_t block_x0 = current_block_x * kBgFillOptimizerWindowSize;
  const int16_t block_y0 = current_block_y * kBgFillOptimizerWindowSize;
  const int16_t block_x1 = block_x0 + kBgFillOptimizerWindowSize - 1;
  const int16_t block_y1 = block_y0 + kBgFillOptimizerWindowSize - 1;

  // Iterate only covered pixels in this block, i.e. pixels belonging both to
  // this block and to [start_ord, end_ord]. Callback returns false to stop.
  auto for_each_covered_pixel = [&](auto&& fn) {
    uint32_t row_base_ord = static_cast<uint32_t>(first_row - aw_y0) *
                            static_cast<uint32_t>(aw_width);
    // `first_row..last_row` are precomputed by `write()` for this block-row and
    // already represent the only stream rows that can intersect this block.
    for (int16_t y = first_row; y <= last_row; ++y) {
      const uint32_t row_start_ord =
          std::max<uint32_t>(start_ord, row_base_ord);
      const uint32_t row_end_ord =
          std::min<uint32_t>(end_ord, row_base_ord + aw_width - 1);
      DCHECK_GE(row_end_ord, row_start_ord);
      int16_t row_x0 =
          aw_x0 + static_cast<int16_t>(row_start_ord - row_base_ord);
      int16_t row_x1 = aw_x0 + static_cast<int16_t>(row_end_ord - row_base_ord);
      row_x0 = std::max<int16_t>(row_x0, block_x0);
      row_x1 = std::min<int16_t>(row_x1, block_x1);
      if (row_x0 <= row_x1) {
        uint32_t ord = row_base_ord + static_cast<uint32_t>(row_x0 - aw_x0);
        for (int16_t x = row_x0; x <= row_x1; ++x, ++ord) {
          if (!fn(x, y, ord)) return;
        }
      }
      row_base_ord += static_cast<uint32_t>(aw_width);
    }
  };

  bool have_pixel = false;
  bool all_same = true;
  Color first_color = color::Transparent;
  uint8_t palette_idx = 0;
  // Number of streamed pixels from this write() chunk that actually land in the
  // current block. Used to detect "fully covered" block writes.
  uint8_t covered_count = 0;

  // Early-stop as soon as we know colors are not uniform.
  for_each_covered_pixel([&](int16_t, int16_t, uint32_t ord) {
    ++covered_count;
    const Color c = color[ord - start_ord];
    if (!have_pixel) {
      have_pixel = true;
      first_color = c;
      return true;
    }
    if (c != first_color) {
      all_same = false;
      return false;
    }
    return true;
  });

  if (!have_pixel) return;
  if (all_same) {
    palette_idx = getIdxInPalette(first_color, palette_, *palette_size_);
  }

  // `covered_count == kBgFillOptimizerWindowSize^2` implies full 4x4 block
  // coverage. Since we only count pixels that are both inside this block and
  // inside the address window, this cannot happen for a clipped (partial)
  // block.
  const bool fully_covered = (covered_count == kBgFillOptimizerWindowSize *
                                                   kBgFillOptimizerWindowSize);

  if (all_same && fully_covered) {
    if (palette_idx == 0) {
      palette_idx = tryAddIdxInPaletteOnSecondConsecutiveColor(first_color);
    } else {
      resetPendingDynamicPaletteColor();
    }
  } else {
    resetPendingDynamicPaletteColor();
  }

  if (palette_idx > 0 &&
      background_mask_->get(current_block_x, current_block_y) == palette_idx) {
    // Already known to be fully this palette color; skip redundant output.
    return;
  }

  uint8_t new_mask_value = 0;
  if (all_same && fully_covered) {
    // Fast path: full block of a uniform color. Emit one fillRect operation.
    // Flush buffered random-pixel writes first to preserve draw order.
    writer.flush();
    output_.fillRect(blending_mode_, block_x0, block_y0, block_x1, block_y1,
                     first_color);
    new_mask_value = palette_idx;
  } else {
    // Partial block or mixed colors: write covered pixels exactly.
    for_each_covered_pixel([&](int16_t x, int16_t y, uint32_t ord) {
      writer.writePixel(x, y, all_same ? first_color : color[ord - start_ord]);
      return true;
    });
  }

  background_mask_->set(current_block_x, current_block_y, new_mask_value);
}

void BackgroundFillOptimizer::writeRects(BlendingMode mode, Color* color,
                                         int16_t* x0, int16_t* y0, int16_t* x1,
                                         int16_t* y1, uint16_t count) {
  BufferedRectWriter writer(output_, mode);
  while (count-- > 0) {
    const Color c = *color++;
    const int16_t rx0 = *x0++;
    const int16_t ry0 = *y0++;
    const int16_t rx1 = *x1++;
    const int16_t ry1 = *y1++;

    uint8_t palette_idx = getIdxInPalette(c, palette_, *palette_size_);
    if (palette_idx == 0) {
      const int16_t rect_w = rx1 - rx0 + 1;
      const int16_t rect_h = ry1 - ry0 + 1;
      if (rect_w >= kBgFillOptimizerDynamicPaletteMinWidth &&
          rect_h >= kBgFillOptimizerDynamicPaletteMinHeight) {
        palette_idx = tryAddIdxInPalette(c, palette_, *palette_size_);
      }
    }

    if (palette_idx != 0) {
      RectFillWriter adapter{
          .color = c,
          .writer = writer,
      };
      fillRectBg(rx0, ry0, rx1, ry1, adapter, palette_idx);
    } else {
      // Not a background palette color -> clear the nibble subrectangle
      // corresponding to a region entirely enclosing the the drawn rectangle
      // before writing to the underlying device.
      background_mask_->fillRect(Box(rx0 / kBgFillOptimizerWindowSize,
                                     ry0 / kBgFillOptimizerWindowSize,
                                     rx1 / kBgFillOptimizerWindowSize,
                                     ry1 / kBgFillOptimizerWindowSize),
                                 0);
      writer.writeRect(rx0, ry0, rx1, ry1, c);
    }
  }
}

void BackgroundFillOptimizer::fillRects(BlendingMode mode, Color color,
                                        int16_t* x0, int16_t* y0, int16_t* x1,
                                        int16_t* y1, uint16_t count) {
  uint8_t palette_idx = getIdxInPalette(color, palette_, *palette_size_);
  BufferedRectFiller filler(output_, color, mode);
  BufferedRectWriter writer(output_, mode);

  for (int i = 0; i < count; ++i) {
    const int16_t rx0 = x0[i];
    const int16_t ry0 = y0[i];
    const int16_t rx1 = x1[i];
    const int16_t ry1 = y1[i];

    if (palette_idx == 0) {
      const int16_t rect_w = rx1 - rx0 + 1;
      const int16_t rect_h = ry1 - ry0 + 1;
      if (rect_w >= kBgFillOptimizerDynamicPaletteMinWidth &&
          rect_h >= kBgFillOptimizerDynamicPaletteMinHeight) {
        palette_idx = tryAddIdxInPalette(color, palette_, *palette_size_);
      }
    }

    if (palette_idx != 0) {
      fillRectBg(rx0, ry0, rx1, ry1, filler, palette_idx);
    } else {
      // Not a background color -> clear the nibble subrectangle
      // corresponding to a region entirely enclosing the drawn rectangle
      // before writing to the underlying device.
      background_mask_->fillRect(Box(rx0 / kBgFillOptimizerWindowSize,
                                     ry0 / kBgFillOptimizerWindowSize,
                                     rx1 / kBgFillOptimizerWindowSize,
                                     ry1 / kBgFillOptimizerWindowSize),
                                 0);
      writer.writeRect(rx0, ry0, rx1, ry1, color);
    }
  }
}

void BackgroundFillOptimizer::writePixels(BlendingMode mode, Color* color,
                                          int16_t* x, int16_t* y,
                                          uint16_t pixel_count) {
  int16_t* x_out = x;
  int16_t* y_out = y;
  Color* color_out = color;
  uint16_t new_pixel_count = 0;
  for (uint16_t i = 0; i < pixel_count; ++i) {
    uint8_t palette_idx = getIdxInPalette(color[i], palette_, *palette_size_);
    if (palette_idx != 0) {
      // Do not actually draw the background pixel if the corresponding
      // bit-mask rectangle is known to be all-background already.
      if (background_mask_->get(x[i] / kBgFillOptimizerWindowSize,
                                y[i] / kBgFillOptimizerWindowSize) ==
          palette_idx)
        continue;
    }
    // Mark the corresponding bit-mask rectangle as no longer
    // all-background.
    background_mask_->set(x[i] / kBgFillOptimizerWindowSize,
                          y[i] / kBgFillOptimizerWindowSize, 0);
    *x_out++ = x[i];
    *y_out++ = y[i];
    *color_out++ = color[i];
    new_pixel_count++;
  }
  if (new_pixel_count > 0) {
    output_.writePixels(mode, color, x, y, new_pixel_count);
  }
}

void BackgroundFillOptimizer::fillPixels(BlendingMode mode, Color color,
                                         int16_t* x, int16_t* y,
                                         uint16_t pixel_count) {
  uint8_t palette_idx = getIdxInPalette(color, palette_, *palette_size_);
  if (palette_idx != 0) {
    int16_t* x_out = x;
    int16_t* y_out = y;
    uint16_t new_pixel_count = 0;
    for (uint16_t i = 0; i < pixel_count; ++i) {
      // Do not actually draw the background pixel if the corresponding
      // bit-mask rectangle is known to be all-background already.
      if (background_mask_->get(x[i] / kBgFillOptimizerWindowSize,
                                y[i] / kBgFillOptimizerWindowSize) ==
          palette_idx) {
        continue;
      }
      background_mask_->set(x[i] / kBgFillOptimizerWindowSize,
                            y[i] / kBgFillOptimizerWindowSize, 0);
      *x_out++ = x[i];
      *y_out++ = y[i];
      new_pixel_count++;
    }
    if (new_pixel_count > 0) {
      output_.fillPixels(mode, color, x, y, new_pixel_count);
    }
  } else {
    // We need to draw all the pixels, but also mark the corresponding
    // bit-mask rectangles as not all-background.
    for (uint16_t i = 0; i < pixel_count; ++i) {
      background_mask_->set(x[i] / kBgFillOptimizerWindowSize,
                            y[i] / kBgFillOptimizerWindowSize, 0);
    }
    output_.fillPixels(mode, color, x, y, pixel_count);
  }
}

void BackgroundFillOptimizer::drawDirectRect(const roo::byte* data,
                                             size_t row_width_bytes,
                                             int16_t src_x0, int16_t src_y0,
                                             int16_t src_x1, int16_t src_y1,
                                             int16_t dst_x0, int16_t dst_y0) {
  if (src_x1 < src_x0 || src_y1 < src_y0) return;
  const int16_t dst_x1 = dst_x0 + (src_x1 - src_x0);
  const int16_t dst_y1 = dst_y0 + (src_y1 - src_y0);

  const int16_t bx_min = dst_x0 / kBgFillOptimizerWindowSize;
  const int16_t by_min = dst_y0 / kBgFillOptimizerWindowSize;
  const int16_t bx_max = dst_x1 / kBgFillOptimizerWindowSize;
  const int16_t by_max = dst_y1 / kBgFillOptimizerWindowSize;

  const ColorFormat& color_format = getColorFormat();

  for (int16_t by = by_min; by <= by_max; ++by) {
    const int16_t block_y0 = by * kBgFillOptimizerWindowSize;
    const int16_t block_y1 = block_y0 + kBgFillOptimizerWindowSize - 1;
    const int16_t draw_y0 = std::max<int16_t>(block_y0, dst_y0);
    const int16_t draw_y1 = std::min<int16_t>(block_y1, dst_y1);

    bool streak_active = false;
    int16_t streak_bx0 = 0;

    for (int16_t bx = bx_min; bx <= bx_max + 1; ++bx) {
      bool must_draw = false;

      if (bx <= bx_max) {
        const int16_t block_x0 = bx * kBgFillOptimizerWindowSize;
        const int16_t block_x1 = block_x0 + kBgFillOptimizerWindowSize - 1;
        const int16_t draw_x0 = std::max<int16_t>(block_x0, dst_x0);
        const int16_t draw_x1 = std::min<int16_t>(block_x1, dst_x1);

        DCHECK_LE(draw_x0, draw_x1);

        const int16_t src_block_x0 = src_x0 + (draw_x0 - dst_x0);
        const int16_t src_block_y0 = src_y0 + (draw_y0 - dst_y0);
        const int16_t src_block_x1 = src_x0 + (draw_x1 - dst_x0);
        const int16_t src_block_y1 = src_y0 + (draw_y1 - dst_y0);
        Color uniform_color;
        const bool all_same = color_format.decodeIfUniform(
            data, row_width_bytes, src_block_x0, src_block_y0, src_block_x1,
            src_block_y1, &uniform_color);

        const bool fully_covered =
            (draw_x0 == block_x0 && draw_x1 == block_x1 &&
             draw_y0 == block_y0 && draw_y1 == block_y1);

        uint8_t palette_idx =
            all_same ? getIdxInPalette(uniform_color, palette_, *palette_size_)
                     : 0;
        if (all_same && fully_covered) {
          if (palette_idx == 0) {
            palette_idx =
                tryAddIdxInPaletteOnSecondConsecutiveColor(uniform_color);
          } else {
            resetPendingDynamicPaletteColor();
          }
        } else {
          resetPendingDynamicPaletteColor();
        }
        const uint8_t current_mask_value = background_mask_->get(bx, by);

        // Redundant block update: all touched pixels are a palette color and
        // the block is already known to be entirely that color.
        must_draw = !(palette_idx > 0 && current_mask_value == palette_idx);

        uint8_t new_mask_value = current_mask_value;
        if (must_draw) {
          new_mask_value = (fully_covered && palette_idx > 0) ? palette_idx : 0;
        }

        if (new_mask_value != current_mask_value) {
          background_mask_->set(bx, by, new_mask_value);
        }
      }

      if (must_draw) {
        if (!streak_active) {
          streak_active = true;
          streak_bx0 = bx;
        }
      } else if (streak_active) {
        const int16_t streak_block_x0 = streak_bx0 * kBgFillOptimizerWindowSize;
        const int16_t streak_block_x1 = bx * kBgFillOptimizerWindowSize - 1;
        const int16_t draw_x0 = std::max<int16_t>(streak_block_x0, dst_x0);
        const int16_t draw_x1 = std::min<int16_t>(streak_block_x1, dst_x1);

        const int16_t src_streak_x0 = src_x0 + (draw_x0 - dst_x0);
        const int16_t src_streak_y0 = src_y0 + (draw_y0 - dst_y0);
        const int16_t src_streak_x1 = src_x0 + (draw_x1 - dst_x0);
        const int16_t src_streak_y1 = src_y0 + (draw_y1 - dst_y0);

        output_.drawDirectRect(data, row_width_bytes, src_streak_x0,
                               src_streak_y0, src_streak_x1, src_streak_y1,
                               draw_x0, draw_y0);
        streak_active = false;
      }
    }
  }
}

void BackgroundFillOptimizer::writePixel(int16_t x, int16_t y, Color c,
                                         BufferedPixelWriter* writer) {
  uint8_t palette_idx = getIdxInPalette(c, palette_, *palette_size_);
  if (palette_idx != 0) {
    // Skip writing if the containing bit-mask mapped rectangle
    // is known to be all-background already.
    if (background_mask_->get(x / kBgFillOptimizerWindowSize,
                              y / kBgFillOptimizerWindowSize) == palette_idx) {
      return;
    }
  }
  // Mark the corresponding bit-mask mapped rectangle as no longer
  // all-background.
  background_mask_->set(x / kBgFillOptimizerWindowSize,
                        y / kBgFillOptimizerWindowSize, 0);
  writer->writePixel(x, y, c);
}

template <typename Filler>
void BackgroundFillOptimizer::fillRectBg(int16_t x0, int16_t y0, int16_t x1,
                                         int16_t y1, Filler& filler,
                                         uint8_t palette_idx) {
  // Iterate over the bit-map rectangle that encloses the requested rectangle.
  // Skip writing sub-rectangles that are known to be already all-background.
  Box filter_box(
      x0 / kBgFillOptimizerWindowSize, y0 / kBgFillOptimizerWindowSize,
      x1 / kBgFillOptimizerWindowSize, y1 / kBgFillOptimizerWindowSize);
  internal::NibbleRectWindowIterator itr(background_mask_, filter_box.xMin(),
                                         filter_box.yMin(), filter_box.xMax(),
                                         filter_box.yMax());
  int16_t xendl = filter_box.xMax() + 1;
  for (int16_t y = filter_box.yMin(); y <= filter_box.yMax(); ++y) {
    int16_t xstart = -1;
    for (int16_t x = filter_box.xMin(); x <= xendl; ++x) {
      if (x == xendl || itr.next() == palette_idx) {
        // End of the streak. Let's draw the necessary rectangle.
        if (xstart >= 0) {
          Box box(
              xstart * kBgFillOptimizerWindowSize,
              y * kBgFillOptimizerWindowSize,
              x * kBgFillOptimizerWindowSize - 1,
              y * kBgFillOptimizerWindowSize + kBgFillOptimizerWindowSize - 1);
          if (box.clip(Box(x0, y0, x1, y1)) != Box::CLIP_RESULT_UNCHANGED) {
            if (y0 > y * kBgFillOptimizerWindowSize ||
                y1 < y * kBgFillOptimizerWindowSize +
                         kBgFillOptimizerWindowSize - 1) {
              // Need to invalidate the entire line.
              background_mask_->fillRect(Box(xstart, y, x - 1, y), 0);
            } else {
              if (x0 > xstart * kBgFillOptimizerWindowSize) {
                background_mask_->set(xstart, y, 0);
              }
              if (x1 < x * kBgFillOptimizerWindowSize - 1) {
                background_mask_->set(x - 1, y, 0);
              }
            }
          }
          filler.fillRect(box.xMin(), box.yMin(), box.xMax(), box.yMax());
        }
        xstart = -1;
      } else {
        // Continuation, or a new streak.
        if (xstart < 0) {
          // New streak.
          xstart = x;
        }
      }
    }
  }
  // Identify the bit-mask rectangle that is entirely covered by the
  // requested rect. If non-empty, set all nibbles corresponding to it in
  // the mask, indicating that the area is all-bg.
  Box inner_filter_box(
      (x0 + kBgFillOptimizerWindowSize - 1) / kBgFillOptimizerWindowSize,
      (y0 + kBgFillOptimizerWindowSize - 1) / kBgFillOptimizerWindowSize,
      (x1 + 1) / kBgFillOptimizerWindowSize - 1,
      (y1 + 1) / kBgFillOptimizerWindowSize - 1);
  if (!inner_filter_box.empty()) {
    background_mask_->fillRect(inner_filter_box, palette_idx);
  }
}

BackgroundFillOptimizerDevice::BackgroundFillOptimizerDevice(
    DisplayDevice& device)
    : DisplayDevice(device.raw_width(), device.raw_height()),
      device_(device),
      buffer_(device_.raw_width(), device_.raw_height()),
      optimizer_(device_, buffer_) {
  setOrientation(device.orientation());
}

void BackgroundFillOptimizerDevice::setPalette(const Color* palette,
                                               uint8_t palette_size,
                                               Color prefilled) {
  buffer_.setPalette(palette, palette_size);
  optimizer_.updatePalette(buffer_.palette_, &buffer_.palette_size_);
  buffer_.setPrefilled(prefilled);
}

void BackgroundFillOptimizerDevice::setPalette(
    std::initializer_list<Color> palette, Color prefilled) {
  uint8_t size = palette.size();
  const Color* p = size == 0 ? nullptr : &*palette.begin();
  setPalette(p, size, prefilled);
}

}  // namespace roo_display
