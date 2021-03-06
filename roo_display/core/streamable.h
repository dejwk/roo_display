#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"

namespace roo_display {

class PixelStream {
 public:
  virtual void Read(Color *buf, uint16_t size, PaintMode mode) = 0;

  virtual void Skip(uint32_t count) {
    Color buf[kPixelWritingBufferSize];
    while (count > kPixelWritingBufferSize) {
      Read(buf, kPixelWritingBufferSize, PAINT_MODE_REPLACE);
      count -= kPixelWritingBufferSize;
    }
    Read(buf, count, PAINT_MODE_REPLACE);
  }

  virtual ~PixelStream() {}
};

namespace internal {

inline void fillReplaceRect(DisplayOutput *output, const Box &extents,
                            PixelStream *stream, PaintMode mode) {
  Color buf[kPixelWritingBufferSize];
  output->setAddress(extents, mode);
  uint32_t count = extents.area();
  while (count > 0) {
    uint32_t n = count;
    if (n > kPixelWritingBufferSize) n = kPixelWritingBufferSize;
    stream->Read(buf, n, PAINT_MODE_REPLACE);
    output->write(buf, n);
    count -= n;
  }
}

inline void fillPaintRectOverOpaqueBg(DisplayOutput *output, const Box &extents,
                                      Color bgColor, PixelStream *stream,
                                      PaintMode mode) {
  Color buf[kPixelWritingBufferSize];
  output->setAddress(extents, mode);
  uint32_t count = extents.area();
  do {
    uint16_t n = kPixelWritingBufferSize;
    if (n > count) n = count;
    stream->Read(buf, n, PAINT_MODE_REPLACE);
    for (int i = 0; i < n; i++) buf[i] = alphaBlendOverOpaque(bgColor, buf[i]);
    output->write(buf, n);
    count -= n;
  } while (count > 0);
}

inline void fillPaintRectOverBg(DisplayOutput *output, const Box &extents,
                                Color bgcolor, PixelStream *stream,
                                PaintMode mode) {
  Color buf[kPixelWritingBufferSize];
  output->setAddress(extents, mode);
  uint32_t count = extents.area();
  do {
    uint16_t n = kPixelWritingBufferSize;
    if (n > count) n = count;
    Color::Fill(buf, n, bgcolor);
    stream->Read(buf, n, PAINT_MODE_BLEND);
    output->write(buf, n);
    count -= n;
  } while (count > 0);
}

// Assumes no bgcolor.
inline void writeRectVisible(DisplayOutput *output, const Box &extents,
                             PixelStream *stream, PaintMode mode) {
  // TODO(dawidk): need to optimize this.
  Color buf[kPixelWritingBufferSize];
  BufferedPixelWriter writer(output, mode);
  uint32_t remaining = extents.area();
  int idx = kPixelWritingBufferSize;
  for (int16_t j = extents.yMin(); j <= extents.yMax(); ++j) {
    for (int16_t i = extents.xMin(); i <= extents.xMax(); ++i) {
      if (idx >= kPixelWritingBufferSize) {
        idx = 0;
        int n = remaining < kPixelWritingBufferSize ? remaining
                                                    : kPixelWritingBufferSize;
        stream->Read(buf, n, PAINT_MODE_REPLACE);
      }
      Color color = buf[idx++];
      if (color.a() != 0) {
        writer.writePixel(i, j, color);
      }
    }
  }
}

inline void writeRectVisibleOverOpaqueBg(DisplayOutput *output,
                                         const Box &extents, Color bgcolor,
                                         PixelStream *stream, PaintMode mode) {
  // TODO(dawidk): need to optimize this.
  Color buf[kPixelWritingBufferSize];
  BufferedPixelWriter writer(output, mode);
  uint32_t remaining = extents.area();
  int idx = kPixelWritingBufferSize;
  for (int16_t j = extents.yMin(); j <= extents.yMax(); ++j) {
    for (int16_t i = extents.xMin(); i <= extents.xMax(); ++i) {
      if (idx >= kPixelWritingBufferSize) {
        idx = 0;
        int n = remaining < kPixelWritingBufferSize ? remaining
                                                    : kPixelWritingBufferSize;
        stream->Read(buf, n, PAINT_MODE_REPLACE);
      }
      Color color = buf[idx++];
      if (color.a() != 0) {
        writer.writePixel(i, j, alphaBlendOverOpaque(bgcolor, color));
      }
    }
  }
}

inline void writeRectVisibleOverBg(DisplayOutput *output, const Box &extents,
                                   Color bgcolor, PixelStream *stream,
                                   PaintMode mode) {
  // TODO(dawidk): need to optimize this.
  Color buf[kPixelWritingBufferSize];
  BufferedPixelWriter writer(output, mode);
  uint32_t remaining = extents.area();
  int idx = kPixelWritingBufferSize;
  for (int16_t j = extents.yMin(); j <= extents.yMax(); ++j) {
    for (int16_t i = extents.xMin(); i <= extents.xMax(); ++i) {
      if (idx >= kPixelWritingBufferSize) {
        idx = 0;
        int n = remaining < kPixelWritingBufferSize ? remaining
                                                    : kPixelWritingBufferSize;
        stream->Read(buf, n, PAINT_MODE_REPLACE);
      }
      Color color = buf[idx++];
      if (color.a() != 0) {
        writer.writePixel(i, j, alphaBlend(bgcolor, color));
      }
    }
  }
}

// This function will fill in the specified rectangle using the most appropriate
// method given the stream's transparency mode.
inline void FillRectFromStream(DisplayOutput *output, const Box &extents,
                               PixelStream *stream, Color bgcolor,
                               FillMode fill_mode, PaintMode mode,
                               TransparencyMode transparency) {
  if (fill_mode == FILL_MODE_RECTANGLE || transparency == TRANSPARENCY_NONE) {
    if (bgcolor.a() == 0 || transparency == TRANSPARENCY_NONE) {
      fillReplaceRect(output, extents, stream, mode);
    } else if (bgcolor.a() == 0xFF) {
      fillPaintRectOverOpaqueBg(output, extents, bgcolor, stream, mode);
    } else {
      fillPaintRectOverBg(output, extents, bgcolor, stream, mode);
    }
  } else {
    if (bgcolor.a() == 0) {
      writeRectVisible(output, extents, stream, mode);
    } else if (bgcolor.a() == 0xFF) {
      writeRectVisibleOverOpaqueBg(output, extents, bgcolor, stream, mode);
    } else {
      writeRectVisibleOverBg(output, extents, bgcolor, stream, mode);
    }
  }
};

class BufferingStream {
 public:
  BufferingStream(std::unique_ptr<PixelStream> stream, uint32_t count)
      : stream_(std::move(stream)),
        remaining_(count),
        idx_(kPixelWritingBufferSize) {}

  Color next() {
    if (idx_ >= kPixelWritingBufferSize) {
      idx_ = 0;
      uint32_t n = kPixelWritingBufferSize;
      if (n > remaining_) n = remaining_;
      stream_->Read(buf_, n, PAINT_MODE_REPLACE);
    }
    --remaining_;
    return buf_[idx_++];
  }

  // void read(Color* buf, uint16_t count) {
  //   while (count-- > 0) {
  //     *buf++ = next();
  //   }
  // }

  // void blend(Color* buf, uint16_t count) {
  //   while (count-- > 0) {
  //     *buf = alphaBlend(*buf, next());
  //     buf++;
  //   }
  //   return;
  // }

  void read(Color *buf, uint16_t count) {
    if (idx_ >= kPixelWritingBufferSize) {
      idx_ = 0;
      uint32_t n = kPixelWritingBufferSize;
      if (n > remaining_) n = remaining_;
      stream_->Read(buf_, n, PAINT_MODE_REPLACE);
    }
    const Color *in = buf_ + idx_;
    uint16_t batch = kPixelWritingBufferSize - idx_;
    while (true) {
      if (count <= batch) {
        idx_ += count;
        memcpy(buf, in, count * sizeof(Color));
        return;
      }
      memcpy(buf, in, batch * sizeof(Color));
      count -= batch;
      buf += batch;
      idx_ = 0;
      in = buf_;
      uint32_t n = kPixelWritingBufferSize;
      if (n > remaining_) n = remaining_;
      stream_->Read(buf_, n, PAINT_MODE_REPLACE);
      batch = n;
    }
  }

  void blend(Color *buf, uint16_t count) {
    if (idx_ >= kPixelWritingBufferSize) {
      idx_ = 0;
      uint32_t n = kPixelWritingBufferSize;
      if (n > remaining_) n = remaining_;
      stream_->Read(buf_, n, PAINT_MODE_REPLACE);
    }
    const Color *in = buf_ + idx_;
    uint16_t batch = kPixelWritingBufferSize - idx_;
    while (true) {
      if (count <= batch) {
        idx_ += count;
        while (count-- > 0) {
          *buf = alphaBlend(*buf, *in);
          buf++;
          in++;
        }
        return;
      }
      count -= batch;
      while (batch-- > 0) {
        *buf = alphaBlend(*buf, *in);
        buf++;
        in++;
      }
      idx_ = 0;
      in = buf_;
      uint32_t n = kPixelWritingBufferSize;
      if (n > remaining_) n = remaining_;
      stream_->Read(buf_, n, PAINT_MODE_REPLACE);
      batch = n;
    }
  }

  void skip(uint32_t count) {
    uint16_t buffered = kPixelWritingBufferSize - idx_;
    if (count < buffered) {
      idx_ += count;
      remaining_ -= count;
      return;
    }
    count -= buffered;
    remaining_ -= buffered;
    idx_ = 0;
    do {
      uint16_t n = kPixelWritingBufferSize;
      if (n > remaining_) n = remaining_;
      stream_->Read(buf_, n, PAINT_MODE_REPLACE);
      if (count < n) {
        idx_ = count;
        remaining_ -= count;
        return;
      }
      remaining_ -= n;
    } while (remaining_ > 0);
  }

 private:
  std::unique_ptr<PixelStream> stream_;
  uint32_t remaining_;
  Color buf_[kPixelWritingBufferSize];
  int idx_;
};

// Restricts the stream to the specified sub-rectangle. More specifically,
// since streams don't know their rectangle dimension, it:
// * assumes that the delegate stream has been already skipped to the right
//   starting place, so that the first next() call would return the pixel
//   corresponding to the top-left corner of the sub-rectanble
// * then, returns the specified number of colored pixels, and then skips
//   by specified amount (to get to the next 'line') in the delegate stream.
//   The caller should guarantee that the sum of these two quantities
//   (width and width_skip) is equal to the pixel width of the image represented
//   by the underlying stream.
class SubRectangleStream : public PixelStream {
 public:
  SubRectangleStream(std::unique_ptr<PixelStream> delegate, uint32_t count,
                     int16_t width, int16_t width_skip)
      : stream_(std::move(delegate)),
        x_(0),
        width_(width),
        width_skip_(width_skip),
        remaining_(count),
        idx_(kPixelWritingBufferSize) {}

  SubRectangleStream(SubRectangleStream &&) = default;

  void Read(Color *buf, uint16_t count, PaintMode mode) override {
    do {
      if (x_ >= width_) {
        dskip(width_skip_);
        x_ = 0;
      }
      uint16_t n = width_ - x_;
      if (n > count) n = count;
      uint16_t read = dnext(buf, n, mode);
      buf += read;
      count -= read;
      x_ += read;
    } while (count > 0);
  }

  // void skip(uint32_t count) {
  //   // TODO: optimize
  //   for (int i = 0; i < count; i++) next();
  // }

 private:
  // Color next() {
  //   if (x_ >= width_) {
  //     dskip(width_skip_);
  //     x_ = 0;
  //   }
  //   Color result = dnext();
  //   ++x_;
  //   return result;
  // }

  void dskip(uint32_t count) {
    uint16_t buffered = kPixelWritingBufferSize - idx_;
    if (count <= buffered) {
      idx_ += count;
      return;
    }
    count -= buffered;
    idx_ = 0;
    do {
      uint16_t n = kPixelWritingBufferSize;
      if (n > remaining_) n = remaining_;
      stream_->Read(buf_, n, PAINT_MODE_REPLACE);
      remaining_ -= n;
      if (count < n) {
        idx_ = count;
        return;
      }
      remaining_ -= n;
      count -= n;
    } while (count > 0);
  }

  uint16_t dnext(Color *buf, int16_t count, PaintMode mode) {
    if (idx_ >= kPixelWritingBufferSize) {
      uint16_t n = kPixelWritingBufferSize;
      if (n > remaining_) n = remaining_;
      stream_->Read(buf_, n, PAINT_MODE_REPLACE);
      idx_ = 0;
      remaining_ -= n;
    }
    if (count > kPixelWritingBufferSize - idx_) {
      count = kPixelWritingBufferSize - idx_;
    }
    if (mode == PAINT_MODE_REPLACE) {
      memcpy(buf, buf_ + idx_, count * sizeof(Color));
    } else {
      for (uint16_t i = 0; i < count; ++i) {
        buf[i] = alphaBlend(buf[i], buf_[idx_ + i]);
      }
    }
    idx_ += count;
    return count;
  }

  SubRectangleStream(const SubRectangleStream &) = delete;

  std::unique_ptr<PixelStream> stream_;
  int16_t x_;
  const int16_t width_;
  const int16_t width_skip_;

  uint32_t remaining_;
  Color buf_[kPixelWritingBufferSize];
  int idx_;
};

inline std::unique_ptr<PixelStream> SubRectangle(
    std::unique_ptr<PixelStream> delegate, uint32_t count, int16_t width,
    int16_t width_skip) {
  return std::unique_ptr<PixelStream>(
      new SubRectangleStream(std::move(delegate), count, width, width_skip));
}

}  // namespace internal

class Streamable : public Drawable {
 public:
  Box extents() const override { return extents_; }

  // CreateStream creates the stream of pixels that should be drawn to
  // the extents box.
  virtual std::unique_ptr<PixelStream> CreateStream() const = 0;

  // GetTransparencyMode is an optimization hint that a streamable can
  // give to the renderer. The default, safe to use, is TRANSPARENCY_GRADUAL,
  // putting no restrictions on the pixels generated. But if this streamable
  // knows that all pixels in the stream will be fully opaque
  // (TRANSPARENCY_NONE) or have 1-bit alpha (TRANSPARENCY_BINARY), by saying
  // so, it may allow the renderer to use more efficient blending algorithm.
  virtual TransparencyMode GetTransparencyMode() const {
    return TRANSPARENCY_GRADUAL;
  }

  void drawTo(const Surface &s) const override {
    Box bounds =
        Box::intersect(s.clip_box(), extents().translate(s.dx(), s.dy()));
    if (bounds.empty()) return;
    std::unique_ptr<PixelStream> stream;
    if (extents().width() == bounds.width() &&
        extents().height() == bounds.height()) {
      // Optimized case: rendering full stream.
      stream = CreateStream();
      internal::FillRectFromStream(s.out(), bounds, stream.get(), s.bgcolor(),
                                   s.fill_mode(), s.paint_mode(),
                                   GetTransparencyMode());
    } else {
      // Non-optimized case: rendering sub-rectangle. Need to go line-by-line.
      int line_offset = extents_.width() - bounds.width();
      int xoffset = bounds.xMin() - extents_.xMin() - s.dx();
      int yoffset = bounds.yMin() - extents_.yMin() - s.dy();
      std::unique_ptr<PixelStream> stream = CreateStream();
      uint32_t skipped = yoffset * extents_.width() + xoffset;
      stream->Skip(skipped);
      internal::SubRectangleStream sub(std::move(stream),
                                       extents_.area() - skipped,
                                       bounds.width(), line_offset);
      internal::FillRectFromStream(s.out(), bounds, &sub, s.bgcolor(),
                                   s.fill_mode(), s.paint_mode(),
                                   GetTransparencyMode());
    }
  }

 protected:
  Streamable(Box extents) : extents_(std::move(extents)) {}

 private:
  Box extents_;
};

// Color-filled rectangle, useful as a background for super-imposed images.
class StreamableFilledRect : public Streamable {
 public:
  class Stream : public PixelStream {
   public:
    Stream(Color color) : color_(color) {}

    void Read(Color *buf, uint16_t count, PaintMode mode) override {
      if (mode == PAINT_MODE_REPLACE || color_.a() == 0xFF) {
        Color::Fill(buf, count, color_);
      } else {
        while (count-- > 0) {
          *buf = alphaBlend(*buf, color_);
          ++buf;
        }
      }
    }

    void skip(uint32_t count) {}

   private:
    Color color_;
  };

  StreamableFilledRect(Box extents, Color color)
      : Streamable(std::move(extents)), color_(color) {}

  StreamableFilledRect(uint16_t width, uint16_t height, Color color)
      : Streamable(Box(0, 0, width - 1, height - 1)), color_(color) {}

  std::unique_ptr<PixelStream> CreateStream() const {
    return std::unique_ptr<PixelStream>(new Stream(color_));
  }

  TransparencyMode GetTransparencyMode() const override {
    return color_.a() == 0xFF ? TRANSPARENCY_NONE : TRANSPARENCY_GRADUAL;
  }

 private:
  Color color_;
};

// Convenience wrapper for image classes that are read from a byte stream
// (as opposed to e.g. generated on the fly).
template <typename Resource, typename ColorMode, typename StreamType>
class SimpleStreamable : public Streamable {
 public:
  SimpleStreamable(int16_t width, int16_t height, Resource resource,
                   const ColorMode &color_mode = ColorMode())
      : SimpleStreamable(Box(0, 0, width - 1, height - 1), std::move(resource),
                         std::move(color_mode)) {}

  SimpleStreamable(Box extents, Resource resource,
                   const ColorMode &color_mode = ColorMode())
      : Streamable(std::move(extents)),
        resource_(std::move(resource)),
        color_mode_(color_mode) {}

  void setColorMode(const ColorMode &color_mode) { color_mode_ = color_mode; }

  const Resource &resource() const { return resource_; }
  const ColorMode &color_mode() const { return color_mode_; }
  ColorMode &color_mode() { return color_mode_; }

  std::unique_ptr<PixelStream> CreateStream() const override {
    return std::unique_ptr<PixelStream>(new StreamType(resource_, color_mode_));
  }

  std::unique_ptr<StreamType> CreateRawStream() const {
    return std::unique_ptr<StreamType>(new StreamType(resource_, color_mode_));
  }

  TransparencyMode GetTransparencyMode() const override {
    return color_mode_.transparency();
  }

 private:
  Resource resource_;
  ColorMode color_mode_;
};

}  // namespace roo_display
