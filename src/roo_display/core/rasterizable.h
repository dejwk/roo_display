#pragma once

#include "roo_display/color/color.h"
#include "roo_display/core/box.h"
#include "roo_display/core/buffered_drawing.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"

namespace roo_display {

/// Drawable that can provide a color for any point within its extents.
///
/// Rasterizables can be used as overlays, backgrounds, and filters. A
/// `Rasterizable` is also a `Streamable`. The minimum requirement is to
/// implement `readColors()`. If your class can stream pixels or draw more
/// efficiently than per-pixel access, override `createStream()` and/or
/// `drawTo()`. If large areas share the same color, override `readColorRect()`
/// to improve performance.
class Rasterizable : public virtual Streamable {
 public:
  /// Read colors for the given points.
  ///
  /// The caller must ensure all points are within bounds.
  virtual void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                          Color* result) const = 0;

  /// Read colors for points that may be out of bounds.
  ///
  /// Out-of-bounds points are set to `out_of_bounds_color`.
  void readColorsMaybeOutOfBounds(
      const int16_t* x, const int16_t* y, uint32_t count, Color* result,
      Color out_of_bounds_color = color::Transparent) const;

  /// Read colors for a rectangle.
  ///
  /// Returns true if all colors are identical (then only result[0] is valid).
  /// The caller must ensure the rectangle is within bounds.
  virtual bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                             int16_t yMax, Color* result) const;

  /// Default `createStream()` using `readColors()`.
  std::unique_ptr<PixelStream> createStream() const override;

  /// Default `createStream()` for a clipped box using `readColors()`.
  std::unique_ptr<PixelStream> createStream(const Box& bounds) const override;

 protected:
  /// Default `drawTo()` using `readColors()`.
  void drawTo(const Surface& s) const override;
};

/// Wrap a function object into a `Rasterizable`.
///
/// The getter must implement:
///
///   `Color operator()(int16_t x, int16_t y)`
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
/// Create a `SimpleRasterizable` from a getter function.
SimpleRasterizable<Getter> MakeRasterizable(
    Box extents, Getter getter, TransparencyMode transparency = kTransparency) {
  return SimpleRasterizable<Getter>(extents, getter, transparency);
}

/// "Infinite" rasterizable background by tiling a finite raster.
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
/// Create a tiled rasterizable from a getter function.
SimpleTiledRasterizable<Getter> MakeTiledRasterizable(
    Box extents, Getter getter, TransparencyMode transparency = kTransparency) {
  return SimpleTiledRasterizable<Getter>(extents, getter, transparency);
}

template <typename Raster>
/// Create a tiled rasterizable from a raster.
SimpleTiledRasterizable<const Raster&> MakeTiledRaster(const Raster* raster) {
  return SimpleTiledRasterizable<const Raster&>(
      raster->extents(), *raster, raster->color_mode().transparency());
}

template <typename Raster>
/// Create a tiled rasterizable from a raster with offsets.
SimpleTiledRasterizable<const Raster&> MakeTiledRaster(const Raster* raster,
                                                       int16_t x_offset,
                                                       int16_t y_offset) {
  return SimpleTiledRasterizable<const Raster&>(
      raster->extents(), *raster, raster->color_mode().transparency(), x_offset,
      y_offset);
}

}  // namespace roo_display
