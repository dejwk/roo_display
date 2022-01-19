#pragma once

#include "roo_display/core/box.h"
#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/color.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"

namespace roo_display {

class Rasterizable : public virtual Streamable {
 public:
  virtual void ReadColors(const int16_t* x, const int16_t* y, uint32_t count,
                          Color* result) const = 0;

  // Read colors corresponding to the specified rectangle. Returns true if all
  // colors are known to be the same. In this case, only the result[0] is
  // supposed to be read. Otherwise, the result array is filled with colors
  // corresponding to all the pixels corresponding to the rectangle.
  virtual bool ReadColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                             int16_t yMax, Color* result) const;

  std::unique_ptr<PixelStream> CreateStream() const override;

  std::unique_ptr<PixelStream> CreateStream(const Box& bounds) const override;

 private:
  void drawTo(const Surface& s) const override;
};

// Takes a function object, overriding:
//
//   Color operator(int16_t x, int16_t y)
//
// and turns it into a rasterizable.
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

  TransparencyMode GetTransparencyMode() const override {
    return transparency_;
  }

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

  TransparencyMode GetTransparencyMode() const override {
    return transparency_;
  }

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
