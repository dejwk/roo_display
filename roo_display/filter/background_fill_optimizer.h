#pragma once

#include "roo_display/core/device.h"

namespace roo_display {

static const int kBgFillOptimizerWindowSize = 4;

// // Sets the nth bit (as specified by offset) in the specified buffer,
// // to the specified value.
// inline void fillBit(uint8_t* buf, uint32_t offset, bool value) {
//   buf += (offset / 8);
//   offset %= 8;
//   if (value) {
//     *buf |= (1 << offset);
//   } else {
//     *buf &= ~(1 << offset);
//   }
// }

inline void fillBits(uint8_t* buf, uint32_t offset, int16_t count, bool value) {
  buf += (offset / 8);
  offset %= 8;
  // while (count-- > 0) {
  //   fillBit(buf, offset++, value);
  // }
  // return;
  if (value) {
    if (offset > 0) {
      if (offset + count < 8) {
        *buf |= (((1 << count) - 1) << offset);
        // while (count-- > 0) {
        //   fillBit(buf, offset++, value);
        // }
        return;
      }
      // while (offset < 8) {
      //   fillBit(buf, offset++, value);
      //   --count;
      // }
      // ++buf;
      *buf++ |= (0xFF << offset);
      count -= (8 - offset);
      offset = 0;
    }
    memset(buf, 0xFF, count / 8);
    buf += (count / 8);
    count %= 8;
    if (count == 0) return;
    *buf |= ((1 << count) - 1);
    // while (count-- > 0) fillBit(buf, offset++, value);
  } else {
    if (offset > 0) {
      if (offset + count < 8) {
        // while (count-- > 0) {
        //   fillBit(buf, offset++, value);
        // }
        *buf &= ~(((1 << count) - 1) << offset);
        return;
      }
      // while (offset < 8) {
      //   fillBit(buf, offset++, value);
      //   --count;
      // }
      // ++buf;
      *buf++ &= ~(0xFF << offset);
      count -= (8 - offset);
      offset = 0;
    }
    memset(buf, 0x00, count / 8);
    buf += (count / 8);
    count %= 8;
    if (count == 0) return;
    *buf &= ~((1 << count) - 1);
    // while (count-- > 0) fillBit(buf, offset++, value);
  }
}

// Rectangular bit mask.
class BitMask {
 public:
  BitMask(uint8_t* buffer, int16_t width, int16_t height)
      : buffer_(buffer), width_(width), height_(height) {}

  uint8_t* buffer() { return buffer_; }
  int16_t width() const { return width_; }
  int16_t height() const { return height_; }

  bool get(int16_t x, int16_t y) const {
    int32_t pixel_idx = (int32_t)y * width_ + x;
    return (buffer_[pixel_idx / 8] & (1 << (pixel_idx % 8))) != 0;
  }

  void clear(int16_t x, int16_t y) {
    int32_t pixel_idx = (int32_t)y * width_ + x;
    buffer_[pixel_idx / 8] &= ~(1 << (pixel_idx % 8));
  }

  // Fills the specified rectangle of the mask with the specified bit value.
  void fillRect(const Box& rect, bool value) {
    uint8_t v = value ? 0xFF : 0x00;
    if (rect.xMin() == 0 && rect.xMax() == width() - 1) {
      fillBits(buffer_, rect.yMin() * width_, rect.height() * width_, value);
    } else {
      uint32_t pixel_idx = rect.xMin() + rect.yMin() * width_;
      int16_t w = rect.width();
      for (int16_t i = rect.height(); i > 0; --i) {
        fillBits(buffer_, pixel_idx, w, value);
        pixel_idx += width_;
      }
    }
  }

 private:
  uint8_t* buffer_;
  int16_t width_;
  int16_t height_;
};

namespace internal {

// Adapter to use writer as filler, for a single rectangle.
struct RectFillWriter {
  Color color;
  BufferedRectWriter& writer;
  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1) const {
    writer.writeRect(x0, y0, x1, y1, color);
  }
};

// Iterates over bits in a specified rectangular sub-window of a bit mask.
class BitMaskWindowIterator {
 public:
  BitMaskWindowIterator(BitMask* bit_mask, int16_t x0, int16_t y0, int16_t x1,
                        int16_t y1)
      : ptr_(bit_mask->buffer() + (y0 * bit_mask->width() + x0) / 8),
        bit_idx_((y0 * bit_mask->width() + x0) % 8),
        x_(0),
        width_(x0 == 0 && x1 == bit_mask->width() - 1
                   ? bit_mask->width() * bit_mask->height()
                   : x1 - x0 + 1),
        width_skip_(bit_mask->width() - (x1 - x0 + 1)) {}

  bool next() {
    if (x_ >= width_) {
      ptr_ += (width_skip_ + bit_idx_) / 8;
      bit_idx_ = (width_skip_ + bit_idx_) % 8;
      x_ = 0;
    }
    bool result = (*ptr_ & (1 << bit_idx_)) != 0;
    ++x_;
    ++bit_idx_;
    ptr_ += (bit_idx_ >= 8);
    bit_idx_ &= 7;
    return result;
  }

 private:
  uint8_t* ptr_;
  uint8_t bit_idx_;
  int16_t x_;
  const int16_t width_;
  const int16_t width_skip_;
};

}  // namespace internal

// Display device which reduces the amount of redundant background re-drawing.
// It maintains a reduced-resolution bit framebuffer. Each bit in the
// framebuffer corresponds to a sub-rectangle of pixels in the underlying
// device. The value of the bit indicates whether the color in the respective
// sub-rectangle is all background. The default resolution reduction is 4x,
// which means that each bit in the mask maps to a 4x4 sub-rectangle. The device
// intercepts fillRect and drawRect, to detect cases when some portions of the
// screen are getting filled with the background color, and set the mask bits
// accordingly. With 4x resolution reduction, for a screen size of 480x320,
// only 120x80 bits -> 1.2 KB storage is needed to maintain the bitmap, yet the
// bulk of background color redraws can usually be avoided.
class BackgroundFillOptimizer : public DisplayOutput {
 public:
  BackgroundFillOptimizer(DisplayOutput* output, BitMask* background_mask)
      : output_(output),
        background_mask_(background_mask),
        address_window_(0, 0, 0, 0),
        cursor_x_(0),
        cursor_y_(0) {}

  virtual ~BackgroundFillOptimizer() {}

  // Sets the color that is considered 'background'.
  void setBackground(Color bgcolor) { bgcolor_ = bgcolor; }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  PaintMode mode) override {
    address_window_ = Box(x0, y0, x1, y1);
    paint_mode_ = mode;
    cursor_x_ = x0;
    cursor_y_ = y0;
  }

  void write(Color* color, uint32_t pixel_count) override {
    // Naive implementation, for now.
    uint32_t i = 0;
    BufferedPixelWriter writer(output_, paint_mode_);
    while (pixel_count-- > 0) {
      writePixel(cursor_x_, cursor_y_, *color++, &writer);
      if (++cursor_x_ > address_window_.xMax()) {
        ++cursor_y_;
        cursor_x_ = address_window_.xMin();
      }
    }
  }

  void writeRects(PaintMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    BufferedRectWriter writer(output_, mode);
    while (count-- > 0) {
      Color c = *color++;
      if (c == bgcolor_) {
        internal::RectFillWriter adapter {
          .color = c,
          .writer = writer,
        };
        fillRectBg(*x0++, *y0++, *x1++, *y1++, &adapter);
      } else {
        // Not a background color -> clear the bit mask (corresponding to an
        // bit-mask rectangle entirely contained in the drawn rectangle) before
        // writing to the underlying device.
        background_mask_->fillRect(Box(*x0 / kBgFillOptimizerWindowSize,
                                       *y0 / kBgFillOptimizerWindowSize,
                                       *x1 / kBgFillOptimizerWindowSize,
                                       *y1 / kBgFillOptimizerWindowSize),
                                   false);
        writer.writeRect(*x0++, *y0++, *x1++, *y1++, c);
      }
    }
  }

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    if (color == bgcolor_) {
      BufferedRectFiller filler(output_, color, mode);
      while (count-- > 0) {
        fillRectBg(*x0++, *y0++, *x1++, *y1++, &filler);
      }
    } else {
      for (int i = 0; i < count; ++i) {
        // Not a background color -> clear the bit mask (corresponding to an
        // bit-mask rectangle entirely contained in the drawn rectangle) before
        // writing to the underlying device.
        background_mask_->fillRect(Box(x0[i] / kBgFillOptimizerWindowSize,
                                       y0[i] / kBgFillOptimizerWindowSize,
                                       x1[i] / kBgFillOptimizerWindowSize,
                                       y1[i] / kBgFillOptimizerWindowSize),
                                   false);
      }
      output_->fillRects(mode, color, x0, y0, x1, y1, count);
    }
  }

  void writePixels(PaintMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    int16_t* x_out = x;
    int16_t* y_out = y;
    Color* color_out = color;
    uint16_t new_pixel_count = 0;
    for (uint16_t i = 0; i < pixel_count; ++i) {
      if (color[i] == bgcolor_) {
        // Do not actually draw the background pixel if the corresponding
        // bit-mask rectangle is known to be all-background already.
        if (background_mask_->get(x[i] / kBgFillOptimizerWindowSize,
                                  y[i] / kBgFillOptimizerWindowSize))
          continue;
      } else {
        // Mark the corresponding bit-mask rectangle as no longer
        // all-background.
        background_mask_->clear(x[i] / kBgFillOptimizerWindowSize,
                                y[i] / kBgFillOptimizerWindowSize);
      }
      *x_out++ = x[i];
      *y_out++ = y[i];
      *color_out++ = color[i];
      new_pixel_count++;
    }
    if (new_pixel_count > 0) {
      output_->writePixels(mode, color, x, y, new_pixel_count);
    }
  }

  void fillPixels(PaintMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    if (color == bgcolor_) {
      int16_t* x_out = x;
      int16_t* y_out = y;
      uint16_t new_pixel_count = 0;
      for (uint16_t i = 0; i < pixel_count; ++i) {
        // Do not actually draw the background pixel if the corresponding
        // bit-mask rectangle is known to be all-background already.
        if (background_mask_->get(x[i] / kBgFillOptimizerWindowSize,
                                  y[i] / kBgFillOptimizerWindowSize))
          continue;
        *x_out++ = x[i];
        *y_out++ = y[i];
        new_pixel_count++;
      }
      if (new_pixel_count > 0) {
        output_->fillPixels(mode, color, x, y, new_pixel_count);
      }
    } else {
      // We need to draw all the pixels, but also mark the corresponding
      // bit-mask rectangles as not all-background.
      for (uint16_t i = 0; i < pixel_count; ++i) {
        background_mask_->clear(x[i] / kBgFillOptimizerWindowSize,
                                y[i] / kBgFillOptimizerWindowSize);
      }
      output_->fillPixels(mode, color, x, y, pixel_count);
    }
  }

 private:
  void writePixel(int16_t x, int16_t y, Color c, BufferedPixelWriter* writer) {
    if (c == bgcolor_) {
      // Skip writing if the containing bit-mask mapped rectangle
      // is known to be all-background already.
      if (background_mask_->get(x / kBgFillOptimizerWindowSize,
                                y / kBgFillOptimizerWindowSize))
        return;
    } else {
      // Mark the corresponding bit-mask mapped rectangle as no longer
      // all-background.
      background_mask_->clear(x / kBgFillOptimizerWindowSize,
                              y / kBgFillOptimizerWindowSize);
    }
    writer->writePixel(x, y, c);
  }

  template <typename Filler>
  void fillRectBg(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  Filler* filler) {
    // Iterate over the bit-map rectangle that encloses the requested rectangle.
    // Skip writing sub-rectangles that are known to be already all-background.
    Box filter_box(
        x0 / kBgFillOptimizerWindowSize, y0 / kBgFillOptimizerWindowSize,
        x1 / kBgFillOptimizerWindowSize, y1 / kBgFillOptimizerWindowSize);
    internal::BitMaskWindowIterator itr(background_mask_, filter_box.xMin(),
                                        filter_box.yMin(), filter_box.xMax(),
                                        filter_box.yMax());
    for (int16_t y = filter_box.yMin(); y <= filter_box.yMax(); ++y) {
      int16_t xstart = -1;
      for (int16_t x = filter_box.xMin(); x <= filter_box.xMax(); ++x) {
        bool bgfilled = itr.next();
        if (bgfilled) {
          // End of the streak. Let's draw the necessary rectangle.
          if (xstart >= 0) {
            // Clip to the target rectangle.
            Box b = Box::intersect(Box(x0, y0, x1, y1),
                                   Box(xstart * kBgFillOptimizerWindowSize,
                                       y * kBgFillOptimizerWindowSize,
                                       x * kBgFillOptimizerWindowSize - 1,
                                       y * kBgFillOptimizerWindowSize +
                                           kBgFillOptimizerWindowSize - 1));
            filler->fillRect(b.xMin(), b.yMin(), b.xMax(), b.yMax());
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
      // End of line. If there was a streak, draw it now.
      if (xstart >= 0) {
        Box b = Box::intersect(
            Box(x0, y0, x1, y1),
            Box(xstart * kBgFillOptimizerWindowSize,
                y * kBgFillOptimizerWindowSize,
                (filter_box.xMax() + 1) * kBgFillOptimizerWindowSize - 1,
                y * kBgFillOptimizerWindowSize + kBgFillOptimizerWindowSize -
                    1));
        filler->fillRect(b.xMin(), b.yMin(), b.xMax(), b.yMax());
      }
    }
    // Identify the bit-mask rectangle that is entirely covered by the
    // requested rect. If non-empty, set all bits corresponding to it in
    // the mask, indicating that the area is all-background.
    Box inner_filter_box(
        (x0 + kBgFillOptimizerWindowSize - 1) / kBgFillOptimizerWindowSize,
        (y0 + kBgFillOptimizerWindowSize - 1) / kBgFillOptimizerWindowSize,
        (x1 + 1) / kBgFillOptimizerWindowSize - 1,
        (y1 + 1) / kBgFillOptimizerWindowSize - 1);
    if (!inner_filter_box.empty()) {
      background_mask_->fillRect(inner_filter_box, true);
    }
  }

  DisplayOutput* output_;
  BitMask* background_mask_;
  Color bgcolor_;

  Box address_window_;
  PaintMode paint_mode_;
  int16_t cursor_x_;
  int16_t cursor_y_;
};

}  // namespace roo_display
