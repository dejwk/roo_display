#pragma once

#include "roo_display/core/box.h"
#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/color.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"

namespace roo_display {

// Drawable that can provide a color of any point within the extents, given the
// coordinates. Rasterizables can be used as overlays, backgrounds, and filters.
//
// Rasterizable is automatically Drawable and Streamable. All you need to do to
// implement it is to implement readColors(). That is, if your class has a
// way of generating a stream of pixels or drawing itself that is more efficient
// than going pixel-by-pixel, you may want to override those methods. Also, if
// your rasterizable possibly returns the same color for large areas, you may
// want to override readColorRect, as it can improve drawing performance when
// this rasterizable is used e.g. as an overlay or a background.
class Rasterizable : public virtual Streamable {
 public:
  // Read colors corresponding to the specified collection of points, and store
  // the results in the result array. The caller must ensure that the points are
  // within this rasterizable's bounds.
  virtual void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                          Color* result) const = 0;

  // Read colors corresponding to the specified collection of points, and store
  // the results in the result array. The points may be outside of this
  // rasterizable's bounds. This function calls readColors after isolating the
  // points that are within the bounds.
  void readColorsMaybeOutOfBounds(const int16_t* x, const int16_t* y,
                                  uint32_t count, Color* result) const;

  // Read colors corresponding to the specified rectangle. Returns true if all
  // colors are known to be the same. In this case, only the result[0] is
  // supposed to be read. Otherwise, the result array is filled with colors
  // corresponding to all the pixels corresponding to the rectangle. The caller
  // must ensure that the points are within this rasterizable's bounds.
  virtual bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                             int16_t yMax, Color* result) const;

  // Default implementation of createStream(), using readColors() to determine
  // colors of subsequent pixels.
  std::unique_ptr<PixelStream> createStream() const override;

  // Default implementation of createStream(), using readColors() to determine
  // colors of subsequent pixels.
  std::unique_ptr<PixelStream> createStream(const Box& bounds) const override;

 private:
  // Default implementation of drawTo(), using readColors() to determine
  // colors of subsequent pixels.
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

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    while (count-- > 0) {
      *result++ = getter_(*x++, *y++);
    }
  }

  TransparencyMode getTransparencyMode() const override {
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

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    for (uint32_t i = 0; i < count; ++i) {
      int16_t xs = (x[i] - extents_.xMin() - x_offset_) % extents_.width() +
                   extents_.xMin();
      int16_t ys = (y[i] - extents_.yMin() - y_offset_) % extents_.height() +
                   extents_.yMin();
      result[i] = getter_.get(xs, ys);
    }
  }

  TransparencyMode getTransparencyMode() const override {
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
