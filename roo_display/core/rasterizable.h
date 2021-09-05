#pragma once

#include "roo_display/core/box.h"
#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/color.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"
// #include "roo_display/core/streamable.h"

namespace roo_display {

class Rasterizable : public Streamable {
 public:
  virtual void ReadColors(const int16_t* x, const int16_t* y, uint32_t count,
                          Color* result) const = 0;
};

// namespace internal {

// template <typename Getter>
// class StreamableGetter {
//  public:
//   class Stream {
//    public:
//     Stream(Box box, Getter getter, TransparencyMode transparency)
//         : getter_(getter),
//           transparency_(transparency),
//           box_(box),
//           x_(box.xMin()),
//           y_(box.yMin()) {}

//     Color next() {
//       Color result = getter_(x_, y_);
//       if (x_ < box_.xMax()) {
//         x_++;
//       } else {
//         x_ = box_.xMin();
//         y_++;
//       }
//     }

//     void skip(uint32_t count) {
//       int16_t width = box_.width();
//       y_ += count / width;
//       x_ += count % width;
//       if (x_ > box_.xMax()) {
//         x_ = box_.xMin();
//         y_++;
//       }
//     }

//     TransparencyMode transparency() const { return transparency_; }

//    private:
//     Getter getter_;
//     TransparencyMode transparency_;
//     Box box_;
//     int16_t x_, y_;
//   };

//  public:
//   StreamableGetter(const Box& extents, Getter getter,
//                    TransparencyMode transparency)
//       : getter_(getter), transparency_(transparency), extents_(extents) {}

//   const Box& extents() const { return extents_; }

//   std::unique_ptr<Stream> CreateStream() const {
//     return std::unique_ptr<Stream>(
//         new Stream(extents_, getter_, transparency_));
//   }

//   std::unique_ptr<Stream> CreateClippedStream(const Box& clip_box) const {
//     return std::unique_ptr<Stream>(
//         new Stream(Box::intersect(extents_, clip_box), getter_,
//         transparency_));
//   }

//  private:
//   Getter getter_;
//   TransparencyMode transparency_;
//   Box extents_;
// };

// }  // namespace internal

// Takes a function object, overriding:
//
//   Color operator(int16_t x, int16_t y)
//
// and turns it into a rasterizable that can be used as a background.
template <typename Getter>
class SimpleRasterizable : public Rasterizable {
 public:
  SimpleRasterizable(Box extents, Getter getter, TransparencyMode transparency)
      : getter_(getter), extents_(extents), transparency_(transparency) {}

  Box extents() const override { return extents_; }

  void ReadColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    while (count-- > 0) {
      *result++ = getter_(*x++, *y++);
    }
  }

  // void drawTo(const Surface& s) const override {
  //   // Delegating to streamable ensures that we properly support backgrounds,
  //   // transparency, etc., and use optimized FillRectFromStream.
  //   streamToSurface(s, internal::StreamableGetter<Getter>(extents_, getter_,
  //                                                         transparency_));
  // }

  TransparencyMode transparency() const override { return transparency_; }

 private:
  Getter getter_;
  Box extents_;
  TransparencyMode transparency_;
};

template <typename Getter>
SimpleRasterizable<Getter> MakeRasterizable(Box extents, Getter getter,
                                            TransparencyMode transparency) {
  return SimpleRasterizable<Getter>(extents, getter, transparency);
}

// 'Infinite-size' rasterizable background, created by tiling the specified
// raster.
template <typename Getter>
class SimpleTiledRasterizable : public Rasterizable {
 public:
  SimpleTiledRasterizable(const Box& extents, Getter getter,
                          TransparencyMode transparency)
      : SimpleTiledRasterizable<Getter>(extents, getter, transparency, 0, 0) {}

  SimpleTiledRasterizable(const Box& extents, Getter getter,
                          TransparencyMode transparency, int16_t x_offset,
                          int16_t y_offset)
      : extents_(extents),
        getter_(getter),
        transparency_(transparency),
        x_offset_(x_offset),
        y_offset_(y_offset) {}

  Box extents() const override { return Box::MaximumBox(); }

  void ReadColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    for (uint32_t i = 0; i < count; ++i) {
      int16_t xs = (x[i] - extents_.xMin() - x_offset_) % extents_.width() +
                   extents_.xMin();
      int16_t ys = (y[i] - extents_.yMin() - y_offset_) % extents_.height() +
                   extents_.yMin();
      result[i] = getter_.get(xs, ys);
    }
  }

  TransparencyMode transparency() const override { return transparency_; }

 private:
  Box extents_;
  Getter getter_;
  TransparencyMode transparency_;
  const int16_t x_offset_, y_offset_;
};

template <typename Getter>
SimpleTiledRasterizable<Getter> MakeTiledRasterizable(
    Box extents, Getter getter, TransparencyMode transparency) {
  return SimpleTiledRasterizable<Getter>(extents, getter, transparency);
}

template <typename Raster>
SimpleTiledRasterizable<const Raster&> MakeTiledRaster(const Raster* raster) {
  return SimpleTiledRasterizable<const Raster&>(
      raster->extents(), *raster, raster->color_mode().transparency());
}

template <typename Raster>
SimpleTiledRasterizable<const Raster&> MakeTiledRaster(const Raster* raster,
                                                       int16_t x_offset,
                                                       int16_t y_offset) {
  return SimpleTiledRasterizable<const Raster&>(
      raster->extents(), *raster, raster->color_mode().transparency(), x_offset,
      y_offset);
}

}  // namespace roo_display
