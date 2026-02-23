#pragma once

/// Utilities for buffering pixel writes before sending to device drivers.
///
/// Buffering has a large performance impact:
/// - Hardware drivers (e.g., SPI displays) can batch and optimize transfers.
/// - In-memory buffers avoid per-pixel virtual calls.
///
/// Utility classes cover common patterns:
/// - random pixels, sequences, lines, rectangles
/// - per-pixel colors (*Writer) vs single color (*Filler)
/// - optional pre-clipping (Clipping* variants)

#include <memory>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"

namespace roo_display {

#ifndef ROO_DISPLAY_TESTING
static const uint8_t kPixelWritingBufferSize = 64;

// Smaller, to conserve stack memory.
static const uint8_t kRectWritingBufferSize = 32;

#else
// Use a small and 'weird' buffer size to challenge unit tests better.
static const uint8_t kPixelWritingBufferSize = 5;
static const uint8_t kRectWritingBufferSize = 5;
#endif

/// Buffered writer for arbitrary pixels with per-pixel colors.
class BufferedPixelWriter {
 public:
  /// Construct a writer targeting `device` using `blending_mode`.
  BufferedPixelWriter(DisplayOutput& device, BlendingMode blending_mode)
      : device_(device), blending_mode_(blending_mode), buffer_size_(0) {}

  BufferedPixelWriter(BufferedPixelWriter&) = delete;
  BufferedPixelWriter(const BufferedPixelWriter&) = delete;

  BufferedPixelWriter& operator=(const BufferedPixelWriter&) = delete;
  BufferedPixelWriter& operator=(BufferedPixelWriter&&) = delete;

  /// Write a single pixel (buffered).
  void writePixel(int16_t x, int16_t y, Color color) {
    if (buffer_size_ == kPixelWritingBufferSize) flush();
    if (color.asArgb() == 0 && (blending_mode_ == kBlendingSourceOver ||
                                blending_mode_ == kBlendingSourceOverOpaque))
      return;
    x_buffer_[buffer_size_] = x;
    y_buffer_[buffer_size_] = y;
    color_buffer_[buffer_size_] = color;
    ++buffer_size_;
  }

  ~BufferedPixelWriter() { flush(); }

  /// Flush any buffered pixels.
  void flush() {
    if (buffer_size_ == 0) return;
    device_.writePixels(blending_mode_, color_buffer_, x_buffer_, y_buffer_,
                        buffer_size_);
    buffer_size_ = 0;
  }

 private:
  DisplayOutput& device_;
  BlendingMode blending_mode_;
  int16_t buffer_size_;
  Color color_buffer_[kPixelWritingBufferSize];
  int16_t x_buffer_[kPixelWritingBufferSize];
  int16_t y_buffer_[kPixelWritingBufferSize];
};

template <typename PixelWriter>
/// Adapter that turns a pixel writer into a single-color filler.
class BufferedPixelWriterFillAdapter {
 public:
  BufferedPixelWriterFillAdapter(PixelWriter& writer, Color color)
      : writer_(writer), color_(color) {}

  /// Fill a pixel using the configured color.
  void fillPixel(int16_t x, int16_t y) { writer_.writePixel(x, y, color_); }

 private:
  PixelWriter& writer_;
  Color color_;
};

/// Buffered pixel writer with a clipping box.
class ClippingBufferedPixelWriter {
 public:
  ClippingBufferedPixelWriter(DisplayOutput& device, Box clip_box,
                              BlendingMode blending_mode)
      : writer_(device, blending_mode), clip_box_(std::move(clip_box)) {}

  ClippingBufferedPixelWriter(ClippingBufferedPixelWriter&) = delete;
  ClippingBufferedPixelWriter(const ClippingBufferedPixelWriter&) = delete;

  /// Write a pixel if it lies within `clip_box()`.
  void writePixel(int16_t x, int16_t y, Color color) {
    if (clip_box_.contains(x, y)) {
      writer_.writePixel(x, y, color);
    }
  }

  /// Flush any buffered pixels.
  void flush() { writer_.flush(); }

  const Box& clip_box() const { return clip_box_; }

  // void set_clip_box(Box clip_box) { clip_box_ = std::move(clip_box); }

 private:
  BufferedPixelWriter writer_;
  Box clip_box_;
};

/// Default pixel writer type (with clipping).
using PixelWriter = ClippingBufferedPixelWriter;

/// Buffered filler for arbitrary pixels using a single color.
class BufferedPixelFiller {
 public:
  BufferedPixelFiller(DisplayOutput& device, Color color,
                      BlendingMode blending_mode)
      : device_(device),
        color_(color),
        blending_mode_(blending_mode),
        buffer_size_(0) {}

  /// Fill a pixel (buffered).
  void fillPixel(int16_t x, int16_t y) {
    if (buffer_size_ == kPixelWritingBufferSize) flush();
    x_buffer_[buffer_size_] = x;
    y_buffer_[buffer_size_] = y;
    ++buffer_size_;
  }

  ~BufferedPixelFiller() { flush(); }

  /// Flush any buffered pixels.
  void flush() {
    if (buffer_size_ == 0) return;
    device_.fillPixels(blending_mode_, color_, x_buffer_, y_buffer_,
                       buffer_size_);
    buffer_size_ = 0;
  }

 private:
  DisplayOutput& device_;
  Color color_;
  BlendingMode blending_mode_;
  int16_t buffer_size_;
  int16_t x_buffer_[kPixelWritingBufferSize];
  int16_t y_buffer_[kPixelWritingBufferSize];
};

/// Buffered pixel filler with a clipping box.
class ClippingBufferedPixelFiller {
 public:
  ClippingBufferedPixelFiller(DisplayOutput& device, Color color, Box clip_box,
                              BlendingMode blending_mode)
      : filler_(device, color, blending_mode), clip_box_(std::move(clip_box)) {}

  /// Fill a pixel if it lies within `clip_box()`.
  void fillPixel(int16_t x, int16_t y) {
    if (clip_box_.contains(x, y)) {
      filler_.fillPixel(x, y);
    }
  }

  /// Flush any buffered pixels.
  void flush() { filler_.flush(); }

  const Box& clip_box() const { return clip_box_; }

  // void set_clip_box(Box clip_box) { clip_box_ = std::move(clip_box); }

 private:
  BufferedPixelFiller filler_;
  Box clip_box_;
};

/// Buffered writer for sequential color values (used with `setAddress()`).
class BufferedColorWriter {
 public:
  BufferedColorWriter(DisplayOutput& device)
      : device_(device), buffer_size_(0) {}

  /// Write a single color value.
  void writeColor(Color color) {
    color_buffer_[buffer_size_] = color;
    ++buffer_size_;
    if (buffer_size_ == kPixelWritingBufferSize) flush();
  }

  /// Write `count` pixels with the same color.
  void writeColorN(Color color, uint16_t count) {
    uint16_t batch = kPixelWritingBufferSize - buffer_size_;
    if (count < batch) {
      FillColor(color_buffer_ + buffer_size_, count, color);
      buffer_size_ += count;
      return;
    }
    FillColor(color_buffer_ + buffer_size_, batch, color);
    device_.write(color_buffer_, kPixelWritingBufferSize);
    count -= batch;
    while (true) {
      if (count < kPixelWritingBufferSize) {
        FillColor(color_buffer_, count, color);
        buffer_size_ = count;
        return;
      }
      FillColor(color_buffer_, kPixelWritingBufferSize, color);
      device_.write(color_buffer_, kPixelWritingBufferSize);
      count -= kPixelWritingBufferSize;
    }
  }

  /// Write `count` pixels with distinct colors.
  void writeColors(Color* colors, uint16_t count) {
    uint16_t batch = kPixelWritingBufferSize - buffer_size_;
    if (count < batch) {
      memcpy(color_buffer_ + buffer_size_, colors, count * sizeof(Color));
      buffer_size_ += count;
      return;
    }
    if (buffer_size_ != 0) {
      memcpy(color_buffer_ + buffer_size_, colors, batch * sizeof(Color));
      device_.write(color_buffer_, kPixelWritingBufferSize);
      count -= batch;
      colors += batch;
    }
    while (true) {
      if (count < kPixelWritingBufferSize) {
        memcpy(color_buffer_, colors, count * sizeof(Color));
        buffer_size_ = count;
        return;
      }
      // For large enough number of pixels, we can skip the buffer entirely.
      device_.write(colors, kPixelWritingBufferSize);
      count -= kPixelWritingBufferSize;
      colors += kPixelWritingBufferSize;
    }
  }

  /// Return remaining buffer capacity before a flush is needed.
  uint16_t remaining_buffer_space() const {
    return kPixelWritingBufferSize - buffer_size_;
  }

  /// Return raw access to the buffer at the current write position.
  Color* buffer_ptr() { return color_buffer_ + buffer_size_; }

  /// Advance the buffer after direct writes via `buffer_ptr()`.
  void advance_buffer_ptr(uint16_t count) {
    buffer_size_ += count;
    if (buffer_size_ >= kPixelWritingBufferSize) {
      device_.write(color_buffer_, buffer_size_);
      buffer_size_ = 0;
    }
  }

  /// Write up to the remaining buffer space and return count written.
  // written.
  uint16_t writeColorsAligned(Color* colors, uint16_t count) {
    uint16_t batch = kPixelWritingBufferSize - buffer_size_;
    if (count < batch) {
      memcpy(color_buffer_ + buffer_size_, colors, count * sizeof(Color));
      buffer_size_ += count;
      return count;
    }
    if (buffer_size_ != 0) {
      memcpy(color_buffer_ + buffer_size_, colors, batch * sizeof(Color));
      device_.write(color_buffer_, kPixelWritingBufferSize);
      buffer_size_ = 0;
      return batch;
    }
    device_.write(colors, kPixelWritingBufferSize);
    return kPixelWritingBufferSize;
  }

  ~BufferedColorWriter() { flush(); }

 private:
  void flush() {
    if (buffer_size_ == 0) return;
    device_.write(color_buffer_, buffer_size_);
    buffer_size_ = 0;
  }

  DisplayOutput& device_;
  Color color_buffer_[kPixelWritingBufferSize];
  int16_t buffer_size_;
};

class BufferedHLineFiller {
 public:
  BufferedHLineFiller(DisplayOutput& device, Color color,
                      BlendingMode blending_mode)
      : device_(device),
        blending_mode_(blending_mode),
        color_(color),
        buffer_size_(0) {}

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
    device_.fillRects(blending_mode_, color_, x0_buffer_, y0_buffer_,
                      x1_buffer_, y0_buffer_, buffer_size_);
    buffer_size_ = 0;
  }

 private:
  DisplayOutput& device_;
  BlendingMode blending_mode_;
  Color color_;
  int16_t buffer_size_;
  int16_t x0_buffer_[kPixelWritingBufferSize];
  int16_t y0_buffer_[kPixelWritingBufferSize];
  int16_t x1_buffer_[kPixelWritingBufferSize];
};

class ClippingBufferedHLineFiller {
 public:
  ClippingBufferedHLineFiller(DisplayOutput& device, Color color, Box clip_box,
                              BlendingMode blending_mode)
      : filler_(device, color, blending_mode), clip_box_(std::move(clip_box)) {}

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

  // void set_clip_box(Box clip_box) { clip_box_ = std::move(clip_box); }

 private:
  BufferedHLineFiller filler_;
  Box clip_box_;
};

class BufferedVLineFiller {
 public:
  BufferedVLineFiller(DisplayOutput& device, Color color,
                      BlendingMode blending_mode)
      : device_(device),
        blending_mode_(blending_mode),
        color_(color),
        buffer_size_(0) {}

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
    device_.fillRects(blending_mode_, color_, x0_buffer_, y0_buffer_,
                      x0_buffer_, y1_buffer_, buffer_size_);
    buffer_size_ = 0;
  }

 private:
  DisplayOutput& device_;
  BlendingMode blending_mode_;
  Color color_;
  int16_t buffer_size_;
  int16_t x0_buffer_[kPixelWritingBufferSize];
  int16_t y0_buffer_[kPixelWritingBufferSize];
  int16_t y1_buffer_[kPixelWritingBufferSize];
};

class ClippingBufferedVLineFiller {
 public:
  ClippingBufferedVLineFiller(DisplayOutput& device, Color color, Box clip_box,
                              BlendingMode blending_mode)
      : filler_(device, color, blending_mode), clip_box_(std::move(clip_box)) {}

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

  // void set_clip_box(Box clip_box) { clip_box_ = std::move(clip_box); }

 private:
  BufferedVLineFiller filler_;
  Box clip_box_;
};

class BufferedRectWriter {
 public:
  BufferedRectWriter(DisplayOutput& device, BlendingMode blending_mode)
      : device_(device), blending_mode_(blending_mode), buffer_size_(0) {}

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
    if (buffer_size_ == kRectWritingBufferSize) flush();
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
    device_.writeRects(blending_mode_, color_, x0_buffer_, y0_buffer_,
                       x1_buffer_, y1_buffer_, buffer_size_);
    buffer_size_ = 0;
  }

 private:
  DisplayOutput& device_;
  BlendingMode blending_mode_;
  int16_t buffer_size_;
  Color color_[kRectWritingBufferSize];
  int16_t x0_buffer_[kRectWritingBufferSize];
  int16_t y0_buffer_[kRectWritingBufferSize];
  int16_t x1_buffer_[kRectWritingBufferSize];
  int16_t y1_buffer_[kRectWritingBufferSize];
};

template <typename RectWriter>
class BufferedRectWriterFillAdapter {
 public:
  BufferedRectWriterFillAdapter(RectWriter& writer, Color color)
      : writer_(writer), color_(color) {}

  inline void fillPixel(int16_t x, int16_t y) {
    writer_.writePixel(x, y, color_);
  }

  inline void fillHLine(int16_t x0, int16_t y0, int16_t x1) {
    writer_.fillHLine(x0, y0, x1, color_);
  }

  inline void fillVLine(int16_t x0, int16_t y0, int16_t y1) {
    writer_.fillVLine(x0, y0, y1, color_);
  }

  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    writer_.writeRect(x0, y0, x1, y1, color_);
  }

 private:
  RectWriter& writer_;
  Color color_;
};

class ClippingBufferedRectWriter {
 public:
  ClippingBufferedRectWriter(DisplayOutput& device, Box clip_box,
                             BlendingMode blending_mode)
      : writer_(device, blending_mode), clip_box_(std::move(clip_box)) {}

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
        y0 > clip_box_.yMax() || y1 < clip_box_.yMin() || y1 < y0 || x1 < x0) {
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

  // void set_clip_box(Box clip_box) { clip_box_ = std::move(clip_box); }

 private:
  BufferedRectWriter writer_;
  Box clip_box_;
};

class BufferedRectFiller {
 public:
  BufferedRectFiller(DisplayOutput& device, Color color,
                     BlendingMode blending_mode)
      : device_(device),
        blending_mode_(blending_mode),
        color_(color),
        buffer_size_(0) {}

  inline void fillPixel(int16_t x, int16_t y) { fillRect(x, y, x, y); }

  inline void fillHLine(int16_t x0, int16_t y0, int16_t x1) {
    fillRect(x0, y0, x1, y0);
  }

  inline void fillVLine(int16_t x0, int16_t y0, int16_t y1) {
    fillRect(x0, y0, x0, y1);
  }

  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    if (buffer_size_ == kRectWritingBufferSize) flush();
    x0_buffer_[buffer_size_] = x0;
    y0_buffer_[buffer_size_] = y0;
    x1_buffer_[buffer_size_] = x1;
    y1_buffer_[buffer_size_] = y1;
    ++buffer_size_;
  }

  ~BufferedRectFiller() { flush(); }

  void flush() {
    if (buffer_size_ == 0) return;
    device_.fillRects(blending_mode_, color_, x0_buffer_, y0_buffer_,
                      x1_buffer_, y1_buffer_, buffer_size_);
    buffer_size_ = 0;
  }

 private:
  DisplayOutput& device_;
  BlendingMode blending_mode_;
  Color color_;
  int16_t buffer_size_;
  int16_t x0_buffer_[kRectWritingBufferSize];
  int16_t y0_buffer_[kRectWritingBufferSize];
  int16_t x1_buffer_[kRectWritingBufferSize];
  int16_t y1_buffer_[kRectWritingBufferSize];
};

class ClippingBufferedRectFiller {
 public:
  ClippingBufferedRectFiller(DisplayOutput& device, Color color, Box clip_box,
                             BlendingMode blending_mode)
      : filler_(device, color, blending_mode), clip_box_(std::move(clip_box)) {}

  inline void fillPixel(int16_t x, int16_t y) { fillRect(x, y, x, y); }

  inline void fillHLine(int16_t x0, int16_t y0, int16_t x1) {
    fillRect(x0, y0, x1, y0);
  }

  inline void fillVLine(int16_t x0, int16_t y0, int16_t y1) {
    fillRect(x0, y0, x0, y1);
  }

  void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    if (x0 > clip_box_.xMax() || x1 < clip_box_.xMin() ||
        y0 > clip_box_.yMax() || y1 < clip_box_.yMin() || y1 < y0 || x1 < x0) {
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

  // void set_clip_box(Box clip_box) { clip_box_ = std::move(clip_box); }

 private:
  BufferedRectFiller filler_;
  Box clip_box_;
};

}  // namespace roo_display