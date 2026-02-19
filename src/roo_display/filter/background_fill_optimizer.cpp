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
      cursor_x_(0),
      cursor_y_(0),
      cursor_ord_(0),
      pinned_palette_size_(buffer.palette_size_),
      palette_full_hint_(false),
      has_pending_dynamic_palette_color_(false),
      pending_dynamic_palette_color_(color::Transparent),
      write_scan_state_(WriteScanState::kScan),
      passthrough_address_set_(false),
      scan_uniform_active_(false),
      scan_uniform_color_(color::Transparent),
      scan_uniform_start_ord_(0),
      scan_uniform_count_(0) {
  // The backing mask may already contain data (e.g. caller-provided external
  // storage), so initialize usage counters from actual mask contents.
  recountMaskUsage();
}

void BackgroundFillOptimizer::updatePalette(Color* palette,
                                            uint8_t* palette_size,
                                            uint8_t pinned_palette_size) {
  palette_ = palette;
  palette_size_ = palette_size;
  pinned_palette_size_ = std::min<uint8_t>(pinned_palette_size, *palette_size_);
  resetPendingDynamicPaletteColor();
  resetMaskAndUsage(0);
}

void BackgroundFillOptimizer::setPrefilled(Color color) {
  resetMaskAndUsage(getIdxInPalette(color, palette_, *palette_size_));
}

bool BackgroundFillOptimizer::isReclaimablePaletteIdx(uint8_t idx) const {
  return idx > pinned_palette_size_ && idx <= *palette_size_;
}

void BackgroundFillOptimizer::recountMaskUsage() {
  for (int i = 0; i < 16; ++i) palette_usage_count_[i] = 0;
  const int16_t w = background_mask_->width();
  const int16_t h = background_mask_->height();
  for (int16_t y = 0; y < h; ++y) {
    for (int16_t x = 0; x < w; ++x) {
      ++palette_usage_count_[background_mask_->get(x, y)];
    }
  }

  if (*palette_size_ < 15) {
    palette_full_hint_ = false;
    return;
  }

  palette_full_hint_ = true;
  for (uint8_t idx = pinned_palette_size_ + 1; idx <= *palette_size_; ++idx) {
    if (palette_usage_count_[idx] == 0) {
      palette_full_hint_ = false;
      return;
    }
  }
}

void BackgroundFillOptimizer::decrementPaletteUsage(uint8_t old_value) {
  DCHECK_GT(palette_usage_count_[old_value], 0);
  --palette_usage_count_[old_value];
  if (palette_usage_count_[old_value] == 0 &&
      isReclaimablePaletteIdx(old_value)) {
    // LOG(INFO) << "Palette idx " << static_cast<int>(old_value - 1)
    //           << " is now reclaimable";
    palette_full_hint_ = false;
  }
}

void BackgroundFillOptimizer::incrementPaletteUsage(uint8_t new_value,
                                                    uint16_t count) {
  palette_usage_count_[new_value] += count;
}

void BackgroundFillOptimizer::updateMaskValue(int16_t x, int16_t y,
                                              uint8_t old_value,
                                              uint8_t new_value) {
  DCHECK_NE(old_value, new_value);
  decrementPaletteUsage(old_value);
  incrementPaletteUsage(new_value);
  background_mask_->set(x, y, new_value);
}

void BackgroundFillOptimizer::fillMaskRect(const Box& box, uint8_t new_value) {
  if (box.empty()) return;
  const int16_t x0 = std::max<int16_t>(0, box.xMin());
  const int16_t y0 = std::max<int16_t>(0, box.yMin());
  const int16_t x1 =
      std::min<int16_t>(background_mask_->width() - 1, box.xMax());
  const int16_t y1 =
      std::min<int16_t>(background_mask_->height() - 1, box.yMax());
  if (x1 < x0 || y1 < y0) return;

  for (int16_t y = y0; y <= y1; ++y) {
    for (int16_t x = x0; x <= x1; ++x) {
      decrementPaletteUsage(background_mask_->get(x, y));
    }
  }

  background_mask_->fillRect(Box(x0, y0, x1, y1), new_value);
  incrementPaletteUsage(new_value,
                        static_cast<uint16_t>((x1 - x0 + 1) * (y1 - y0 + 1)));
}

void BackgroundFillOptimizer::resetMaskAndUsage(uint8_t mask_value) {
  background_mask_->fillRect(
      Box(0, 0, background_mask_->width() - 1, background_mask_->height() - 1),
      mask_value);
  for (int i = 0; i < 16; ++i) palette_usage_count_[i] = 0;
  const uint16_t cell_count = static_cast<uint16_t>(background_mask_->width() *
                                                    background_mask_->height());
  palette_usage_count_[mask_value] = cell_count;
  palette_full_hint_ = false;
}

uint8_t BackgroundFillOptimizer::tryAddIdxInPaletteDynamic(Color color) {
  if (*palette_size_ < 15) {
    uint8_t reclaim_idx = *palette_size_;
    while (reclaim_idx > pinned_palette_size_ &&
           palette_usage_count_[reclaim_idx] == 0) {
      --reclaim_idx;
    }
    const bool append = (reclaim_idx == *palette_size_);
    if (append) {
      ++(*palette_size_);
      // LOG(INFO) << "Added color " << roo_logging::hex << color.asArgb()
      //           << " to palette idx " << static_cast<int>(*palette_size_);
    } else {
      // LOG(INFO) << "Reclaimed palette idx " << static_cast<int>(reclaim_idx)
      //           << " for color " << roo_logging::hex << color.asArgb();
    }
    palette_[reclaim_idx] = color;
    palette_full_hint_ = false;
    return reclaim_idx + 1;
  }

  if (palette_full_hint_) return 0;

  for (uint8_t idx = pinned_palette_size_ + 1; idx <= *palette_size_; ++idx) {
    if (palette_usage_count_[idx] == 0) {
      palette_[idx - 1] = color;
      return idx;
    }
  }

  palette_full_hint_ = true;
  return 0;
}

uint8_t BackgroundFillOptimizer::tryAddIdxInPaletteOnSecondConsecutiveColor(
    Color color) {
  if (has_pending_dynamic_palette_color_ &&
      pending_dynamic_palette_color_ == color) {
    has_pending_dynamic_palette_color_ = false;
    return tryAddIdxInPaletteDynamic(color);
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
  flushDeferredUniformRun();
  address_window_ = Box(x0, y0, x1, y1);
  blending_mode_ = mode;
  cursor_x_ = x0;
  cursor_y_ = y0;
  cursor_ord_ = 0;
  write_scan_state_ = WriteScanState::kScan;
  passthrough_address_set_ = false;
  scan_uniform_active_ = false;
  scan_uniform_count_ = 0;
  resetPendingDynamicPaletteColor();
}

uint32_t BackgroundFillOptimizer::addressWindowPixelCount() const {
  return static_cast<uint32_t>(address_window_.width()) *
         static_cast<uint32_t>(address_window_.height());
}

bool BackgroundFillOptimizer::writeCursorAtOrPastWindowEnd() const {
  return cursor_ord_ >= addressWindowPixelCount();
}

void BackgroundFillOptimizer::setWriteCursorOrd(uint32_t ord) {
  cursor_ord_ = ord;
  const int16_t aw_width = address_window_.width();
  const uint32_t row = cursor_ord_ / static_cast<uint32_t>(aw_width);
  const uint32_t col = cursor_ord_ % static_cast<uint32_t>(aw_width);
  cursor_x_ = address_window_.xMin() + static_cast<int16_t>(col);
  cursor_y_ = address_window_.yMin() + static_cast<int16_t>(row);
}

void BackgroundFillOptimizer::advanceWriteCursor(uint32_t pixel_count) {
  setWriteCursorOrd(cursor_ord_ + pixel_count);
}

uint32_t BackgroundFillOptimizer::pixelsUntilStripeEnd() const {
  if (writeCursorAtOrPastWindowEnd()) return 0;
  const int16_t aw_width = address_window_.width();
  const int16_t aw_y0 = address_window_.yMin();
  const int16_t aw_y1 = address_window_.yMax();
  const int16_t aw_x1 = address_window_.xMax();

  const int16_t y = cursor_y_;
  const int16_t mod =
      static_cast<int16_t>((y + 1) % kBgFillOptimizerWindowSize);
  const int16_t rows_to_aligned_end =
      (mod == 0) ? 0 : (kBgFillOptimizerWindowSize - mod);
  const int16_t stripe_end_y =
      std::min<int16_t>(aw_y1, y + rows_to_aligned_end);

  const uint32_t stripe_end_ord =
      static_cast<uint32_t>(stripe_end_y - aw_y0) *
          static_cast<uint32_t>(aw_width) +
      static_cast<uint32_t>(aw_x1 - address_window_.xMin());
  DCHECK_GE(stripe_end_ord, cursor_ord_);
  return stripe_end_ord - cursor_ord_ + 1;
}

void BackgroundFillOptimizer::beginPassthroughIfNeeded() {
  if (passthrough_address_set_) return;
  output_.setAddress(cursor_x_, cursor_y_, address_window_.xMax(),
                     address_window_.yMax(), blending_mode_);
  passthrough_address_set_ = true;
}

void BackgroundFillOptimizer::clearMaskForOrdRange(uint32_t start_ord,
                                                   uint32_t pixel_count) {
  if (pixel_count == 0) return;
  static constexpr int16_t kBlock = kBgFillOptimizerWindowSize;
  const int16_t aw_width = address_window_.width();
  const int16_t aw_x0 = address_window_.xMin();
  const int16_t aw_y0 = address_window_.yMin();
  const int16_t aw_x1 = address_window_.xMax();
  const uint32_t end_ord = start_ord + pixel_count - 1;

  const int16_t start_y = aw_y0 + static_cast<int16_t>(start_ord / aw_width);
  const int16_t end_y = aw_y0 + static_cast<int16_t>(end_ord / aw_width);
  const int16_t start_x = aw_x0 + static_cast<int16_t>(start_ord % aw_width);
  const int16_t end_x = aw_x0 + static_cast<int16_t>(end_ord % aw_width);

  const int16_t bx_min = aw_x0 / kBlock;
  const int16_t bx_max = aw_x1 / kBlock;
  const int16_t start_by = start_y / kBlock;
  const int16_t end_by = end_y / kBlock;

  // The stream range [start_ord, end_ord] maps to contiguous rows. Mask
  // clearing can always be decomposed into up to 3 rectangle clears:
  //   1) top boundary block-row (partial-X),
  //   2) middle block-rows (full-X),
  //   3) bottom boundary block-row (partial-X).
  //
  //     ..****
  //     ******
  //     ******
  //     ****..
  //
  // Special case is when the range covers only a single row, then it's just one
  // partial-X clear.:

  if (start_y == end_y) {
    fillMaskRect(Box(start_x / kBlock, start_by, end_x / kBlock, start_by), 0);
    return;
  }

  int16_t full_by0 = start_by;
  int16_t full_by1 = end_by;

  // If the first touched row is the last row of its 4px stripe, only that row
  // contributes in start_by and needs partial-x clearing.
  if (((start_y + 1) % kBlock) == 0) {
    fillMaskRect(Box(start_x / kBlock, start_by, bx_max, start_by), 0);
    ++full_by0;
  }

  // If the last touched row is the first row of its 4px stripe, only that row
  // contributes in end_by and needs partial-x clearing.
  if ((end_y % kBlock) == 0) {
    fillMaskRect(Box(bx_min, end_by, end_x / kBlock, end_by), 0);
    --full_by1;
  }

  if (full_by0 <= full_by1) {
    fillMaskRect(Box(bx_min, full_by0, bx_max, full_by1), 0);
  }
}

void BackgroundFillOptimizer::passthroughWrite(Color* color,
                                               uint32_t pixel_count) {
  if (pixel_count == 0) return;
  beginPassthroughIfNeeded();
  clearMaskForOrdRange(cursor_ord_, pixel_count);
  output_.write(color, pixel_count);
  advanceWriteCursor(pixel_count);
}

void BackgroundFillOptimizer::passthroughFill(Color color,
                                              uint32_t pixel_count) {
  if (pixel_count == 0) return;
  beginPassthroughIfNeeded();
  clearMaskForOrdRange(cursor_ord_, pixel_count);
  output_.fill(color, pixel_count);
  advanceWriteCursor(pixel_count);
}

void BackgroundFillOptimizer::flushDeferredUniformRun() {
  if (!scan_uniform_active_ || scan_uniform_count_ == 0) return;
  setWriteCursorOrd(scan_uniform_start_ord_);
  write_scan_state_ = WriteScanState::kPassthrough;
  passthrough_address_set_ = false;
  passthroughFill(scan_uniform_color_, scan_uniform_count_);
  scan_uniform_active_ = false;
  scan_uniform_count_ = 0;
}

void BackgroundFillOptimizer::processAlignedFullStripe(Color* color) {
  const int16_t aw_x0 = address_window_.xMin();
  const int16_t aw_width = address_window_.width();
  const int16_t stripe_y0 = cursor_y_;
  const int16_t stripe_y1 = stripe_y0 + kBgFillOptimizerWindowSize - 1;
  const int16_t block_count_x = aw_width / kBgFillOptimizerWindowSize;
  BufferedPixelWriter writer(output_, blending_mode_);

  for (int16_t block = 0; block < block_count_x; ++block) {
    const int16_t block_x0 = aw_x0 + block * kBgFillOptimizerWindowSize;
    const int16_t block_x1 = block_x0 + kBgFillOptimizerWindowSize - 1;
    const int16_t bx = block_x0 / kBgFillOptimizerWindowSize;
    const int16_t by = stripe_y0 / kBgFillOptimizerWindowSize;

    bool all_same = true;
    Color first_color = color[block * kBgFillOptimizerWindowSize];
    for (int16_t dy = 0; dy < kBgFillOptimizerWindowSize && all_same; ++dy) {
      const int16_t row_base =
          dy * aw_width + block * kBgFillOptimizerWindowSize;
      for (int16_t dx = 0; dx < kBgFillOptimizerWindowSize; ++dx) {
        if (color[row_base + dx] != first_color) {
          all_same = false;
          break;
        }
      }
    }

    const uint8_t current_mask_value = background_mask_->get(bx, by);
    if (!all_same) {
      resetPendingDynamicPaletteColor();
      if (current_mask_value != 0) {
        updateMaskValue(bx, by, current_mask_value, 0);
      }
      for (int16_t dy = 0; dy < kBgFillOptimizerWindowSize; ++dy) {
        const int16_t row_base =
            dy * aw_width + block * kBgFillOptimizerWindowSize;
        const int16_t y = stripe_y0 + dy;
        for (int16_t dx = 0; dx < kBgFillOptimizerWindowSize; ++dx) {
          writer.writePixel(block_x0 + dx, y, color[row_base + dx]);
        }
      }
      continue;
    }

    uint8_t palette_idx =
        getIdxInPalette(first_color, palette_, *palette_size_);
    if (palette_idx == 0) {
      palette_idx = tryAddIdxInPaletteOnSecondConsecutiveColor(first_color);
    } else {
      resetPendingDynamicPaletteColor();
    }

    if (palette_idx > 0 && current_mask_value == palette_idx) {
      continue;
    }

    writer.flush();
    output_.fillRect(blending_mode_, block_x0, stripe_y0, block_x1, stripe_y1,
                     first_color);
    const uint8_t new_mask_value = palette_idx;
    if (new_mask_value != current_mask_value) {
      updateMaskValue(bx, by, current_mask_value, new_mask_value);
    }
  }
  writer.flush();
}

bool BackgroundFillOptimizer::tryProcessGridAlignedBlockStripes(
    Color*& color, uint32_t& pixel_count) {
  if (pixel_count == 0 || writeCursorAtOrPastWindowEnd()) return false;
  if (cursor_x_ != address_window_.xMin()) return false;
  if ((address_window_.xMin() % kBgFillOptimizerWindowSize) != 0) return false;
  if ((cursor_y_ % kBgFillOptimizerWindowSize) != 0) return false;
  const int16_t aw_width = address_window_.width();
  if ((aw_width % kBgFillOptimizerWindowSize) != 0) return false;

  const uint32_t stripe_pixels =
      static_cast<uint32_t>(aw_width) * kBgFillOptimizerWindowSize;
  bool consumed_any = false;
  while (pixel_count >= stripe_pixels &&
         cursor_y_ + kBgFillOptimizerWindowSize - 1 <= address_window_.yMax()) {
    processAlignedFullStripe(color);
    color += stripe_pixels;
    pixel_count -= stripe_pixels;
    advanceWriteCursor(stripe_pixels);
    consumed_any = true;
    scan_uniform_active_ = false;
    scan_uniform_count_ = 0;
    if (writeCursorAtOrPastWindowEnd()) break;
    DCHECK_EQ(cursor_y_ % kBgFillOptimizerWindowSize, 0);
  }
  return consumed_any;
}

void BackgroundFillOptimizer::emitUniformScanRun(Color color,
                                                 uint32_t start_ord,
                                                 uint32_t count) {
  if (count == 0) return;
  const int16_t aw_width = address_window_.width();
  const int16_t aw_x0 = address_window_.xMin();
  const int16_t aw_y0 = address_window_.yMin();

  const uint32_t end_ord = start_ord + count - 1;
  const int16_t x0 = aw_x0 + static_cast<int16_t>(start_ord % aw_width);
  const int16_t y0 = aw_y0 + static_cast<int16_t>(start_ord / aw_width);
  const int16_t x1 = aw_x0 + static_cast<int16_t>(end_ord % aw_width);
  const int16_t y1 = aw_y0 + static_cast<int16_t>(end_ord / aw_width);

  uint8_t palette_idx = getIdxInPalette(color, palette_, *palette_size_);
  const int16_t rect_w = x1 - x0 + 1;
  const int16_t rect_h = y1 - y0 + 1;
  if (palette_idx == 0 && rect_w >= kBgFillOptimizerDynamicPaletteMinWidth &&
      rect_h >= kBgFillOptimizerDynamicPaletteMinHeight) {
    palette_idx = tryAddIdxInPaletteDynamic(color);
  }

  BufferedRectWriter writer(output_, blending_mode_);
  if (palette_idx != 0) {
    RectFillWriter adapter{.color = color, .writer = writer};
    fillRectBg(x0, y0, x1, y1, adapter, palette_idx);
  } else {
    fillMaskRect(
        Box(x0 / kBgFillOptimizerWindowSize, y0 / kBgFillOptimizerWindowSize,
            x1 / kBgFillOptimizerWindowSize, y1 / kBgFillOptimizerWindowSize),
        0);
    writer.writeRect(x0, y0, x1, y1, color);
  }
}

void BackgroundFillOptimizer::write(Color* color, uint32_t pixel_count) {
  if (pixel_count == 0 || writeCursorAtOrPastWindowEnd()) return;

  while (pixel_count > 0 && !writeCursorAtOrPastWindowEnd()) {
    if (write_scan_state_ == WriteScanState::kScan) {
      // Strictly grid-aligned block fast path.
      if (tryProcessGridAlignedBlockStripes(color, pixel_count)) {
        continue;
      }

      // Uniform scanner path: works for aligned and unaligned windows.
      const uint32_t to_stripe_end = pixelsUntilStripeEnd();
      if (to_stripe_end == 0) break;
      const uint32_t inspect_count =
          std::min<uint32_t>(pixel_count, to_stripe_end);

      if (!scan_uniform_active_) {
        scan_uniform_active_ = true;
        scan_uniform_color_ = color[0];
        scan_uniform_start_ord_ = cursor_ord_;
        scan_uniform_count_ = 0;
      }

      uint32_t uniform_prefix = 0;
      while (uniform_prefix < inspect_count &&
             color[uniform_prefix] == scan_uniform_color_) {
        ++uniform_prefix;
      }

      if (uniform_prefix < inspect_count) {
        flushDeferredUniformRun();
        scan_uniform_active_ = false;
        scan_uniform_count_ = 0;
        write_scan_state_ = WriteScanState::kPassthrough;
        continue;
      }

      scan_uniform_count_ += inspect_count;
      advanceWriteCursor(inspect_count);
      color += inspect_count;
      pixel_count -= inspect_count;

      if (inspect_count == to_stripe_end) {
        emitUniformScanRun(scan_uniform_color_, scan_uniform_start_ord_,
                           scan_uniform_count_);
        scan_uniform_active_ = false;
        scan_uniform_count_ = 0;
      }
      continue;
    }

    const uint32_t to_stripe_end = pixelsUntilStripeEnd();
    if (to_stripe_end == 0) break;
    const uint32_t passthrough_count =
        std::min<uint32_t>(pixel_count, to_stripe_end);
    passthroughWrite(color, passthrough_count);
    color += passthrough_count;
    pixel_count -= passthrough_count;

    if (passthrough_count == to_stripe_end) {
      write_scan_state_ = WriteScanState::kScan;
      passthrough_address_set_ = false;
      scan_uniform_active_ = false;
      scan_uniform_count_ = 0;
    }
  }
}

void BackgroundFillOptimizer::fill(Color color, uint32_t pixel_count) {
  if (pixel_count == 0 || writeCursorAtOrPastWindowEnd()) return;

  while (pixel_count > 0 && !writeCursorAtOrPastWindowEnd()) {
    if (write_scan_state_ == WriteScanState::kScan) {
      if (scan_uniform_active_ && scan_uniform_count_ > 0 &&
          scan_uniform_color_ != color) {
        emitUniformScanRun(scan_uniform_color_, scan_uniform_start_ord_,
                           scan_uniform_count_);
        scan_uniform_active_ = false;
        scan_uniform_count_ = 0;
      }

      const uint32_t to_stripe_end = pixelsUntilStripeEnd();
      if (to_stripe_end == 0) break;
      const uint32_t inspect_count = std::min<uint32_t>(pixel_count, to_stripe_end);

      if (!scan_uniform_active_) {
        scan_uniform_active_ = true;
        scan_uniform_color_ = color;
        scan_uniform_start_ord_ = cursor_ord_;
        scan_uniform_count_ = 0;
      }

      scan_uniform_count_ += inspect_count;
      advanceWriteCursor(inspect_count);
      pixel_count -= inspect_count;

      if (inspect_count == to_stripe_end) {
        emitUniformScanRun(scan_uniform_color_, scan_uniform_start_ord_,
                           scan_uniform_count_);
        scan_uniform_active_ = false;
        scan_uniform_count_ = 0;
      }
      continue;
    }

    const uint32_t to_stripe_end = pixelsUntilStripeEnd();
    if (to_stripe_end == 0) break;
    const uint32_t passthrough_count =
        std::min<uint32_t>(pixel_count, to_stripe_end);
    passthroughFill(color, passthrough_count);
    pixel_count -= passthrough_count;

    if (passthrough_count == to_stripe_end) {
      write_scan_state_ = WriteScanState::kScan;
      passthrough_address_set_ = false;
      scan_uniform_active_ = false;
      scan_uniform_count_ = 0;
    }
  }
}

void BackgroundFillOptimizer::writeRects(BlendingMode mode, Color* color,
                                         int16_t* x0, int16_t* y0, int16_t* x1,
                                         int16_t* y1, uint16_t count) {
  flushDeferredUniformRun();
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
        palette_idx = tryAddIdxInPaletteDynamic(c);
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
      fillMaskRect(Box(rx0 / kBgFillOptimizerWindowSize,
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
  flushDeferredUniformRun();
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
        palette_idx = tryAddIdxInPaletteDynamic(color);
      }
    }

    if (palette_idx != 0) {
      fillRectBg(rx0, ry0, rx1, ry1, filler, palette_idx);
    } else {
      // Not a background color -> clear the nibble subrectangle
      // corresponding to a region entirely enclosing the drawn rectangle
      // before writing to the underlying device.
      fillMaskRect(Box(rx0 / kBgFillOptimizerWindowSize,
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
  flushDeferredUniformRun();
  int16_t* x_out = x;
  int16_t* y_out = y;
  Color* color_out = color;
  uint16_t new_pixel_count = 0;
  for (uint16_t i = 0; i < pixel_count; ++i) {
    const int16_t bx = x[i] / kBgFillOptimizerWindowSize;
    const int16_t by = y[i] / kBgFillOptimizerWindowSize;
    const uint8_t old_value = background_mask_->get(bx, by);

    uint8_t palette_idx = getIdxInPalette(color[i], palette_, *palette_size_);
    if (palette_idx != 0 && old_value == palette_idx) {
      // Do not actually draw the background pixel if the corresponding
      // bit-mask rectangle is known to be all-background already.
      continue;
    }
    // Mark the corresponding bit-mask rectangle as no longer
    // all-background.
    if (old_value != 0) {
      updateMaskValue(bx, by, old_value, 0);
    }
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
  flushDeferredUniformRun();
  uint8_t palette_idx = getIdxInPalette(color, palette_, *palette_size_);
  if (palette_idx != 0) {
    int16_t* x_out = x;
    int16_t* y_out = y;
    uint16_t new_pixel_count = 0;
    for (uint16_t i = 0; i < pixel_count; ++i) {
      const int16_t bx = x[i] / kBgFillOptimizerWindowSize;
      const int16_t by = y[i] / kBgFillOptimizerWindowSize;
      const uint8_t old_value = background_mask_->get(bx, by);

      // Do not actually draw the background pixel if the corresponding
      // bit-mask rectangle is known to be all-background already.
      if (old_value == palette_idx) {
        continue;
      }
      if (old_value != 0) {
        updateMaskValue(bx, by, old_value, 0);
      }
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
      const int16_t bx = x[i] / kBgFillOptimizerWindowSize;
      const int16_t by = y[i] / kBgFillOptimizerWindowSize;
      const uint8_t old_value = background_mask_->get(bx, by);
      if (old_value != 0) {
        updateMaskValue(bx, by, old_value, 0);
      }
    }
    output_.fillPixels(mode, color, x, y, pixel_count);
  }
}

void BackgroundFillOptimizer::drawDirectRect(const roo::byte* data,
                                             size_t row_width_bytes,
                                             int16_t src_x0, int16_t src_y0,
                                             int16_t src_x1, int16_t src_y1,
                                             int16_t dst_x0, int16_t dst_y0) {
  flushDeferredUniformRun();
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
          updateMaskValue(bx, by, current_mask_value, new_mask_value);
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
              fillMaskRect(Box(xstart, y, x - 1, y), 0);
            } else {
              if (x0 > xstart * kBgFillOptimizerWindowSize) {
                const uint8_t old_value = background_mask_->get(xstart, y);
                if (old_value != 0) {
                  updateMaskValue(xstart, y, old_value, 0);
                }
              }
              if (x1 < x * kBgFillOptimizerWindowSize - 1) {
                const uint8_t old_value = background_mask_->get(x - 1, y);
                if (old_value != 0) {
                  updateMaskValue(x - 1, y, old_value, 0);
                }
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
    fillMaskRect(inner_filter_box, palette_idx);
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
  optimizer_.updatePalette(buffer_.palette_, &buffer_.palette_size_,
                           palette_size);
  optimizer_.setPrefilled(prefilled);
}

void BackgroundFillOptimizerDevice::setPalette(
    std::initializer_list<Color> palette, Color prefilled) {
  uint8_t size = palette.size();
  const Color* p = size == 0 ? nullptr : &*palette.begin();
  setPalette(p, size, prefilled);
}

}  // namespace roo_display
