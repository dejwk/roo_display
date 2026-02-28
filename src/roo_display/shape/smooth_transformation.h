#pragma once

#include <cmath>

#include "roo_display/core/box.h"
#include "roo_display/core/raster.h"
#include "roo_display/core/rasterizable.h"
#include "roo_display/shape/point.h"

namespace roo_display {

/// Transformation classes for smooth shapes.
///
/// Translation, scaling, and rotation are primitive transformations.
/// `AffineTransformation` combines translation, scaling, and rotation. Use
/// `then()` to compose transformations.
///
/// Common methods:
/// - `apply(FpPoint)` transforms a point.
/// - `then(T)` composes with another transformation.
/// - `inversion()` returns the inverse transformation.
/// - `transformExtents(Box)` returns a bounding box covering transformed
///   pixels.
///
/// Prefer using the factory functions below and `auto`.

/// Identity transformation.
class IdentityTransformation;
/// Translation transformation.
class Translation;
/// Scaling transformation.
class Scaling;
/// Rotation transformation.
class Rotation;
/// Affine transformation (composition of translation, scaling, rotation).
class AffineTransformation;
/// Projective transformation (homography in 2D).
class ProjectiveTransformation;

/// Return a translation by the specified vector.
Translation Translate(float dx, float dy);

/// Return a scaling about the origin.
Scaling Scale(float sx, float sy);

/// Return a scaling about a given center.
AffineTransformation ScaleAbout(float sx, float sy, const FpPoint& center);

/// Return a clockwise rotation about the origin.
Rotation RotateRight(float angle);

/// Return a 90-degree clockwise rotation about the origin.
Rotation RotateRight();

/// Return a clockwise rotation about a point.
AffineTransformation RotateRightAbout(float angle, const FpPoint& center);

/// Return a 90-degree clockwise rotation about a point.
AffineTransformation RotateRightAbout(const FpPoint& center);

/// Return a counter-clockwise rotation about the origin.
Rotation RotateLeft(float angle);

/// Return a 90-degree counter-clockwise rotation about the origin.
Rotation RotateLeft();

/// Return a 90-degree counter-clockwise rotation about a point.
AffineTransformation RotateLeftAbout(float angle, const FpPoint& center);

/// Return a shear rooted at the origin.
AffineTransformation Shear(float sx, float sy);

/// Return a shear rooted at the specified point.
AffineTransformation Shear(float sx, float sy, const FpPoint& base);

/// Return a horizontal shear rooted at the origin.
AffineTransformation ShearHorizontally(float sx);

/// Return a horizontal shear rooted at `base_y`.
AffineTransformation ShearHorizontallyAbout(float sx, float base_y);

/// Return a vertical shear rooted at the origin.
AffineTransformation ShearVertically(float sy);

/// Return a vertical shear rooted at `base_x`.
AffineTransformation ShearVerticallyAbout(float sy, float base_x);

/// Return a perspective transformation rooted at the origin.
///
/// Maps (x, y) to (x / d, y / d), where d = 1 + px * x + py * y.
ProjectiveTransformation Perspective(float px, float py);

/// Return a perspective transformation rooted at `base`.
ProjectiveTransformation PerspectiveAbout(float px, float py,
                                          const FpPoint& base);

template <typename RasterType, typename TransformationType>
class TransformedRaster;

/// Return a rasterizable representation of a transformed raster.
template <typename RasterType, typename TransformationType>
TransformedRaster<RasterType, TransformationType> TransformRaster(
    RasterType& original, TransformationType transformation);

// Implementation details.

/// Identity transformation.
class IdentityTransformation {
 public:
  IdentityTransformation() {}
  FpPoint apply(FpPoint p) const { return p; }

  IdentityTransformation then(IdentityTransformation t) const { return *this; }
  Translation then(Translation t) const;
  Scaling then(Scaling t) const;
  Rotation then(Rotation t) const;
  AffineTransformation then(AffineTransformation t) const;
  ProjectiveTransformation then(ProjectiveTransformation t) const;

  IdentityTransformation inversion() const { return *this; }

  Box transformExtents(Box extents) const { return extents; }
};

/// Translation by (dx, dy).
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
  ProjectiveTransformation then(ProjectiveTransformation t) const;

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

/// Scaling by (sx, sy).
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
  ProjectiveTransformation then(ProjectiveTransformation t) const;

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

/// Rotation by angle in radians (clockwise).
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
  ProjectiveTransformation then(ProjectiveTransformation t) const;

  Rotation inversion() const { return Rotation(-theta_); }

  Box transformExtents(Box extents) const;

 private:
  float theta_, sin_theta_, cos_theta_;
};

/// General affine transformation.
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
  ProjectiveTransformation then(ProjectiveTransformation t) const;

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

/// General projective transformation (2D homography).
class ProjectiveTransformation {
 public:
  ProjectiveTransformation(float m11, float m12, float m13, float m21,
                           float m22, float m23, float m31, float m32,
                           float m33)
      : m11_(m11),
        m12_(m12),
        m13_(m13),
        m21_(m21),
        m22_(m22),
        m23_(m23),
        m31_(m31),
        m32_(m32),
        m33_(m33) {}

  ProjectiveTransformation(IdentityTransformation t)
      : m11_(1.0f),
        m12_(0.0f),
        m13_(0.0f),
        m21_(0.0f),
        m22_(1.0f),
        m23_(0.0f),
        m31_(0.0f),
        m32_(0.0f),
        m33_(1.0f) {}

  ProjectiveTransformation(Translation t)
      : m11_(1.0f),
        m12_(0.0f),
        m13_(t.dx()),
        m21_(0.0f),
        m22_(1.0f),
        m23_(t.dy()),
        m31_(0.0f),
        m32_(0.0f),
        m33_(1.0f) {}

  ProjectiveTransformation(Scaling t)
      : m11_(t.sx()),
        m12_(0.0f),
        m13_(0.0f),
        m21_(0.0f),
        m22_(t.sy()),
        m23_(0.0f),
        m31_(0.0f),
        m32_(0.0f),
        m33_(1.0f) {}

  ProjectiveTransformation(Rotation t)
      : m11_(t.cos_theta()),
        m12_(-t.sin_theta()),
        m13_(0.0f),
        m21_(t.sin_theta()),
        m22_(t.cos_theta()),
        m23_(0.0f),
        m31_(0.0f),
        m32_(0.0f),
        m33_(1.0f) {}

  ProjectiveTransformation(AffineTransformation t)
      : m11_(t.a11()),
        m12_(t.a12()),
        m13_(t.tx()),
        m21_(t.a21()),
        m22_(t.a22()),
        m23_(t.ty()),
        m31_(0.0f),
        m32_(0.0f),
        m33_(1.0f) {}

  FpPoint apply(FpPoint p) const {
    float w = p.x * m31_ + p.y * m32_ + m33_;
    return FpPoint{(p.x * m11_ + p.y * m12_ + m13_) / w,
                   (p.x * m21_ + p.y * m22_ + m23_) / w};
  }

  float m11() const { return m11_; }
  float m12() const { return m12_; }
  float m13() const { return m13_; }
  float m21() const { return m21_; }
  float m22() const { return m22_; }
  float m23() const { return m23_; }
  float m31() const { return m31_; }
  float m32() const { return m32_; }
  float m33() const { return m33_; }

  ProjectiveTransformation then(IdentityTransformation t) const {
    return *this;
  }
  ProjectiveTransformation then(Translation t) const;
  ProjectiveTransformation then(Scaling t) const;
  ProjectiveTransformation then(Rotation t) const;
  ProjectiveTransformation then(AffineTransformation t) const;
  ProjectiveTransformation then(ProjectiveTransformation t) const;

  ProjectiveTransformation inversion() const;

  Box transformExtents(Box extents) const;

 private:
  float m11_, m12_, m13_;
  float m21_, m22_, m23_;
  float m31_, m32_, m33_;
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
      FpPoint orig =
          inverse_transformation_.apply(FpPoint{(float)x[i], (float)y[i]});
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
        result[i] = reader(ptr, offset, original_.color_mode());
        continue;
      }
      if (xMin == xMax) {
        int16_t interpolation_fraction = (int16_t)(256 * (orig.y - yMin));
        if (yMin < orig_extents.yMin()) {
          result[i] =
              InterpolateColors(color::Transparent,
                                reader(ptr, offset + w, original_.color_mode()),
                                interpolation_fraction);
        } else if (yMax > orig_extents.yMax()) {
          result[i] =
              InterpolateColors(original_.get(xMin, yMin), color::Transparent,
                                interpolation_fraction);
        } else {
          result[i] =
              InterpolateColors(reader(ptr, offset, original_.color_mode()),
                                reader(ptr, offset + w, original_.color_mode()),
                                interpolation_fraction);
        }
        continue;
      }
      if (yMin == yMax) {
        int16_t interpolation_fraction = (uint16_t)(256 * (orig.x - xMin));
        if (xMin < orig_extents.xMin()) {
          result[i] =
              InterpolateColors(color::Transparent,
                                reader(ptr, offset + 1, original_.color_mode()),
                                interpolation_fraction);
        } else if (xMax > orig_extents.xMax()) {
          result[i] =
              InterpolateColors(reader(ptr, offset, original_.color_mode()),
                                color::Transparent, interpolation_fraction);
        } else {
          result[i] =
              InterpolateColors(reader(ptr, offset, original_.color_mode()),
                                reader(ptr, offset + 1, original_.color_mode()),
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
      //     a = reader(ptr, offset, original_.color_mode());
      //   }
      //   if (yMax <= orig_extents.yMax()) {
      //     c = reader(ptr, offset + w, original_.color_mode());
      //   }
      // }
      // if (xMax <= orig_extents.xMax()) {
      //   if (yMin >= orig_extents.yMin()) {
      //     b = reader(ptr, offset + 1, original_.color_mode());
      //   }
      //   if (yMax <= orig_extents.yMax()) {
      //     d = reader(ptr, offset + w + 1, original_.color_mode());
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
          ab_mix =
              InterpolateColors(color::Transparent,
                                reader(ptr, offset + 1, original_.color_mode()),
                                interpolation_fraction_x);
          // ab_mix = InterpolateColorWithTransparency(
          //     reader(ptr, offset + 1, original_.color_mode())),
          //     interpolation_fraction_x);
        } else if (xMax > orig_extents.xMax()) {
          // ab_mix = color::BlueViolet;
          ab_mix =
              InterpolateColors(reader(ptr, offset, original_.color_mode()),
                                color::Transparent, interpolation_fraction_x);
          // ab_mix = InterpolateColorWithTransparency(
          //     reader(ptr, offset,original_.color_mode())), 256 -
          //     interpolation_fraction_x);
        } else {
          // ab_mix =
          //     interpolator(reader(ptr, offset, original_.color_mode()),
          //     reader(ptr, offset + 1, original_.color_mode()),
          //                  interpolation_fraction_x, original_.color_mode());
          ab_mix =
              InterpolateColors(reader(ptr, offset, original_.color_mode()),
                                reader(ptr, offset + 1, original_.color_mode()),
                                interpolation_fraction_x);
        }
      }
      Color cd_mix = color::Transparent;
      if (yMax <= orig_extents.yMax()) {
        if (xMin < orig_extents.xMin()) {
          // cd_mix = color::Red;
          cd_mix = InterpolateColors(
              color::Transparent,
              reader(ptr, offset + w + 1, original_.color_mode()),
              interpolation_fraction_x);
          // cd_mix = InterpolateColorWithTransparency(
          //     original_.color_mode().toArgbColor(reader(ptr, offset + w +
          //     1, original_.color_mode())), interpolation_fraction_x);
        } else if (xMax > orig_extents.xMax()) {
          // cd_mix = color::Blue;
          // cd_mix = InterpolateColorWithTransparency(
          //     original_.color_mode().toArgbColor(reader(ptr, offset + w,
          //     original_.color_mode())), 256 - interpolation_fraction_x);
          cd_mix =
              InterpolateColors(reader(ptr, offset + w, original_.color_mode()),
                                color::Transparent, interpolation_fraction_x);
        } else {
          // cd_mix =
          //     interpolator(reader(ptr, offset + w, original_.color_mode()),
          //     reader(ptr, offset + w + 1, original_.color_mode()),
          //     interpolation_fraction_x, original_.color_mode());
          //                  interpolation_fraction_x, original_.color_mode());
          cd_mix = InterpolateColors(
              reader(ptr, offset + w, original_.color_mode()),
              reader(ptr, offset + w + 1, original_.color_mode()),
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
