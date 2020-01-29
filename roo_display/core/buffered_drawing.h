#pragma once

// Utilities for buffering pixel writes before they are sent out to underlying
// device drivers. The buffering has enormous performance impact:
// * for hardware devices, e.g. SPI-based displays, it allows the driver to
//   optimize commands by noticing patterns like horizontal or vertical streaks,
//   and to amortize per-command overheads in the protocol layer.
// * for in-memory buffers, eliminating per-pixel virtual calls leads to
//   massive performance improvements as well.
//
// Specific utility classes are provided for combinations of:
// * various write patterns: writing random pixels, subsequent pixels, horizontal
//   lines, vertical lines, rectangles;
// * using different colors (*Writer) vs a single color (*Filler);
// * additionally performing pre-clipping (Clipping* variants) or not.
//   The non-clipping variants can be used (and will be slightly faster) when
//   it is known that all drawn primitives fit in the pre-assumed clipping
//   rectangle (e.g., within the hardware-supported bounds).

#include <memory>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/memfill.h"

namespace roo_display {

static const uint8_t kPixelWritingBufferSize = 64;

class BufferedPixelWriter {
 public:
  BufferedPixelWriter(DisplayOutput* device,
                      PaintMode mode = PAINT_MODE_REPLACE)
      : device_(device), mode_(mode), buffer_size_(0) {}

  void writePixel(int16_t x, int16_t y, Color color) {
    if (buffer_size_ == kPixelWritingBufferSize) flush();
    if (color.a() == 0 && mode_ == PAINT_MODE_BLEND) return;
    x_buffer_[buffer_size_] = x;
    y_buffer_[buffer_size_] = y;
    color_buffer_[buffer_size_] = color;
    ++buffer_size_;
  }

  ~BufferedPixelWriter() { flush(); }

  void flush() {
    if (buffer_size_ == 0) return;
    device_->writePixels(mode_, color_buffer_, x_buffer_, y_buffer_,
                         buffer_size_);
    buffer_size_ = 0;
  }

 private:
  DisplayOutput* device_;
  PaintMode mode_;
  Color color_buffer_[kPixelWritingBufferSize];
  int16_t x_buffer_[kPixelWritingBufferSize];
  int16_t y_buffer_[kPixelWritingBufferSize];
  int16_t buffer_size_;
};

class ClippingBufferedPixelWriter {
 public:
  ClippingBufferedPixelWriter(DisplayOutput* device, Box clip_box,
                              PaintMode mode = PAINT_MODE_REPLACE)
      : writer_(device, mode), clip_box_(std::move(clip_box)) {}

  void writePixel(int16_t x, int16_t y, Color color) {
    if (clip_box_.contains(x, y)) {
      writer_.writePixel(x, y, color);
    }
  }

  void flush() { writer_.flush(); }

  const Box& clip_box() const { return clip_box_; }

  void set_clip_box(Box clip_box) { clip_box_ = std::move(clip_box); }

 private:
  BufferedPixelWriter writer_;
  Box clip_box_;
};

class BufferedPixelFiller {
 public:
  BufferedPixelFiller(DisplayOutput* device, Color color,
                      PaintMode mode = PAINT_MODE_REPLACE)
      : device_(device), color_(color), mode_(mode), buffer_size_(0) {}

  void fillPixel(int16_t x, int16_t y) {
    if (buffer_size_ == kPixelWritingBufferSize) flush();
    x_buffer_[buffer_size_] = x;
    y_buffer_[buffer_size_] = y;
    ++buffer_size_;
  }

  ~BufferedPixelFiller() { flush(); }

  void flush() {
    if (buffer_size_ == 0) return;
    device_->fillPixels(mode_, color_, x_buffer_, y_buffer_, buffer_size_);
    buffer_size_ = 0;
  }

 private:
  DisplayOutput* device_;
  Color color_;
  PaintMode mode_;
  int16_t x_buffer_[kPixelWritingBufferSize];
  int16_t y_buffer_[kPixelWritingBufferSize];
  int16_t buffer_size_;
};

class ClippingBufferedPixelFiller {
 public:
  ClippingBufferedPixelFiller(DisplayOutput* device, Color color,
                              Box clip_box,
                              PaintMode mode = PAINT_MODE_REPLACE)
      : filler_(device, color, mode), clip_box_(std::move(clip_box)) {}

  void fillPixel(int16_t x, int16_t y) {
    if (clip_box_.contains(x, y)) {
      filler_.fillPixel(x, y);
    }
  }

  void flush() { filler_.flush(); }

  const Box& clip_box() const { return clip_box_; }

  void set_clip_box(Box clip_box) { clip_box_ = std::move(clip_box); }

 private:
  BufferedPixelFiller filler_;
  Box clip_box_;
};

class BufferedColorWriter {
 public:
  BufferedColorWriter(DisplayOutput* device,
                      PaintMode mode = PAINT_MODE_REPLACE)
      : device_(device), mode_(mode), buffer_size_(0) {}

  void writeColor(Color color) {
    if (buffer_size_ == kPixelWritingBufferSize) flush();
    color_buffer_[buffer_size_] = color;
    ++buffer_size_;
  }

  ~BufferedColorWriter() { flush(); }

 private:
  void flush() {
    if (buffer_size_ == 0) return;
    device_->write(mode_, color_buffer_, buffer_size_);
    buffer_size_ = 0;
  }

  DisplayOutput* device_;
  PaintMode mode_;
  Color color_buffer_[kPixelWritingBufferSize];
  int16_t buffer_size_;
};

class BufferedHLineFiller {
 public:
  BufferedHLineFiller(DisplayOutput* device, Color color,
                      PaintMode mode = PAINT_MODE_REPLACE)
      : device_(device), mode_(mode), color_(color), buffer_size_(0) {}

  void fillHLine(int16_t x0, int16_t y0, int16_t x1) {
    if (buffer_size_ == kPixelWritingBufferSize) flush();
    x0_buffer_[buffer_size_] = x0;
    y0_buffer_[buffer_size_] = y0;
    x1_buffer_[buffer_size_] = x1;
    ++buffer_size_;
  }

  ~BufferedHLineFiller() { flush(); }

  void flush() {
    if (buffer_size_ == 0) return;
    device_->fillRects(mode_, color_, x0_buffer_, y0_buffer_, x1_buffer_,
                       y0_buffer_, buffer_size_);
    buffer_size_ = 0;
  }

 private:
  DisplayOutput* device_;
  PaintMode mode_;
  Color color_;
  int16_t x0_buffer_[kPixelWritingBufferSize];
  int16_t y0_buffer_[kPixelWritingBufferSize];
  int16_t x1_buffer_[kPixelWritingBufferSize];
  int16_t buffer_size_;
};

class ClippingBufferedHLineFiller {
 public:
  ClippingBufferedHLineFiller(DisplayOutput* device, Color color,
                              Box clip_box,
                              PaintMode mode = PAINT_MODE_REPLACE)
      : filler_(device, color, mode), clip_box_(std::move(clip_box)) {}

  void fillHLine(int16_t x0, int16_t y0, int16_t x1) {
    if (x0 > clip_box_.xMax() || x1 < clip_box_.xMin() ||
        y0 > clip_box_.yMax() || y0 < clip_box_.yMin() || x1 < x0) {
      return;
    }

    if (x0 < clip_box_.xMin()) x0 = clip_box_.xMin();
    if (x1 > clip_box_.xMax()) x1 = clip_box_.xMax();
    filler_.fillHLine(x0, y0, x1);
  }

  void flush() { filler_.flush(); }

  const Box& clip_box() const { return clip_box_; }

  void set_clip_box(Box clip_box) { clip_box_ = std::move(clip_box); }

 private:
  BufferedHLineFiller filler_;
  Box clip_box_;
};

class BufferedVLineFiller {
 public:
  BufferedVLineFiller(DisplayOutput* device, Color color,
                      PaintMode mode = PAINT_MODE_REPLACE)
      : device_(device), mode_(mode), color_(color), buffer_size_(0) {}

  void fillVLine(int16_t x0, int16_t y0, int16_t y1) {
    if (buffer_size_ == kPixelWritingBufferSize) flush();
    x0_buffer_[buffer_size_] = x0;
    y0_buffer_[buffer_size_] = y0;
    y1_buffer_[buffer_size_] = y1;
    ++buffer_size_;
  }

  ~BufferedVLineFiller() { flush(); }

  void flush() {
    if (buffer_size_ == 0) return;
    device_->fillRects(mode_, color_, x0_buffer_, y0_buffer_, x0_buffer_,
                       y1_buffer_, buffer_size_);
    buffer_size_ = 0;
  }

 private:
  DisplayOutput* device_;
  PaintMode mode_;
  Color color_;
  int16_t x0_buffer_[kPixelWritingBufferSize];
  int16_t y0_buffer_[kPixelWritingBufferSize];
  int16_t y1_buffer_[kPixelWritingBufferSize];
  int16_t buffer_size_;
};

class ClippingBufferedVLineFiller {
 public:
  ClippingBufferedVLineFiller(DisplayOutput* device, Color color,
                              Box clip_box,
                              PaintMode mode = PAINT_MODE_REPLACE)
      : filler_(device, color, mode), clip_box_(std::move(clip_box)) {}

  void fillVLine(int16_t x0, int16_t y0, int16_t y1) {
    if (x0 > clip_box_.xMax() || x0 < clip_box_.xMin() ||
        y0 > clip_box_.yMax() || y1 < clip_box_.yMin() || y1 < y0) {
      return;
    }

    if (y0 < clip_box_.yMin()) y0 = clip_box_.yMin();
    if (y1 > clip_box_.yMax()) y1 = clip_box_.yMax();
    filler_.fillVLine(x0, y0, y1);
  }

  void flush() { filler_.flush(); }

  const Box& clip_box() const { return clip_box_; }

  void set_clip_box(Box clip_box) { clip_box_ = std::move(clip_box); }

 private:
  BufferedVLineFiller filler_;
  Box clip_box_;
};

class BufferedRectWriter {
 public:
  BufferedRectWriter(DisplayOutput* device, PaintMode mode = PAINT_MODE_REPLACE)
      : device_(device), mode_(mode), buffer_size_(0) {}

  inline void writePixel(int16_t x, int16_t y, Color color) {
    writeRect(x, y, x, y, color);
  }

  inline void writeHLine(int16_t x0, int16_t y0, int16_t x1, Color color) {
    writeRect(x0, y0, x1, y0, color);
  }

  inline void writeVLine(int16_t x0, int16_t y0, int16_t y1, Color color) {
    writeRect(x0, y0, x0, y1, color);
  }

  void writeRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color) {
    if (buffer_size_ == kPixelWritingBufferSize) flush();
    color_[buffer_size_] = color;
    x0_buffer_[buffer_size_] = x0;
    y0_buffer_[buffer_size_] = y0;
    x1_buffer_[buffer_size_] = x1;
    y1_buffer_[buffer_size_] = y1;
    ++buffer_size_;
  }

  ~BufferedRectWriter() { flush(); }

  void flush() {
    if (buffer_size_ == 0) return;
    device_->writeRects(mode_, color_, x0_buffer_, y0_buffer_, x1_buffer_,
                        y1_buffer_, buffer_size_);
    buffer_size_ = 0;
  }

 private:
  DisplayOutput* device_;
  PaintMode mode_;
  Color color_[kPixelWritingBufferSize];
  int16_t x0_buffer_[kPixelWritingBufferSize];
  int16_t y0_buffer_[kPixelWritingBufferSize];
  int16_t x1_buffer_[kPixelWritingBufferSize];
  int16_t y1_buffer_[kPixelWritingBufferSize];
  int16_t buffer_size_;
};

class ClippingBufferedRectWriter {
 public:
  ClippingBufferedRectWriter(DisplayOutput* device, Box clip_box,
                             PaintMode mode = PAINT_MODE_REPLACE)
      : writer_(device, mode), clip_box_(std::move(clip_box)) {}

  inline void writePixel(int16_t x, int16_t y, Color color) {
    writeRect(x, y, x, y, color);
  }

  inline void writeHLine(int16_t x0, int16_t y0, int16_t x1, Color color) {
    writeRect(x0, y0, x1, y0, color);
  }

  inline void writeVLine(int16_t x0, int16_t y0, int16_t y1, Color color) {
    writeRect(x0, y0, x0, y1, color);
  }

  void writeRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, Color color) {
    if (x0 > clip_box_.xMax() || x1 < clip_box_.xMin() ||
        y0 > clip_box_.yMax() || y1 < clip_box_.yMin() || y1 < y0 ||
        x1 < x0) {
      return;
    }

    if (x0 < clip_box_.xMin()) x0 = clip_box_.xMin();
    if (x1 > clip_box_.xMax()) x1 = clip_box_.xMax();
    if (y0 < clip_box_.yMin()) y0 = clip_box_.yMin();
    if (y1 > clip_box_.yMax()) y1 = clip_box_.yMax();

    writer_.writeRect(x0, y0, x1, y1, color);
  }

  void flush() { writer_.flush(); }

  const Box& clip_box() const { return clip_box_; }

  void set_clip_box(Box clip_box) { clip_box_ = std::move(clip_box); }

 private:
  BufferedRectWriter writer_;
  Box clip_box_;
};

class BufferedRectFiller {
 public:
  BufferedRectFiller(DisplayOutput* device, Color color,
                     PaintMode mode = PAINT_MODE_REPLACE)
      : device_(device), mode_(mode), color_(color), buffer_size_(0) {}

  inline void fillPixel(int16_t x, int16_t y) { fillRect(x, y, x, y); }

  inline void fillHLine(int16_t x0, int16_t y0, int16_t x1) {
    fillRect(x0, y0, x1, y0);
  }

  inline void fillVLine(int16_t x0, int16_t y0, int16_t y1) {
    fillRect(x0, y0, x0, y1);
  }

  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    if (buffer_size_ == kPixelWritingBufferSize) flush();
    x0_buffer_[buffer_size_] = x0;
    y0_buffer_[buffer_size_] = y0;
    x1_buffer_[buffer_size_] = x1;
    y1_buffer_[buffer_size_] = y1;
    ++buffer_size_;
  }

  ~BufferedRectFiller() { flush(); }

  void flush() {
    if (buffer_size_ == 0) return;
    device_->fillRects(mode_, color_, x0_buffer_, y0_buffer_, x1_buffer_,
                       y1_buffer_, buffer_size_);
    buffer_size_ = 0;
  }

 private:
  DisplayOutput* device_;
  PaintMode mode_;
  Color color_;
  int16_t x0_buffer_[kPixelWritingBufferSize];
  int16_t y0_buffer_[kPixelWritingBufferSize];
  int16_t x1_buffer_[kPixelWritingBufferSize];
  int16_t y1_buffer_[kPixelWritingBufferSize];
  int16_t buffer_size_;
};

class ClippingBufferedRectFiller {
 public:
  ClippingBufferedRectFiller(DisplayOutput* device, Color color, Box clip_box,
                             PaintMode mode = PAINT_MODE_REPLACE)
      : filler_(device, color, mode), clip_box_(std::move(clip_box)) {}

  inline void fillPixel(int16_t x, int16_t y) { fillRect(x, y, x, y); }

  inline void fillHLine(int16_t x0, int16_t y0, int16_t x1) {
    fillRect(x0, y0, x1, y0);
  }

  inline void fillVLine(int16_t x0, int16_t y0, int16_t y1) {
    fillRect(x0, y0, x0, y1);
  }

  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    if (x0 > clip_box_.xMax() || x1 < clip_box_.xMin() ||
        y0 > clip_box_.yMax() || y1 < clip_box_.yMin() || y1 < y0 ||
        x1 < x0) {
      return;
    }

    if (x0 < clip_box_.xMin()) x0 = clip_box_.xMin();
    if (x1 > clip_box_.xMax()) x1 = clip_box_.xMax();
    if (y0 < clip_box_.yMin()) y0 = clip_box_.yMin();
    if (y1 > clip_box_.yMax()) y1 = clip_box_.yMax();

    filler_.fillRect(x0, y0, x1, y1);
  }

  void flush() { filler_.flush(); }

  const Box& clip_box() const { return clip_box_; }

  void set_clip_box(Box clip_box) { clip_box_ = std::move(clip_box); }

 private:
  BufferedRectFiller filler_;
  Box clip_box_;
};

}  // namespace roo_display