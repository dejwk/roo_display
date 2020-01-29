#pragma once

#include "roo_display/core/streamable.h"

namespace roo_display {

namespace internal {

// Expands the 'rectangle size' of the specified stream, generating
// empty (fully transparent) pixels in the extended area surrounding the
// specified stream.
template <typename PixelStream>
class SuperRectangleStream {
 public:
  // width: the width of the surrounding rectangle
  // inner: the bounding box of the delegate stream in the surrounding rectangle
  SuperRectangleStream(std::unique_ptr<PixelStream> delegate, int16_t width,
                       const Box &inner)
      : delegate_(std::move(delegate)),
        count_(0),
        skip_(inner.xMin() + (int32_t)width * inner.yMin()),
        remaining_lines_(
            inner.empty() || inner.width() == width ? 0 : inner.height() - 1),
        width_(inner.empty()
                   ? 0
                   : (inner.width() == width ? (int32_t)width * inner.height()
                                             : inner.width())),
        width_skip_(width - inner.width()),
        transparency_(delegate_->transparency() != TRANSPARENCY_NONE
                          ? delegate_->transparency()
                          : width == inner.width() && inner.xMin() == 0 &&
                                    inner.yMin() == 0
                                ? TRANSPARENCY_NONE
                                : TRANSPARENCY_BINARY) {}

  SuperRectangleStream(SuperRectangleStream &&) = default;

  Color next() {
    if (skip_ > 0) {
      --skip_;
      return color::Transparent;
    }
    if (count_ < width_) {
      count_++;
      return delegate_->next();
    }
    if (remaining_lines_ == 0) {
      return color::Transparent;
    } else {
      count_ = 0;
      skip_ = width_skip_ - 1;
      --remaining_lines_;
      return color::Transparent;
    }
  }

  void skip(uint32_t count) {
    for (int i = 0; i < count; ++i) {
      next();
    }
  }

  TransparencyMode transparency() const { return transparency_; }

 private:
  SuperRectangleStream(const SuperRectangleStream &) = delete;

  std::unique_ptr<PixelStream> delegate_;
  int32_t count_;
  int32_t skip_;
  int16_t remaining_lines_;
  const int32_t width_;
  const int16_t width_skip_;
  TransparencyMode transparency_;
};

template <typename PixelStream>
std::unique_ptr<SuperRectangleStream<PixelStream>> Realign(
    const Box &outer_extents, const Box &inner_extents,
    std::unique_ptr<PixelStream> stream) {
  return std::unique_ptr<SuperRectangleStream<PixelStream>>(
      new SuperRectangleStream<PixelStream>(
          std::move(stream), outer_extents.width(),
          inner_extents.translate(-outer_extents.xMin(), -outer_extents.yMin())));
}

// The streams MUST be aligned first.
template <typename Bg, typename Fg>
class UnionStream {
 public:
  UnionStream(std::unique_ptr<Bg> bg, std::unique_ptr<Fg> fg)
      : bg_(std::move(bg)),
        fg_(std::move(fg)),
        transparency_(bg_->transparency() == TRANSPARENCY_NONE &&
                              fg_->transparency() == TRANSPARENCY_NONE
                          ? TRANSPARENCY_NONE
                          : bg_->transparency() == TRANSPARENCY_GRADUAL ||
                                    fg_->transparency() == TRANSPARENCY_GRADUAL
                                ? TRANSPARENCY_GRADUAL
                                : TRANSPARENCY_BINARY) {}

  UnionStream(UnionStream &&) = default;

  Color next() {
    Color bg = bg_->next();
    Color fg = fg_->next();
    return alphaBlend(bg, fg);
  }

  void skip(uint32_t count) {
    bg_->skip(count);
    fg_->skip(count);
  }

  TransparencyMode transparency() const { return transparency_; }

 private:
  UnionStream(const UnionStream &) = delete;

  std::unique_ptr<Bg> bg_;
  std::unique_ptr<Fg> fg_;
  TransparencyMode transparency_;
};

template <typename Bg, typename Fg>
std::unique_ptr<UnionStream<Bg, Fg>> MakeUnionStream(std::unique_ptr<Bg> bg,
                                                     std::unique_ptr<Fg> fg) {
  return std::unique_ptr<UnionStream<Bg, Fg>>(
      new UnionStream<Bg, Fg>(std::move(bg), std::move(fg)));
}

// Takes two streamables, and super-imposes one over another.
// Does not use any additional memory buffer.
template <typename Bg, typename Fg>
class Superposition {
 public:
  Superposition(Bg bg, int16_t bg_x, int16_t bg_y, Fg fg, int16_t fg_x,
                int16_t fg_y)
      : bg_(std::move(bg)),
        bg_x_(bg_x),
        bg_y_(bg_y),
        fg_(std::move(fg)),
        fg_x_(fg_x),
        fg_y_(fg_y),
        // bg_bounds_(bg_.bounds().translate(bg_x, bg_y)),
        // fg_bounds_(fg_.bounds().translate(fg_x, fg_y)),
        extents_(Box::extent(bg_extents(), fg_extents())) {}

  Superposition(Superposition &&) = default;

  auto CreateStream() const -> std::unique_ptr<
      internal::UnionStream<SuperRectangleStream<StreamTypeOf<Bg>>,
                            SuperRectangleStream<StreamTypeOf<Fg>>>> {
    return MakeUnionStream(
        Realign(extents_, bg_extents(), bg_.CreateStream()),
        Realign(extents_, fg_extents(), fg_.CreateStream()));
  }

  auto CreateClippedStream(const Box &clip_box) const -> std::unique_ptr<
      internal::UnionStream<SuperRectangleStream<ClipperedStreamTypeOf<Bg>>,
                            SuperRectangleStream<ClipperedStreamTypeOf<Fg>>>> {
    Box bounds = Box::intersect(extents_, clip_box);
    return MakeUnionStream(
        Realign(bounds, Box::intersect(bounds, bg_extents()),
                CreateClippedStreamFor(bg_, bounds.translate(-bg_x_, -bg_y_))),
        Realign(bounds, Box::intersect(bounds, fg_extents()),
                CreateClippedStreamFor(fg_, bounds.translate(-fg_x_, -fg_y_))));
  }

  const Box &extents() const { return extents_; }

 private:
  Superposition(const Superposition &) = delete;

  Bg bg_;
  int16_t bg_x_;
  int16_t bg_y_;
  Fg fg_;
  int16_t fg_x_;
  int16_t fg_y_;

  Box bg_extents() const { return bg_.extents().translate(bg_x_, bg_y_); }

  Box fg_extents() const { return fg_.extents().translate(fg_x_, fg_y_); }

  // Rect bg_bounds_;
  // Rect fg_bounds_;
  Box extents_;
};

template <typename Streamable>
class StreamableRef {
 public:
  StreamableRef(const Streamable &ref) : ref_(ref) {}
  const Box extents() const { return ref_.extents(); }
  decltype(std::declval<const Streamable &>().CreateStream()) CreateStream()
      const {
    return ref_.CreateStream();
  }

 private:
  const Streamable &ref_;
};

}  // namespace internal

template <typename Bg, typename Fg>
internal::Superposition<Bg, Fg> Overlay(Bg bg, int16_t bg_x, int16_t bg_y,
                                        Fg fg, int16_t fg_x, int16_t fg_y) {
  return internal::Superposition<Bg, Fg>(std::move(bg), bg_x, bg_y,
                                         std::move(fg), fg_x, fg_y);
}

// template <typename Content>
// internal::Superposition<StreamableFilledRect, Content>
// OverlayOnSolidRectangle(
//     const Rect &rect, Color bgcolor, Content content) {
//   return Overlay(StreamableFilledRect(rect.width(), rect.height(), bgcolor),
//                  rect.xMin(), rect.yMin(), std::move(content), 0, 0);
// }

template <typename Streamable>
internal::StreamableRef<Streamable> Ref(const Streamable &ref) {
  return internal::StreamableRef<Streamable>(ref);
}

}  // namespace roo_display