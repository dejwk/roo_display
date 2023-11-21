#pragma once

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"

namespace roo_display {

// RawPixelStream template contract:
// class RawPixelStream {
//   typedef ptr_type;  // usually const uint8_t *PROGMEM
//   typedef ColorMode;
//   TransparencyMode transparency() const;
//
//   RawPixelStream(const ptr_type data, ColorMode mode);
//   Color next();
// };

// RawPixelStreamable template contract:
// class RawPixelStreamable {
//   const Box& extents() const;
//   std::unique_ptr<any> createRawStream() const;
// }

template <typename RawStreamable>
using RawStreamTypeOf = typename decltype(std::declval<const RawStreamable>()
                                              .createRawStream())::element_type;

template <typename RawStreamable>
using NativelyClippedRawStreamTypeOf =
    typename decltype(std::declval<const RawStreamable>()
                          .CreateClippedRawStream(
                              std::declval<Box>()))::element_type;

template <typename RawStreamable>
using ColorModeOf = decltype(std::declval<const RawStreamable>().color_mode());

namespace internal {

template <typename RawPixelStream>
struct RectFillerVisible {
  void operator()(DisplayOutput &output, const Box &extents, Color bgcolor,
                  RawPixelStream *stream, BlendingMode mode,
                  TransparencyMode transparency_mode) const {
    BufferedPixelWriter writer(output, mode);
    if (bgcolor.a() == 0) {
      for (int16_t j = extents.yMin(); j <= extents.yMax(); ++j) {
        for (int16_t i = extents.xMin(); i <= extents.xMax(); ++i) {
          Color color = stream->next();
          if (color.a() != 0) {
            writer.writePixel(i, j, color);
          }
        }
      }
    } else if (transparency_mode == TRANSPARENCY_GRADUAL) {
      for (int16_t j = extents.yMin(); j <= extents.yMax(); ++j) {
        for (int16_t i = extents.xMin(); i <= extents.xMax(); ++i) {
          Color color = stream->next();
          if (color.a() != 0) {
            writer.writePixel(i, j, AlphaBlend(bgcolor, color));
          }
        }
      }
    } else {
      for (int16_t j = extents.yMin(); j <= extents.yMax(); ++j) {
        for (int16_t i = extents.xMin(); i <= extents.xMax(); ++i) {
          Color color = stream->next();
          if (color.a() != 0) {
            writer.writePixel(i, j, color);
          }
        }
      }
    }
  }
};

template <typename RawPixelStream>
struct RectFillerRectangle {
  void operator()(DisplayOutput &output, const Box &extents, Color bgcolor,
                  RawPixelStream *stream, BlendingMode blending_mode,
                  TransparencyMode transparency_mode) const {
    output.setAddress(extents, blending_mode);
    int count = extents.area();
    BufferedColorWriter writer(output);
    if (bgcolor.a() == 0) {
      while (count-- > 0) {
        writer.writeColor(stream->next());
      }
    } else if (transparency_mode == TRANSPARENCY_GRADUAL) {
      if (bgcolor.a() == 0xFF) {
        while (count-- > 0) {
          writer.writeColor(AlphaBlendOverOpaque(bgcolor, stream->next()));
        }
      } else {
        while (count-- > 0) {
          writer.writeColor(AlphaBlend(bgcolor, stream->next()));
        }
      }
    } else {
      while (count-- > 0) {
        Color color = stream->next();
        writer.writeColor(color.a() == 0 ? bgcolor : color);
      }
    }
  }
};

// This function will fill in the specified rectangle using the most appropriate
// method given the stream's transparency mode.
template <typename RawPixelStream>
void FillRectFromRawStream(DisplayOutput &output, const Box &extents,
                           RawPixelStream *stream, Color bgcolor,
                           FillMode fill_mode, BlendingMode blending_mode) {
  if (stream->transparency() == TRANSPARENCY_NONE &&
      (blending_mode == BLENDING_MODE_SOURCE_OVER ||
       blending_mode == BLENDING_MODE_SOURCE_OVER_OPAQUE)) {
    // The stream will overwrite the entire rectangle anyway.
    blending_mode = BLENDING_MODE_SOURCE;
    bgcolor = color::Transparent;
  }
  if (fill_mode == FILL_MODE_RECTANGLE) {
    RectFillerRectangle<RawPixelStream> fill;
    fill(output, extents, bgcolor, stream, blending_mode,
         stream->transparency());
  } else {
    RectFillerVisible<RawPixelStream> fill;
    fill(output, extents, bgcolor, stream, blending_mode,
         stream->transparency());
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
template <typename RawPixelStream>
class SubRectangleRawStream {
 public:
  SubRectangleRawStream(std::unique_ptr<RawPixelStream> delegate, int16_t width,
                        int16_t width_skip)
      : delegate_(std::move(delegate)),
        x_(0),
        width_(width),
        width_skip_(width_skip) {}

  SubRectangleRawStream(SubRectangleRawStream &&) = default;

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
  SubRectangleRawStream(const SubRectangleRawStream &) = delete;

  std::unique_ptr<RawPixelStream> delegate_;
  int16_t x_;
  const int16_t width_;
  const int16_t width_skip_;
};

template <typename RawPixelStream>
std::unique_ptr<SubRectangleRawStream<RawPixelStream>> SubRectangle(
    std::unique_ptr<RawPixelStream> delegate, int16_t width,
    int16_t width_skip) {
  return std::unique_ptr<SubRectangleRawStream<RawPixelStream>>(
      new SubRectangleRawStream<RawPixelStream>(std::move(delegate), width,
                                                width_skip));
}

template <typename RawStreamable,
          typename RawStream = RawStreamTypeOf<RawStreamable>, typename = void>
struct Clipper {
  std::unique_ptr<SubRectangleRawStream<RawStream>> operator()(
      const RawStreamable &streamable, const Box &clip_box) const {
    if (clip_box.contains(streamable.extents())) {
      // Optimized case: nothing has been clipped; rendering full stream.
      return internal::SubRectangle(streamable.createRawStream(),
                                    streamable.extents().area(), 0);
    } else {
      // Non-optimized case: rendering sub-rectangle. Need to go line-by-line.
      Box bounds = Box::Intersect(streamable.extents(), clip_box);
      int line_offset = streamable.extents().width() - bounds.width();
      int xoffset = bounds.xMin() - streamable.extents().xMin();
      int yoffset = bounds.yMin() - streamable.extents().yMin();
      std::unique_ptr<RawStream> stream = streamable.createRawStream();
      stream->skip(yoffset * streamable.extents().width() + xoffset);
      return internal::SubRectangle(std::move(stream), bounds.width(),
                                    line_offset);
    }
  }
};

template <typename RawStreamable>
struct Clipper<RawStreamable, RawStreamTypeOf<RawStreamable>,
               decltype(std::declval<RawStreamable>().CreateClippedRawStream(
                            std::declval<Box>()),
                        void())> {
  std::unique_ptr<NativelyClippedRawStreamTypeOf<RawStreamable>> operator()(
      const RawStreamable &streamable, const Box &clip_box) const {
    return streamable.CreateClippedRawStream(clip_box);
  }
};

}  // namespace internal

template <typename RawStreamable>
using ClipperedRawStreamTypeOf =
    typename decltype(internal::Clipper<RawStreamable>().operator()(
        std::declval<RawStreamable>(), std::declval<Box>()))::element_type;

template <typename RawStreamable>
std::unique_ptr<ClipperedRawStreamTypeOf<RawStreamable>>
CreateClippedRawStreamFor(const RawStreamable &streamable,
                          const Box &clip_box) {
  internal::Clipper<RawStreamable> clip;
  return clip(streamable, clip_box);
}

// Color-filled rectangle, useful as a background for super-imposed images.
class RawStreamableFilledRect {
 public:
  class RawStream {
   public:
    RawStream(Color color) : color_(color) {}
    Color next() { return color_; }
    void skip(uint32_t count) {}

    TransparencyMode transparency() const {
      return color_.isOpaque() ? TRANSPARENCY_NONE
             : color_.a() == 0 ? TRANSPARENCY_BINARY
                               : TRANSPARENCY_GRADUAL;
    }

   private:
    Color color_;
  };

  RawStreamableFilledRect(Box extents, Color color)
      : extents_(std::move(extents)), color_(color) {}

  RawStreamableFilledRect(uint16_t width, uint16_t height, Color color)
      : extents_(0, 0, width - 1, height - 1), color_(color) {}

  const Box &extents() const { return extents_; }
  const Box &anchorExtents() const { return extents_; }
  std::unique_ptr<RawStream> createRawStream() const {
    return std::unique_ptr<RawStream>(new RawStream(color_));
  }
  std::unique_ptr<RawStream> CreateClippedRawStream(const Box &clip_box) const {
    // No need to clip anything, since the stream is a uniform color.
    return std::unique_ptr<RawStream>(new RawStream(color_));
  }

 private:
  Box extents_;
  Color color_;
};

template <typename RawStreamable>
class DrawableRawStreamable : public Streamable {
 public:
  DrawableRawStreamable(RawStreamable streamable)
      : streamable_(std::move(streamable)) {}

  DrawableRawStreamable(DrawableRawStreamable &&other)
      : streamable_(std::move(other.streamable_)) {}

  Box extents() const override { return streamable_.extents(); }

  Box anchorExtents() const override { return streamable_.anchorExtents(); }

  const RawStreamable &contents() const { return streamable_; }

  std::unique_ptr<PixelStream> createStream() const override {
    return std::unique_ptr<PixelStream>(
        new Stream<RawStreamTypeOf<RawStreamable>>(
            streamable_.createRawStream()));
  }

  std::unique_ptr<PixelStream> createStream(const Box &bounds) const override {
    return std::unique_ptr<PixelStream>(
        new Stream<ClipperedRawStreamTypeOf<RawStreamable>>(
            CreateClippedRawStreamFor(streamable_, bounds)));
  }

  decltype(std::declval<RawStreamable>().createRawStream()) createRawStream()
      const {
    return streamable_.createRawStream();
  }

 private:
  template <typename RawStream>
  class Stream : public PixelStream {
   public:
    Stream(std::unique_ptr<RawStream> raw) : raw_(std::move(raw)) {}

    void Read(Color *buf, uint16_t count) override {
      while (count-- > 0) *buf++ = raw_->next();
    }

   private:
    std::unique_ptr<RawStream> raw_;
  };

  void drawTo(const Surface &s) const override {
    Box bounds = Box::Intersect(
        s.clip_box(), streamable_.extents().translate(s.dx(), s.dy()));
    if (bounds.empty()) return;
    if (streamable_.extents().width() == bounds.width() &&
        streamable_.extents().height() == bounds.height()) {
      // Optimized case: rendering full stream.
      auto stream = streamable_.createRawStream();
      internal::FillRectFromRawStream(s.out(), bounds, stream.get(),
                                      s.bgcolor(), s.fill_mode(),
                                      s.blending_mode());
    } else {
      auto stream = CreateClippedRawStreamFor(
          streamable_, bounds.translate(-s.dx(), -s.dy()));
      internal::FillRectFromRawStream(s.out(), bounds, stream.get(),
                                      s.bgcolor(), s.fill_mode(),
                                      s.blending_mode());
    }
  }

  RawStreamable streamable_;
};

template <typename RawStreamable>
DrawableRawStreamable<RawStreamable> MakeDrawableRawStreamable(
    RawStreamable streamable) {
  return DrawableRawStreamable<RawStreamable>(std::move(streamable));
}

template <typename RawStreamable>
Drawable *MakeNewDrawableRawStreamable(RawStreamable streamable) {
  return new DrawableRawStreamable<RawStreamable>(std::move(streamable));
}

template <typename RawStreamable,
          typename RawStream = RawStreamTypeOf<RawStreamable>>
void streamToSurface(const Surface &s, RawStreamable streamable) {
  s.drawObject(MakeDrawableRawStreamable(std::move(streamable)));
}

// Convenience wrapper for image classes that are read from a byte stream
// (as opposed to e.g. generated on the fly).
template <typename Resource, typename ColorMode, typename RawStreamType>
class SimpleRawStreamable {
 public:
  SimpleRawStreamable(int16_t width, int16_t height, Resource resource,
                      const ColorMode &color_mode = ColorMode())
      : SimpleRawStreamable(Box(0, 0, width - 1, height - 1),
                            Box(0, 0, width - 1, height - 1),
                            std::move(resource), std::move(color_mode)) {}

  SimpleRawStreamable(Box extents, Box anchor_extents, Resource resource,
                      const ColorMode &color_mode = ColorMode())
      : extents_(std::move(extents)),
        anchor_extents_(anchor_extents),
        resource_(std::move(resource)),
        color_mode_(color_mode) {}

  void setColorMode(const ColorMode &color_mode) { color_mode_ = color_mode; }

  const Box &extents() const { return extents_; }
  const Box &anchorExtents() const { return anchor_extents_; }
  const Resource &resource() const { return resource_; }
  const ColorMode &color_mode() const { return color_mode_; }

  std::unique_ptr<RawStreamType> createRawStream() const {
    return std::unique_ptr<RawStreamType>(
        new RawStreamType(resource_, color_mode_));
  }

 private:
  Box extents_;
  Box anchor_extents_;
  Resource resource_;
  ColorMode color_mode_;
};

// Similar to SimpleRawStreamable, but additionally allows to clip the stream
// to a specified viewport. Useful for image classes like XBitmap, with
// alignment to full byte or word boundaries.
template <typename RawStreamable,
          typename RawStream =
              decltype(std::declval<RawStreamable>().createRawStream())>
class Clipping {
 public:
  template <typename... Args>
  Clipping(const Box &clip_box, Args &&...args)
      : streamable_(std::forward<Args>(args)...),
        extents_(Box::Intersect(streamable_.extents(), clip_box)) {}

  Clipping(const Box &clip_box, RawStreamable streamable)
      : streamable_(std::move(streamable)),
        extents_(Box::Intersect(streamable_.extents(), clip_box)) {}

  const Box &extents() const { return extents_; }
  Box anchorExtents() const { return streamable_.anchorExtents(); }

  const ColorModeOf<RawStreamable> &color_mode() const {
    return streamable_.color_mode();
  }

  std::unique_ptr<ClipperedRawStreamTypeOf<RawStreamable>> createRawStream()
      const {
    return CreateClippedRawStreamFor(streamable_, extents_);
  }

  std::unique_ptr<ClipperedRawStreamTypeOf<RawStreamable>>
  CreateClippedRawStream(const Box &clip_box) const {
    return CreateClippedRawStreamFor(streamable_,
                                     Box::Intersect(clip_box, extents_));
  }

 private:
  const RawStreamable streamable_;
  Box extents_;
};

template <typename RawStreamable>
Clipping<RawStreamable> Clipped(const Box &clip_box, RawStreamable streamable) {
  return Clipping<RawStreamable>(clip_box, std::move(streamable));
}

}  // namespace roo_display
