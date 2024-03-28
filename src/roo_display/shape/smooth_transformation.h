#pragma once

#include <cmath>

#include "roo_display/core/box.h"
#include "roo_display/core/raster.h"
#include "roo_display/core/rasterizable.h"
#include "roo_display/shape/point.h"

namespace roo_display {

// Transformation classes describe various types of transformations.
// Translation, Scaling, and Rotation are primitive transformations.
// AffineTransformation is a combination of translations, scalings, and
// rotations. Use 'then()' (described below) to compose transformations.

// All transformation classes have some common methods:
// * apply(FpPoint): calculate the transformation for a given point.
// * then(TransformationType t): composes t with this transformation, returning
//   a new composed transformation.
// * inversion(): returns a transformation that is the inverse of this
//   transformation.
// * transformExtents(Box): calculates a bouding box that encapsulates all
//   transformed pixels from the specified box.

// Avoid declaring transformations explicitly; prefer auto and the construction
// methods defined below.

class IdentityTransformation;
class Translation;
class Scaling;
class Rotation;
class AffineTransformation;

// Returns a translation by the specified vector.
Translation Translate(float dx, float dy);

// Returns scaling relative to the origin (0, 0) by the specified magnitude in
// the x and y direction.
Scaling Scale(float sx, float sy);

// Returns scaling relative to the specified center point by the specified
// magnitude in the x and y direction.
AffineTransformation ScaleAbout(float sx, float sy, const FpPoint& center);

// Returns a rotation about the origin (0, 0) by the specified angle, clockwise.
Rotation RotateRight(float angle);

// Returns a rotation about the origin (0, 0) clockwise by 90 degrees.
Rotation RotateRight();

// Returns a rotation about the specified point by the specified angle,
// clockwise.
AffineTransformation RotateRightAbout(float angle, const FpPoint& center);

// Returns a rotation about the specified point clockwise by 90 degrees.
AffineTransformation RotateRightAbout(const FpPoint& center);

// Returns a rotation about the origin (0, 0) by the specified angle,
// counter-clockwise.
Rotation RotateLeft(float angle);

// Returns a rotation about the origin (0, 0) counter-clockwise by 90 degrees.
Rotation RotateLeft();

// Returns a rotation about the specified point counter-clockwise by 90 degrees.
AffineTransformation RotateLeftAbout(float angle, const FpPoint& center);

// Returns a shear, rooted at the origin.
AffineTransformation Shear(float sx, float sy);

// Returns a shear, rooted at the specified point.
AffineTransformation Shear(float sx, float sy, const FpPoint& base);

// Returns a horizontal shear of the specified magnitude, rooted at the origin.
AffineTransformation ShearHorizontally(float sx);

// Returns a horizontal shear of the specified magnitude, rooted at the
// specified y coordinate.
AffineTransformation ShearHorizontallyAbout(float sx, float base_y);

// Returns a vertical shear of the specified magnitude, rooted at the origin.
AffineTransformation ShearVertically(float sy);

// Returns a horizontal shear of the specified magnitude, rooted at the
// specified x coordinate.
AffineTransformation ShearVerticallyAbout(float sy, float base_x);

template <typename RasterType, typename TransformationType>
class TransformedRaster;

// Returns a rasterizable representation of the specified raster, transformed
// using the specified transformation.
template <typename RasterType, typename TransformationType>
TransformedRaster<RasterType, TransformationType> TransformRaster(
    RasterType& original, TransformationType transformation);

// Implementation details.

class IdentityTransformation {
 public:
  IdentityTransformation() {}
  FpPoint apply(FpPoint p) const { return p; }

  IdentityTransformation then(IdentityTransformation t) const { return *this; }
  Translation then(Translation t) const;
  Scaling then(Scaling t) const;
  Rotation then(Rotation t) const;
  AffineTransformation then(AffineTransformation t) const;

  IdentityTransformation inversion() const { return *this; }

  Box transformExtents(Box extents) const { return extents; }
};

class Translation {
 public:
  Translation(float dx, float dy) : dx_(dx), dy_(dy) {}

  FpPoint apply(FpPoint p) const { return FpPoint{p.x + dx_, p.y + dy_}; }

  float dx() const { return dx_; }
  float dy() const { return dy_; }

  Translation then(IdentityTransformation t) const { return *this; }
  Translation then(Translation t) const;
  AffineTransformation then(Scaling t) const;
  AffineTransformation then(Rotation t) const;
  AffineTransformation then(AffineTransformation t) const;

  Translation inversion() const { return Translation(-dx_, -dy_); }

  Box transformExtents(Box extents) const {
    return Box((int16_t)floorf(extents.xMin() + dx_),
               (int16_t)floorf(extents.yMin() + dy_),
               (int16_t)ceilf(extents.xMax() + dx_),
               (int16_t)ceilf(extents.yMax() + dy_));
  }

 private:
  float dx_, dy_;
};

class Scaling {
 public:
  Scaling(float sx, float sy) : sx_(sx), sy_(sy) {}

  FpPoint apply(FpPoint p) const { return FpPoint{p.x * sx_, p.y * sy_}; }

  float sx() const { return sx_; }
  float sy() const { return sy_; }

  Scaling then(IdentityTransformation t) const { return *this; }
  AffineTransformation then(Translation t) const;
  Scaling then(Scaling t) const;
  AffineTransformation then(Rotation t) const;
  AffineTransformation then(AffineTransformation t) const;

  Scaling inversion() const { return Scaling(1.0f / sx_, 1.0f / sy_); }

  Box transformExtents(Box extents) const {
    float xMin = sx_ * extents.xMin();
    float yMin = sy_ * extents.yMin();
    float xMax = sx_ * extents.xMax();
    float yMax = sy_ * extents.yMax();
    if (xMin > xMax) std::swap(xMin, xMax);
    if (yMin > yMax) std::swap(yMin, yMax);
    return Box((int16_t)floorf(xMin), (int16_t)floorf(yMin),
               (int16_t)ceilf(xMax), (int16_t)ceilf(yMax));
  }

 private:
  float sx_, sy_;
};

class Rotation {
 public:
  Rotation(float theta)
      : theta_(theta), sin_theta_(sinf(theta)), cos_theta_(cosf(theta)) {}

  FpPoint apply(FpPoint p) const {
    return FpPoint{p.x * cos_theta_ - p.y * sin_theta_,
                   p.x * sin_theta_ + p.y * cos_theta_};
  }

  float theta() const { return theta_; }
  float sin_theta() const { return sin_theta_; }
  float cos_theta() const { return cos_theta_; }

  Rotation then(IdentityTransformation t) const { return *this; }
  AffineTransformation then(Translation t) const;
  AffineTransformation then(Scaling t) const;
  Rotation then(Rotation t) const;
  AffineTransformation then(AffineTransformation t) const;

  Rotation inversion() const { return Rotation(-theta_); }

  Box transformExtents(Box extents) const;

 private:
  float theta_, sin_theta_, cos_theta_;
};

class AffineTransformation {
 public:
  AffineTransformation(float a11, float a12, float a21, float a22, float tx,
                       float ty)
      : a11_(a11), a12_(a12), a21_(a21), a22_(a22), tx_(tx), ty_(ty) {}

  AffineTransformation(IdentityTransformation t)
      : a11_(1.0), a12_(0), a21_(0), a22_(1.0), tx_(0), ty_(0) {}

  AffineTransformation(Translation t)
      : a11_(1.0), a12_(0), a21_(0), a22_(1.0), tx_(t.dx()), ty_(t.dy()) {}

  AffineTransformation(Scaling t)
      : a11_(t.sx()), a12_(0), a21_(0), a22_(t.sy()), tx_(0), ty_(0) {}

  AffineTransformation(Rotation t)
      : a11_(t.cos_theta()),
        a12_(-t.sin_theta()),
        a21_(t.sin_theta()),
        a22_(t.cos_theta()),
        tx_(0),
        ty_(0) {}

  FpPoint apply(FpPoint p) const {
    return FpPoint{p.x * a11_ + p.y * a12_ + tx_,
                   p.x * a21_ + p.y * a22_ + ty_};
  }

  float a11() const { return a11_; }
  float a12() const { return a12_; }
  float a21() const { return a21_; }
  float a22() const { return a22_; }
  float tx() const { return tx_; }
  float ty() const { return ty_; }

  AffineTransformation then(IdentityTransformation t) const { return *this; }
  AffineTransformation then(Translation t) const;
  AffineTransformation then(Scaling t) const;
  AffineTransformation then(Rotation t) const;
  AffineTransformation then(AffineTransformation t) const;

  AffineTransformation inversion() const {
    float inv_det = 1.0f / (a11_ * a22_ - a12_ * a21_);
    float inv_a11 = a22_ * inv_det;
    float inv_a12 = -a12_ * inv_det;
    float inv_a21 = -a21_ * inv_det;
    float inv_a22 = a11_ * inv_det;

    return AffineTransformation(inv_a11, inv_a12, inv_a21, inv_a22,
                                -inv_a11 * tx_ - inv_a12 * ty_,
                                -inv_a21 * tx_ - inv_a22 * ty_);
  }

  Box transformExtents(Box extents) const;

 private:
  float a11_, a12_, a21_, a22_;
  float tx_, ty_;
};

// Uses bi-linear interpolation.
template <typename RasterType, typename TransformationType>
class TransformedRaster : public Rasterizable {
 public:
  TransformedRaster(RasterType& original, TransformationType transformation)
      : original_(original),
        inverse_transformation_(transformation.inversion()),
        extents_(transformation.transformExtents(original.extents())),
        anchor_extents_(
            transformation.transformExtents(original.anchorExtents())) {}

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    Box orig_extents = original_.extents();
    if (orig_extents.empty()) {
      for (uint32_t i = 0; i < count; ++i) {
        result[i] = color::Transparent;
      }
      return;
    }
    typename RasterType::Reader reader;
    const auto ptr = original_.buffer();
    uint16_t w = orig_extents.width();
    RawColorInterpolator<typename RasterType::ColorMode> interpolator;
    for (uint32_t i = 0; i < count; ++i) {
      FpPoint orig = inverse_transformation_.apply(FpPoint{x[i], y[i]});
      int16_t xMin = (int16_t)floorf(orig.x);
      int16_t yMin = (int16_t)floorf(orig.y);
      int16_t xMax = (int16_t)ceilf(orig.x);
      int16_t yMax = (int16_t)ceilf(orig.y);
      if (xMax < orig_extents.xMin() || xMin > orig_extents.xMax() ||
          yMax < orig_extents.yMin() || yMin > orig_extents.yMax()) {
        result[i] = color::Transparent;
        continue;
      }
      int32_t offset =
          xMin - orig_extents.xMin() + (yMin - orig_extents.yMin()) * w;
      if (xMin == xMax && yMin == yMax) {
        result[i] = original_.color_mode().toArgbColor(reader(ptr, offset));
        continue;
      }
      if (xMin == xMax) {
        int16_t interpolation_fraction = (int16_t)(256 * (orig.y - yMin));
        if (yMin < orig_extents.yMin()) {
          result[i] = InterpolateColors(
              color::Transparent,
              original_.color_mode().toArgbColor(reader(ptr, offset + w)),
              interpolation_fraction);
        } else if (yMax > orig_extents.yMax()) {
          result[i] =
              InterpolateColors(original_.get(xMin, yMin), color::Transparent,
                                interpolation_fraction);
        } else {
          result[i] = InterpolateColors(
              original_.color_mode().toArgbColor(reader(ptr, offset)),
              original_.color_mode().toArgbColor(reader(ptr, offset + w)),
              interpolation_fraction);
        }
        continue;
      }
      if (yMin == yMax) {
        int16_t interpolation_fraction = (uint16_t)(256 * (orig.x - xMin));
        if (xMin < orig_extents.xMin()) {
          result[i] = InterpolateColors(
              color::Transparent,
              original_.color_mode().toArgbColor(reader(ptr, offset + 1)),
              interpolation_fraction);
        } else if (xMax > orig_extents.xMax()) {
          result[i] = InterpolateColors(
              original_.color_mode().toArgbColor(reader(ptr, offset)),
              color::Transparent, interpolation_fraction);
        } else {
          result[i] = InterpolateColors(
              original_.color_mode().toArgbColor(reader(ptr, offset)),
              original_.color_mode().toArgbColor(reader(ptr, offset + 1)),
              interpolation_fraction);
        }
        continue;
      }
      // General case.
      int16_t interpolation_fraction_x = (int16_t)(256 * (orig.x - xMin));
      int16_t interpolation_fraction_y = (int16_t)(256 * (orig.y - yMin));

      // Color a = color::Transparent;
      // Color b = color::Transparent;
      // Color c = color::Transparent;
      // Color d = color::Transparent;

      // if (xMin >= orig_extents.xMin()) {
      //   if (yMin >= orig_extents.yMin()) {
      //     a = original_.color_mode().toArgbColor(reader(ptr, offset));
      //   }
      //   if (yMax <= orig_extents.yMax()) {
      //     c = original_.color_mode().toArgbColor(reader(ptr, offset + w));
      //   }
      // }
      // if (xMax <= orig_extents.xMax()) {
      //   if (yMin >= orig_extents.yMin()) {
      //     b = original_.color_mode().toArgbColor(reader(ptr, offset + 1));
      //   }
      //   if (yMax <= orig_extents.yMax()) {
      //     d = original_.color_mode().toArgbColor(reader(ptr, offset + w +
      //     1));
      //   }
      // }
      // result[i] =
      //     InterpolateColors(InterpolateColors(a, b,
      //     interpolation_fraction_x),
      //                       InterpolateColors(c, d,
      //                       interpolation_fraction_x),
      //                       interpolation_fraction_y);

      Color ab_mix = color::Transparent;
      if (yMin >= orig_extents.yMin()) {
        if (xMin < orig_extents.xMin()) {
          // ab_mix = color::Olive;
          ab_mix = InterpolateColors(
              color::Transparent,
              original_.color_mode().toArgbColor(reader(ptr, offset + 1)),
              interpolation_fraction_x);
          // ab_mix = InterpolateColorWithTransparency(
          //     original_.color_mode().toArgbColor(reader(ptr, offset + 1)),
          //     interpolation_fraction_x);
        } else if (xMax > orig_extents.xMax()) {
          // ab_mix = color::BlueViolet;
          ab_mix = InterpolateColors(
              original_.color_mode().toArgbColor(reader(ptr, offset)),
              color::Transparent, interpolation_fraction_x);
          // ab_mix = InterpolateColorWithTransparency(
          //     original_.color_mode().toArgbColor(reader(ptr, offset)),
          //     256 - interpolation_fraction_x);
        } else {
          // ab_mix =
          //     interpolator(reader(ptr, offset), reader(ptr, offset + 1),
          //                  interpolation_fraction_x, original_.color_mode());
          ab_mix = InterpolateColors(
              original_.color_mode().toArgbColor(reader(ptr, offset)),
              original_.color_mode().toArgbColor(reader(ptr, offset + 1)),
              interpolation_fraction_x);
        }
      }
      Color cd_mix = color::Transparent;
      if (yMax <= orig_extents.yMax()) {
        if (xMin < orig_extents.xMin()) {
          // cd_mix = color::Red;
          cd_mix = InterpolateColors(
              color::Transparent,
              original_.color_mode().toArgbColor(reader(ptr, offset + w + 1)),
              interpolation_fraction_x);
          // cd_mix = InterpolateColorWithTransparency(
          //     original_.color_mode().toArgbColor(reader(ptr, offset + w +
          //     1)), interpolation_fraction_x);
        } else if (xMax > orig_extents.xMax()) {
          // cd_mix = color::Blue;
          // cd_mix = InterpolateColorWithTransparency(
          //     original_.color_mode().toArgbColor(reader(ptr, offset + w)),
          //     256 - interpolation_fraction_x);
          cd_mix = InterpolateColors(
              original_.color_mode().toArgbColor(reader(ptr, offset + w)),
              color::Transparent, interpolation_fraction_x);
        } else {
          // cd_mix =
          //     interpolator(reader(ptr, offset + w), reader(ptr, offset + w +
          //     1),
          //                  interpolation_fraction_x, original_.color_mode());
          cd_mix = InterpolateColors(
              original_.color_mode().toArgbColor(reader(ptr, offset + w)),
              original_.color_mode().toArgbColor(reader(ptr, offset + w + 1)),
              interpolation_fraction_x);
        }
      }

      result[i] = InterpolateColors(ab_mix, cd_mix, interpolation_fraction_y);
    }
  }

  Box extents() const override { return extents_; }
  Box anchorExtents() const override { return anchor_extents_; }

 private:
  RasterType& original_;
  TransformationType inverse_transformation_;
  Box extents_;
  Box anchor_extents_;
};

template <typename RasterType, typename TransformationType>
TransformedRaster<RasterType, TransformationType> TransformRaster(
    RasterType& original, TransformationType transformation) {
  return TransformedRaster<RasterType, TransformationType>(original,
                                                           transformation);
}

}  // namespace roo_display
