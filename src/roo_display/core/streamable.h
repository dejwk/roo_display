#pragma once

#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"

namespace roo_display {

/// Stream of pixels in row-major order.
class PixelStream {
 public:
  /// Read up to `size` pixels into `buf`.
  virtual void Read(Color* buf, uint16_t size) = 0;

  /// Skip `count` pixels.
  virtual void Skip(uint32_t count) {
    Color buf[kPixelWritingBufferSize];
    while (count > kPixelWritingBufferSize) {
      Read(buf, kPixelWritingBufferSize);
      count -= kPixelWritingBufferSize;
    }
    Read(buf, count);
  }

  virtual ~PixelStream() {}
};

namespace internal {

inline void fillReplaceRect(DisplayOutput& output, const Box& extents,
                            PixelStream* stream, BlendingMode mode) {
  Color buf[kPixelWritingBufferSize];
  output.setAddress(extents, mode);
  uint32_t count = extents.area();
  while (count > 0) {
    uint32_t n = count;
    if (n > kPixelWritingBufferSize) n = kPixelWritingBufferSize;
    stream->Read(buf, n);
    output.write(buf, n);
    count -= n;
  }
}

inline void fillPaintRectOverOpaqueBg(DisplayOutput& output, const Box& extents,
                                      Color bgColor, PixelStream* stream,
                                      BlendingMode mode) {
  Color buf[kPixelWritingBufferSize];
  output.setAddress(extents, mode);
  uint32_t count = extents.area();
  do {
    uint16_t n = kPixelWritingBufferSize;
    if (n > count) n = count;
    stream->Read(buf, n);
    for (int i = 0; i < n; i++) buf[i] = AlphaBlendOverOpaque(bgColor, buf[i]);
    output.write(buf, n);
    count -= n;
  } while (count > 0);
}

inline void fillPaintRectOverBg(DisplayOutput& output, const Box& extents,
                                Color bgcolor, PixelStream* stream,
                                BlendingMode mode) {
  Color buf[kPixelWritingBufferSize];
  output.setAddress(extents, mode);
  uint32_t count = extents.area();
  do {
    uint16_t n = kPixelWritingBufferSize;
    if (n > count) n = count;
    stream->Read(buf, n);
    for (int i = 0; i < n; ++i) {
      buf[i] = AlphaBlend(bgcolor, buf[i]);
    }
    output.write(buf, n);
    count -= n;
  } while (count > 0);
}

// Assumes no bgcolor.
inline void writeRectVisible(DisplayOutput& output, const Box& extents,
                             PixelStream* stream, BlendingMode mode) {
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
        stream->Read(buf, n);
        remaining -= n;
      }
      Color color = buf[idx++];
      if (color.a() != 0) {
        writer.writePixel(i, j, color);
      }
    }
  }
}

inline void writeRectVisibleOverOpaqueBg(DisplayOutput& output,
                                         const Box& extents, Color bgcolor,
                                         PixelStream* stream,
                                         BlendingMode mode) {
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
        stream->Read(buf, n);
      }
      Color color = buf[idx++];
      if (color.a() != 0) {
        writer.writePixel(i, j, AlphaBlendOverOpaque(bgcolor, color));
      }
    }
  }
}

inline void writeRectVisibleOverBg(DisplayOutput& output, const Box& extents,
                                   Color bgcolor, PixelStream* stream,
                                   BlendingMode mode) {
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
        stream->Read(buf, n);
      }
      Color color = buf[idx++];
      if (color.a() != 0) {
        writer.writePixel(i, j, AlphaBlend(bgcolor, color));
      }
    }
  }
}

// This function will fill in the specified rectangle using the most appropriate
// method given the stream's transparency mode.
inline void FillRectFromStream(DisplayOutput& output, const Box& extents,
                               PixelStream* stream, Color bgcolor,
                               FillMode fill_mode, BlendingMode blending_mode,
                               TransparencyMode transparency) {
  if (fill_mode == kFillRectangle || transparency == kNoTransparency) {
    if (bgcolor.a() == 0 || transparency == kNoTransparency) {
      fillReplaceRect(output, extents, stream, blending_mode);
    } else if (bgcolor.a() == 0xFF) {
      fillPaintRectOverOpaqueBg(output, extents, bgcolor, stream,
                                blending_mode);
    } else {
      fillPaintRectOverBg(output, extents, bgcolor, stream, blending_mode);
    }
  } else {
    if (bgcolor.a() == 0) {
      writeRectVisible(output, extents, stream, blending_mode);
    } else if (bgcolor.a() == 0xFF) {
      writeRectVisibleOverOpaqueBg(output, extents, bgcolor, stream,
                                   blending_mode);
    } else {
      writeRectVisibleOverBg(output, extents, bgcolor, stream, blending_mode);
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
      fetch();
    }
    return buf_[idx_++];
  }

  // void read(Color* buf, uint16_t count) {
  //   while (count-- > 0) {
  //     *buf++ = next();
  //   }
  // }

  // void blend(Color* buf, uint16_t count) {
  //   while (count-- > 0) {
  //     *buf = AlphaBlend(*buf, next());
  //     buf++;
  //   }
  //   return;
  // }

  void read(Color* buf, uint16_t count) {
    if (idx_ >= kPixelWritingBufferSize) {
      idx_ = 0;
      fetch();
    }
    const Color* in = buf_ + idx_;
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
      batch = fetch();
    }
  }

  void blend(Color* buf, uint16_t count, BlendingMode blending_mode) {
    if (idx_ >= kPixelWritingBufferSize) {
      idx_ = 0;
      fetch();
    }
    const Color* in = buf_ + idx_;
    uint16_t batch = kPixelWritingBufferSize - idx_;
    while (true) {
      if (count <= batch) {
        idx_ += count;
        ApplyBlendingInPlace(blending_mode, buf, in, count);
        return;
      }
      count -= batch;
      ApplyBlendingInPlace(blending_mode, buf, in, batch);
      buf += batch;
      idx_ = 0;
      in = buf_;
      batch = fetch();
    }
  }

  void skip(uint32_t count) {
    uint16_t buffered = kPixelWritingBufferSize - idx_;
    if (count < buffered) {
      idx_ += count;
      return;
    }
    count -= buffered;
    idx_ = 0;
    do {
      uint16_t n = fetch();
      if (count < n) {
        idx_ = count;
        return;
      }
      count -= n;
    } while (remaining_ > 0);
  }

 private:
  uint16_t fetch() {
    uint16_t n = kPixelWritingBufferSize;
    if (n > remaining_) n = remaining_;
    stream_->Read(buf_, n);
    remaining_ -= n;
    return n;
  }

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
template <typename Delegate>
class SubRectangleStream : public PixelStream {
 public:
  SubRectangleStream(Delegate delegate, uint32_t count, int16_t width,
                     int16_t width_skip)
      : stream_(std::move(delegate)),
        x_(0),
        width_(width),
        width_skip_(width_skip),
        remaining_(count),
        idx_(kPixelWritingBufferSize) {}

  SubRectangleStream(SubRectangleStream&&) = default;

  void Read(Color* buf, uint16_t count) override {
    do {
      if (x_ >= width_) {
        dskip(width_skip_);
        x_ = 0;
      }
      uint16_t n = width_ - x_;
      if (n > count) n = count;
      uint16_t read = dnext(buf, n);
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
    idx_ = kPixelWritingBufferSize;
    if (count >= kPixelWritingBufferSize / 2) {
      stream_.Skip(count);
      remaining_ -= count;
      return;
    }
    uint16_t n = kPixelWritingBufferSize;
    if (n > remaining_) n = remaining_;
    stream_.Read(buf_, n);
    remaining_ -= n;
    idx_ = count;
  }

  uint16_t dnext(Color* buf, int16_t count) {
    if (idx_ >= kPixelWritingBufferSize) {
      uint16_t n = kPixelWritingBufferSize;
      if (n > remaining_) n = remaining_;
      stream_.Read(buf_, n);
      idx_ = 0;
      remaining_ -= n;
    }
    if (count > kPixelWritingBufferSize - idx_) {
      count = kPixelWritingBufferSize - idx_;
    }
    memcpy(buf, buf_ + idx_, count * sizeof(Color));
    idx_ += count;
    return count;
  }

  SubRectangleStream(const SubRectangleStream&) = delete;

  Delegate stream_;
  int16_t x_;
  const int16_t width_;
  const int16_t width_skip_;

  uint32_t remaining_;
  Color buf_[kPixelWritingBufferSize];
  int idx_;
};

template <typename Stream>
inline SubRectangleStream<Stream> MakeSubRectangle(Stream stream,
                                                   const Box& extents,
                                                   const Box& bounds) {
  int line_offset = extents.width() - bounds.width();
  int xoffset = bounds.xMin() - extents.xMin();
  int yoffset = bounds.yMin() - extents.yMin();
  uint32_t skipped = yoffset * extents.width() + xoffset;
  stream.Skip(skipped);
  return SubRectangleStream<Stream>(std::move(stream), extents.area() - skipped,
                                    bounds.width(), line_offset);
}

}  // namespace internal

template <typename Stream>
/// Create a pixel stream over a sub-rectangle of a larger stream.
inline std::unique_ptr<PixelStream> SubRectangle(Stream stream,
                                                 const Box& extents,
                                                 const Box& bounds) {
  int line_offset = extents.width() - bounds.width();
  int xoffset = bounds.xMin() - extents.xMin();
  int yoffset = bounds.yMin() - extents.yMin();
  uint32_t skipped = yoffset * extents.width() + xoffset;
  stream.Skip(skipped);
  return std::unique_ptr<PixelStream>(new internal::SubRectangleStream<Stream>(
      std::move(stream), extents.area() - skipped, bounds.width(),
      line_offset));
}

/// Drawable that can provide a sequential pixel stream.
///
/// Streamables allow the renderer to efficiently draw clipped rectangles and
/// blend pixel data without per-pixel virtual calls.  This is commonly used by
/// images and offscreen buffers, and it is also the base for Rasterizable.
class Streamable : public virtual Drawable {
 public:
  /// Create a stream covering the full `extents()`.
  virtual std::unique_ptr<PixelStream> createStream() const = 0;

  /// Create a stream for the given clipped bounds.
  virtual std::unique_ptr<PixelStream> createStream(
      const Box& clip_box) const = 0;

  /// Return the transparency mode for pixels in this stream.
  ///
  /// This is an optimization hint. The default is `kTransparency`, which
  /// is always safe. If pixels are guaranteed fully opaque or 1-bit alpha,
  /// return `kNoTransparency` or `kCrudeTransparency` to enable faster
  /// blending paths.
  virtual TransparencyMode getTransparencyMode() const { return kTransparency; }

 private:
  void drawTo(const Surface& s) const override {
    Box ext = extents();
    Box bounds = Box::Intersect(s.clip_box().translate(-s.dx(), -s.dy()), ext);
    if (bounds.empty()) return;
    std::unique_ptr<PixelStream> stream;
    if (ext.width() == bounds.width() && ext.height() == bounds.height()) {
      stream = createStream();
    } else {
      stream = createStream(bounds);
    }
    internal::FillRectFromStream(s.out(), bounds.translate(s.dx(), s.dy()),
                                 stream.get(), s.bgcolor(), s.fill_mode(),
                                 s.blending_mode(), getTransparencyMode());
  }
};

/// Convenience wrapper for images backed by a byte stream.
template <typename Iterable, typename ColorMode, typename StreamType>
class SimpleStreamable : public Streamable {
 public:
  /// Construct from width/height and a resource.
  SimpleStreamable(int16_t width, int16_t height, Iterable resource,
                   const ColorMode& color_mode = ColorMode())
      : SimpleStreamable(Box(0, 0, width - 1, height - 1), std::move(resource),
                         std::move(color_mode)) {}

  /// Construct from extents and a resource.
  SimpleStreamable(Box extents, Iterable resource,
                   const ColorMode& color_mode = ColorMode())
      : SimpleStreamable(extents, extents, std::move(resource), color_mode) {}

  /// Construct from extents, anchor extents, and a resource.
  SimpleStreamable(Box extents, Box anchor_extents, Iterable resource,
                   const ColorMode& color_mode = ColorMode())
      : extents_(std::move(extents)),
        anchor_extents_(anchor_extents),
        resource_(std::move(resource)),
        color_mode_(color_mode) {}

  /// Set the color mode.
  void setColorMode(const ColorMode& color_mode) { color_mode_ = color_mode; }

  /// Return extents of the image.
  Box extents() const override { return extents_; }

  /// Return anchor extents used for alignment.
  Box anchorExtents() const override { return anchor_extents_; }

  /// Access underlying resource.
  const Iterable& resource() const { return resource_; }
  /// Access color mode (const).
  const ColorMode& color_mode() const { return color_mode_; }
  /// Access color mode (mutable).
  ColorMode& color_mode() { return color_mode_; }

  /// Create a pixel stream for the full extents.
  std::unique_ptr<PixelStream> createStream() const override {
    return std::unique_ptr<PixelStream>(
        new StreamType(resource_.iterator(), color_mode_));
  }

  /// Create a pixel stream for a clipped box.
  std::unique_ptr<PixelStream> createStream(const Box& bounds) const override {
    return SubRectangle(StreamType(resource_.iterator(), color_mode_),
                        extents(), bounds);
  }

  /// Create the raw stream type.
  std::unique_ptr<StreamType> createRawStream() const {
    return std::unique_ptr<StreamType>(
        new StreamType(resource_.iterator(), color_mode_));
  }

  /// Return transparency mode derived from color mode.
  TransparencyMode getTransparencyMode() const override {
    return color_mode_.transparency();
  }

 private:
  Box extents_;
  Box anchor_extents_;

  Iterable resource_;
  ColorMode color_mode_;
};

}  // namespace roo_display
