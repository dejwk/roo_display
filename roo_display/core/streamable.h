#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"

namespace roo_display {

// PixelStream template contract:
// class PixelStream {
//   typedef ptr_type;  // usually const uint8_t *PROGMEM
//   typedef ColorMode;
//   TransparencyType transparency() const;
//
//   PixelStream(const ptr_type data, ColorMode mode);
//   Color next();
// };

// PixelStreamable template contract:
// class PixelStreamable {
//   const Rect& extents() const;
//   std::unique_ptr<any> CreateStream() const;
// }

template <typename Streamable>
using StreamTypeOf =
    typename decltype(std::declval<Streamable>().CreateStream())::element_type;

template <typename Streamable>
using NativelyClippedStreamTypeOf =
    typename decltype(std::declval<Streamable>().CreateClippedStream(
        std::declval<Box>()))::element_type;

template <typename Streamable>
using ColorModeOf = decltype(std::declval<Streamable>().color_mode());

namespace internal {

template <typename PixelStream, TransparencyMode mode>
struct RectFiller;

template <typename PixelStream>
struct RectFiller<PixelStream, TRANSPARENCY_GRADUAL> {
  void operator()(DisplayOutput *output, const Box &extents,
                  PixelStream *stream) const {
    BufferedPixelWriter writer(output, PAINT_MODE_BLEND);
    for (int16_t j = extents.yMin(); j <= extents.yMax(); ++j) {
      for (int16_t i = extents.xMin(); i <= extents.xMax(); ++i) {
        writer.writePixel(i, j, stream->next());
      }
    }
  }
};

template <typename PixelStream>
struct RectFiller<PixelStream, TRANSPARENCY_BINARY> {
  void operator()(DisplayOutput *output, const Box &extents,
                  PixelStream *stream) const {
    BufferedPixelWriter writer(output);
    for (int16_t j = extents.yMin(); j <= extents.yMax(); ++j) {
      for (int16_t i = extents.xMin(); i <= extents.xMax(); ++i) {
        Color color = stream->next();
        if (color.a() == 0xFF) {
          writer.writePixel(i, j, color);
        }
      }
    }
  }
};

template <typename PixelStream>
struct RectFiller<PixelStream, TRANSPARENCY_NONE> {
  void operator()(DisplayOutput *output, const Box &extents,
                  PixelStream *stream) const {
    output->setAddress(extents, PAINT_MODE_REPLACE);
    int count = extents.area();
    BufferedColorWriter writer(output);
    while (count-- > 0) {
      writer.writeColor(stream->next());
    }
  }
};

template <typename PixelStream, TransparencyMode mode>
struct RectFillerWithBgColor {
  void operator()(DisplayOutput *output, const Box &extents, Color bgcolor,
                  PixelStream *stream) const {
    output->setAddress(extents, PAINT_MODE_REPLACE);
    int count = extents.area();
    BufferedColorWriter writer(output);
    while (count-- > 0) {
      Color color = stream->next();
      if (color.a() != 0xFF) {
        color = (mode == TRANSPARENCY_BINARY)
                    ? bgcolor
                    : alphaBlendOverOpaque(bgcolor, color);
      }
      writer.writeColor(color);
    }
  }
};

// Depending on the transparency mode of the passed stream, this method
// will fill in the specified rectangle by using either putPixel,
// paintPixel, or writeColor.
template <typename PixelStream>
void FillRectFromStream(DisplayOutput *output, const Box &extents,
                        PixelStream *stream, Color bgcolor) {
  switch (stream->transparency()) {
    case TRANSPARENCY_GRADUAL: {
      if (bgcolor.a() == 0) {
        RectFiller<PixelStream, TRANSPARENCY_GRADUAL> fill;
        fill(output, extents, stream);
      } else {
        RectFillerWithBgColor<PixelStream, TRANSPARENCY_GRADUAL> fill;
        fill(output, extents, bgcolor, stream);
      }
      break;
    }
    case TRANSPARENCY_BINARY: {
      if (bgcolor.a() == 0) {
        RectFiller<PixelStream, TRANSPARENCY_BINARY> fill;
        fill(output, extents, stream);
      } else {
        RectFillerWithBgColor<PixelStream, TRANSPARENCY_BINARY> fill;
        fill(output, extents, bgcolor, stream);
      }
      break;
    }
    case TRANSPARENCY_NONE: {
      RectFiller<PixelStream, TRANSPARENCY_NONE> fill;
      fill(output, extents, stream);
      break;
    }
  }
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
template <typename PixelStream>
class SubRectangleStream {
 public:
  SubRectangleStream(std::unique_ptr<PixelStream> delegate, int16_t width,
                     int16_t width_skip)
      : delegate_(std::move(delegate)),
        x_(0),
        width_(width),
        width_skip_(width_skip) {}

  SubRectangleStream(SubRectangleStream &&) = default;

  Color next() {
    if (x_ >= width_) {
      delegate_->skip(width_skip_);
      x_ = 0;
    }
    Color result = delegate_->next();
    ++x_;
    return result;
  }

  void skip(uint32_t count) {
    // TODO: optimize
    for (int i = 0; i < count; i++) next();
  }

  TransparencyMode transparency() const { return delegate_->transparency(); }

 private:
  SubRectangleStream(const SubRectangleStream &) = delete;

  std::unique_ptr<PixelStream> delegate_;
  int16_t x_;
  const int16_t width_;
  const int16_t width_skip_;
};

template <typename PixelStream>
std::unique_ptr<SubRectangleStream<PixelStream>> SubRectangle(
    std::unique_ptr<PixelStream> delegate, int16_t width, int16_t width_skip) {
  return std::unique_ptr<SubRectangleStream<PixelStream>>(
      new SubRectangleStream<PixelStream>(std::move(delegate), width,
                                          width_skip));
}

template <typename Streamable, typename Stream = StreamTypeOf<Streamable>,
          typename = void>
struct Clipper {
  std::unique_ptr<SubRectangleStream<Stream>> operator()(
      const Streamable &streamable, const Box &clip_box) const {
    if (clip_box.contains(streamable.extents())) {
      // Optimized case: nothing has been clipped; rendering full stream.
      return internal::SubRectangle(streamable.CreateStream(),
                                    streamable.extents().area(), 0);
    } else {
      // Non-optimized case: rendering sub-rectangle. Need to go line-by-line.
      Box bounds = Box::intersect(streamable.extents(), clip_box);
      int line_offset = streamable.extents().width() - bounds.width();
      int xoffset = bounds.xMin() - streamable.extents().xMin();
      int yoffset = bounds.yMin() - streamable.extents().yMin();
      std::unique_ptr<Stream> stream = streamable.CreateStream();
      stream->skip(yoffset * streamable.extents().width() + xoffset);
      return internal::SubRectangle(std::move(stream), bounds.width(),
                                    line_offset);
    }
  }
};

template <typename Streamable>
struct Clipper<Streamable, StreamTypeOf<Streamable>,
               decltype(std::declval<Streamable>().CreateClippedStream(
                            std::declval<Box>()),
                        void())> {
  std::unique_ptr<NativelyClippedStreamTypeOf<Streamable>> operator()(
      const Streamable &streamable, const Box &clip_box) const {
    return streamable.CreateClippedStream(clip_box);
  }
};

}  // namespace internal

template <typename Streamable>
using ClipperedStreamTypeOf =
    typename decltype(internal::Clipper<Streamable>().operator()(
        std::declval<Streamable>(), std::declval<Box>()))::element_type;

template <typename Streamable>
std::unique_ptr<ClipperedStreamTypeOf<Streamable>> CreateClippedStreamFor(
    const Streamable &streamable, const Box &clip_box) {
  internal::Clipper<Streamable> clip;
  return clip(streamable, clip_box);
}

// Color-filled rectangle, useful as a background for super-imposed images.
class StreamableFilledRect {
 public:
  class Stream {
   public:
    Stream(Color color) : color_(color) {}
    Color next() { return color_; }
    void skip(uint32_t count) {}

    TransparencyMode transparency() const { return TRANSPARENCY_NONE; }

   private:
    Color color_;
  };

  StreamableFilledRect(Box extents, Color color)
      : extents_(std::move(extents)), color_(color) {}

  StreamableFilledRect(uint16_t width, uint16_t height, Color color)
      : extents_(0, 0, width - 1, height - 1), color_(color) {}

  const Box &extents() const { return extents_; }
  std::unique_ptr<Stream> CreateStream() const {
    return std::unique_ptr<Stream>(new Stream(color_));
  }
  std::unique_ptr<Stream> CreateClippedStream(const Box &clip_box) const {
    // No need to clip anything, since the stream is a uniform color.
    return std::unique_ptr<Stream>(new Stream(color_));
  }

 private:
  Box extents_;
  Color color_;
};

template <typename Streamable>
class DrawableStreamable : public Drawable {
 public:
  DrawableStreamable(Streamable streamable)
      : streamable_(std::move(streamable)) {}

  DrawableStreamable(DrawableStreamable &&other)
      : streamable_(std::move(other.streamable_)) {}

  Box extents() const { return streamable_.extents(); }

 private:
  void drawTo(const Surface &s) const override {
    Box bounds =
        Box::intersect(s.clip_box, streamable_.extents().translate(s.dx, s.dy));
    if (bounds.empty()) return;
    if (streamable_.extents().width() == bounds.width() &&
        streamable_.extents().height() == bounds.height()) {
      // Optimized case: rendering full stream.
      auto stream = streamable_.CreateStream();
      internal::FillRectFromStream(s.out, bounds, stream.get(), s.bgcolor);
    } else {
      auto stream =
          CreateClippedStreamFor(streamable_, bounds.translate(-s.dx, -s.dy));
      internal::FillRectFromStream(s.out, bounds, stream.get(), s.bgcolor);
    }
  }

  Streamable streamable_;
};

template <typename Streamable>
DrawableStreamable<Streamable> MakeDrawableStreamable(Streamable streamable) {
  return DrawableStreamable<Streamable>(std::move(streamable));
}

template <typename Streamable>
Drawable *MakeNewDrawableStreamable(Streamable streamable) {
  return new DrawableStreamable<Streamable>(std::move(streamable));
}

template <typename Streamable, typename Stream = StreamTypeOf<Streamable>>
void streamToSurface(const Surface &s, Streamable streamable) {
  s.drawObject(MakeDrawableStreamable(std::move(streamable)));
}

// Convenience wrapper for image classes that are read from a byte stream
// (as opposed to e.g. generated on the fly).
template <typename Resource, typename ColorMode, typename StreamType>
class SimpleStreamable {
 public:
  SimpleStreamable(int16_t width, int16_t height, Resource resource,
                   const ColorMode &color_mode = ColorMode())
      : SimpleStreamable(Box(0, 0, width - 1, height - 1), std::move(resource),
                         std::move(color_mode)) {}

  SimpleStreamable(Box extents, Resource resource,
                   const ColorMode &color_mode = ColorMode())
      : extents_(std::move(extents)),
        resource_(std::move(resource)),
        color_mode_(color_mode) {}

  void setColorMode(const ColorMode &color_mode) { color_mode_ = color_mode; }

  const Box &extents() const { return extents_; }
  const ColorMode &color_mode() const { return color_mode_; }

  std::unique_ptr<StreamType> CreateStream() const {
    return std::unique_ptr<StreamType>(new StreamType(resource_, color_mode_));
  }

 private:
  Box extents_;
  Resource resource_;
  ColorMode color_mode_;
};

// Similar to SimpleStreamable, but additionally allows to clip the stream
// to a specified viewport. Useful for image classes like XBitmap, with
// alignment to full byte or word boundaries.
template <typename Streamable,
          typename Stream = decltype(std::declval<Streamable>().CreateStream())>
class Clipping {
 public:
  template <typename... Args>
  Clipping(const Box &clip_box, Args &&... args)
      : streamable_(std::forward<Args>(args)...),
        extents_(Box::intersect(streamable_.extents(), clip_box)) {}

  Clipping(const Box &clip_box, Streamable streamable)
      : streamable_(std::move(streamable)),
        extents_(Box::intersect(streamable_.extents(), clip_box)) {}

  const Box &extents() const { return extents_; }

  ColorModeOf<Streamable> color_mode() const {
    return streamable_.color_mode();
  }

  std::unique_ptr<ClipperedStreamTypeOf<Streamable>> CreateStream() const {
    return CreateClippedStreamFor(streamable_, extents_);
  }

  std::unique_ptr<ClipperedStreamTypeOf<Streamable>> CreateClippedStream(
      const Box &clip_box) const {
    return CreateClippedStreamFor(streamable_,
                                  Box::intersect(clip_box, extents_));
  }

 private:
  const Streamable streamable_;
  Box extents_;
};

template <typename Streamable>
Clipping<Streamable> Clipped(const Box &clip_box, Streamable streamable) {
  return Clipping<Streamable>(clip_box, std::move(streamable));
}

}  // namespace roo_display
