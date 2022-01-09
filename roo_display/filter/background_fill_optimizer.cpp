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

}  // namespace

BackgroundFillOptimizer::FrameBuffer::FrameBuffer(int16_t width, int16_t height)
    : FrameBuffer(width, height,
                  new uint8_t[FrameBuffer::SizeForDimensions(width, height)],
                  true) {
  invalidate();
}

BackgroundFillOptimizer::FrameBuffer::FrameBuffer(int16_t width, int16_t height,
                                                  uint8_t* buffer)
    : FrameBuffer(width, height, buffer, false) {}

BackgroundFillOptimizer::FrameBuffer::FrameBuffer(int16_t width, int16_t height,
                                                  uint8_t* buffer,
                                                  bool owns_buffer)
    : background_mask_(
          buffer, ((((width - 1) / kBgFillOptimizerWindowSize + 1) + 1) / 2),
          ((((height - 1) / kBgFillOptimizerWindowSize + 1) + 1) / 2) * 2),
      palette_size_(0),
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

void BackgroundFillOptimizer::FrameBuffer::invalidate() {
  background_mask_.fillRect(
      Box(0, 0, background_mask_.width() - 1, background_mask_.height() - 1),
      0);
}

void BackgroundFillOptimizer::FrameBuffer::invalidateRect(const Box& rect) {
  background_mask_.fillRect(
      Box(rect.xMin() / roo_display::kBgFillOptimizerWindowSize,
          rect.yMin() / roo_display::kBgFillOptimizerWindowSize,
          rect.xMax() / roo_display::kBgFillOptimizerWindowSize,
          rect.yMax() / roo_display::kBgFillOptimizerWindowSize),
      0);
}

BackgroundFillOptimizer::BackgroundFillOptimizer(DisplayOutput& output,
                                                 FrameBuffer& buffer)
    : output_(output),
      background_mask_(&buffer.background_mask_),
      palette_(buffer.palette_),
      palette_size_(buffer.palette_size_),
      address_window_(0, 0, 0, 0),
      cursor_x_(0),
      cursor_y_(0) {}

void BackgroundFillOptimizer::setAddress(uint16_t x0, uint16_t y0, uint16_t x1,
                                         uint16_t y1, PaintMode mode) {
  address_window_ = Box(x0, y0, x1, y1);
  paint_mode_ = mode;
  cursor_x_ = x0;
  cursor_y_ = y0;
}

void BackgroundFillOptimizer::write(Color* color, uint32_t pixel_count) {
  // Naive implementation, for now.
  BufferedPixelWriter writer(output_, paint_mode_);
  while (pixel_count-- > 0) {
    writePixel(cursor_x_, cursor_y_, *color++, &writer);
    if (++cursor_x_ > address_window_.xMax()) {
      ++cursor_y_;
      cursor_x_ = address_window_.xMin();
    }
  }
}

void BackgroundFillOptimizer::writeRects(PaintMode mode, Color* color,
                                         int16_t* x0, int16_t* y0, int16_t* x1,
                                         int16_t* y1, uint16_t count) {
  BufferedRectWriter writer(output_, mode);
  while (count-- > 0) {
    Color c = *color++;
    uint8_t palette_idx = getIdxInPalette(c);
    if (palette_idx != 0) {
      RectFillWriter adapter{
          .color = c,
          .writer = writer,
      };
      fillRectBg(*x0++, *y0++, *x1++, *y1++, &adapter, palette_idx);
    } else {
      // Not a background palette color -> clear the nibble subrectangle
      // corresponding to a region entirely enclosing the the drawn rectangle
      // before writing to the underlying device.
      background_mask_->fillRect(Box(*x0 / kBgFillOptimizerWindowSize,
                                     *y0 / kBgFillOptimizerWindowSize,
                                     *x1 / kBgFillOptimizerWindowSize,
                                     *y1 / kBgFillOptimizerWindowSize),
                                 0);
      writer.writeRect(*x0++, *y0++, *x1++, *y1++, c);
    }
  }
}

void BackgroundFillOptimizer::fillRects(PaintMode mode, Color color,
                                        int16_t* x0, int16_t* y0, int16_t* x1,
                                        int16_t* y1, uint16_t count) {
  uint8_t palette_idx = getIdxInPalette(color);
  if (palette_idx != 0) {
    BufferedRectFiller filler(output_, color, mode);
    while (count-- > 0) {
      fillRectBg(*x0++, *y0++, *x1++, *y1++, &filler, palette_idx);
    }
  } else {
    for (int i = 0; i < count; ++i) {
      // Not a background color -> clear the nibble subrectangle
      // corresponding to a region entirely enclosing the drawn rectangle
      // before writing to the underlying device.
      background_mask_->fillRect(Box(x0[i] / kBgFillOptimizerWindowSize,
                                     y0[i] / kBgFillOptimizerWindowSize,
                                     x1[i] / kBgFillOptimizerWindowSize,
                                     y1[i] / kBgFillOptimizerWindowSize),
                                 0);
    }
    output_.fillRects(mode, color, x0, y0, x1, y1, count);
  }
}

void BackgroundFillOptimizer::writePixels(PaintMode mode, Color* color,
                                          int16_t* x, int16_t* y,
                                          uint16_t pixel_count) {
  int16_t* x_out = x;
  int16_t* y_out = y;
  Color* color_out = color;
  uint16_t new_pixel_count = 0;
  for (uint16_t i = 0; i < pixel_count; ++i) {
    uint8_t palette_idx = getIdxInPalette(color[i]);
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

void BackgroundFillOptimizer::fillPixels(PaintMode mode, Color color,
                                         int16_t* x, int16_t* y,
                                         uint16_t pixel_count) {
  uint8_t palette_idx = getIdxInPalette(color);
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

inline uint8_t BackgroundFillOptimizer::getIdxInPalette(Color color) {
  for (int i = 0; i < palette_size_; ++i) {
    if (color == palette_[i]) return i + 1;
  }
  return 0;
}

void BackgroundFillOptimizer::writePixel(int16_t x, int16_t y, Color c,
                                         BufferedPixelWriter* writer) {
  uint8_t palette_idx = getIdxInPalette(c);
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
                                         int16_t y1, Filler* filler,
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
          filler->fillRect(box.xMin(), box.yMin(), box.xMax(), box.yMax());
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

}  // namespace roo_display
