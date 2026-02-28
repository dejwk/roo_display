#include "roo_display/shape/smooth_transformation.h"

namespace roo_display {

namespace {

template <typename TransformationType>
Box TransformExtents(const TransformationType& transformation, Box extents) {
  FpPoint tl =
      transformation.apply({(float)extents.xMin(), (float)extents.yMin()});
  FpPoint tr =
      transformation.apply({(float)extents.xMax(), (float)extents.yMin()});
  FpPoint bl =
      transformation.apply({(float)extents.xMin(), (float)extents.yMax()});
  FpPoint br =
      transformation.apply({(float)extents.xMax(), (float)extents.yMax()});
  float xMin = std::min(std::min(tl.x, tr.x), std::min(bl.x, br.x));
  float yMin = std::min(std::min(tl.y, tr.y), std::min(bl.y, br.y));
  float xMax = std::max(std::max(tl.x, tr.x), std::max(bl.x, br.x));
  float yMax = std::max(std::max(tl.y, tr.y), std::max(bl.y, br.y));
  return Box((int16_t)floorf(xMin), (int16_t)floorf(yMin), (int16_t)ceilf(xMax),
             (int16_t)ceilf(yMax));
}

}  // namespace

Translation Translate(float dx, float dy) { return Translation(dx, dy); }

Scaling Scale(float sx, float sy) { return Scaling(sx, sy); }

AffineTransformation ScaleAbout(float sx, float sy, const FpPoint& center) {
  return Translate(-center.x, -center.y)
      .then(Scale(sx, sy))
      .then(Translate(center.x, center.y));
}

Rotation RotateRight(float angle) { return Rotation(angle); }

Rotation RotateRight() { return RotateRight(M_PI * 0.5f); }

AffineTransformation RotateRightAbout(float angle, const FpPoint& center) {
  return Translate(-center.x, -center.y)
      .then(RotateRight(angle))
      .then(Translate(center.x, center.y));
}

AffineTransformation RotateRightAbout(const FpPoint& center) {
  return Translate(-center.x, -center.y)
      .then(RotateRight())
      .then(Translate(center.x, center.y));
}

Rotation RotateLeft(float angle) { return Rotation(-angle); }

Rotation RotateLeft() { return RotateLeft(M_PI * 0.5f); }

AffineTransformation RotateLeftAbout(float angle, const FpPoint& center) {
  return RotateRightAbout(-angle, center);
}

AffineTransformation RotateLeftAbout(const FpPoint& center) {
  return RotateLeftAbout(M_PI * 0.5f, center);
}

AffineTransformation Shear(float sx, float sy) {
  return AffineTransformation(1.0f + sx * sy, sx, sy, 1.0f, 0.0f, 0.0f);
}

AffineTransformation Shear(float sx, float sy, const FpPoint& base) {
  return Translate(-base.x, -base.y)
      .then(Shear(sx, sy))
      .then(Translate(base.x, base.y));
}

AffineTransformation ShearHorizontally(float sx) {
  return AffineTransformation(1.0f, sx, 0.0f, 1.0f, 0.0f, 0.0f);
}

AffineTransformation ShearHorizontallyAbout(float sx, float base_y) {
  return Translate(0.0f, -base_y)
      .then(Shear(sx, 0.0f))
      .then(Translate(0.0f, base_y));
}

AffineTransformation ShearVertically(float sy) {
  return AffineTransformation(1.0f, 0.0f, sy, 1.0f, 0.0f, 0.0f);
}

AffineTransformation ShearVerticallyAbout(float sy, float base_x) {
  return Translate(-base_x, 0.0f)
      .then(Shear(0.0f, sy))
      .then(Translate(base_x, 0.0f));
}

ProjectiveTransformation Projective(float m11, float m12, float m13, float m21,
                                    float m22, float m23, float m31, float m32,
                                    float m33) {
  return ProjectiveTransformation(m11, m12, m13, m21, m22, m23, m31, m32, m33);
}

ProjectiveTransformation Perspective(float px, float py) {
  return Projective(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, px, py, 1.0f);
}

ProjectiveTransformation PerspectiveAbout(float px, float py,
                                          const FpPoint& base) {
  return Translate(-base.x, -base.y)
      .then(Perspective(px, py))
      .then(Translate(base.x, base.y));
}

AffineTransformation IdentityTransformation::then(
    AffineTransformation t) const {
  return t;
}

Translation IdentityTransformation::then(Translation t) const { return t; }

Scaling IdentityTransformation::then(Scaling t) const { return t; }

Rotation IdentityTransformation::then(Rotation t) const { return t; }

ProjectiveTransformation IdentityTransformation::then(
    ProjectiveTransformation t) const {
  return t;
}

Translation Translation::then(Translation t) const {
  return Translation(dx_ + t.dx(), dy_ + t.dy());
}

AffineTransformation Translation::then(Scaling t) const {
  return AffineTransformation(t.sx(), 0, 0, t.sy(), t.sx() * dx_, t.sy() * dy_);
}

AffineTransformation Translation::then(Rotation t) const {
  return AffineTransformation(t.cos_theta(), -t.sin_theta(), t.sin_theta(),
                              t.cos_theta(),
                              t.cos_theta() * dx_ - t.sin_theta() * dy_,
                              t.sin_theta() * dx_ + t.cos_theta() * dy_);
}

AffineTransformation Translation::then(AffineTransformation t) const {
  return AffineTransformation(t.a11(), t.a12(), t.a21(), t.a22(),
                              t.a11() * dx_ + t.a12() * dy_ + t.tx(),
                              t.a21() * dx_ + t.a21() * dy_ + t.ty());
}

ProjectiveTransformation Translation::then(ProjectiveTransformation t) const {
  return ProjectiveTransformation(*this).then(t);
}

AffineTransformation Scaling::then(Translation t) const {
  return AffineTransformation(sx_, 0, 0, sy_, t.dx(), t.dy());
}

Scaling Scaling::then(Scaling t) const {
  return Scaling(sx_ * t.sx(), sy_ * t.sy());
}

AffineTransformation Scaling::then(Rotation t) const {
  return AffineTransformation(sx_ * t.cos_theta(), -sy_ * t.sin_theta(),
                              sx_ * t.sin_theta(), sy_ * t.cos_theta(), 0, 0);
}

AffineTransformation Scaling::then(AffineTransformation t) const {
  return AffineTransformation(sx_ * t.a11(), sy_ * t.a12(), sx_ * t.a21(),
                              sy_ * t.a22(), t.tx(), t.ty());
}

ProjectiveTransformation Scaling::then(ProjectiveTransformation t) const {
  return ProjectiveTransformation(*this).then(t);
}

Box Rotation::transformExtents(Box extents) const {
  return TransformExtents(*this, extents);
}

AffineTransformation Rotation::then(Translation t) const {
  return AffineTransformation(cos_theta_, -sin_theta_, sin_theta_, cos_theta_,
                              t.dx(), t.dy());
}

AffineTransformation Rotation::then(Scaling t) const {
  return AffineTransformation(t.sx() * cos_theta_, -t.sx() * sin_theta_,
                              t.sy() * sin_theta_, t.sy() * cos_theta_, 0, 0);
}

Rotation Rotation::then(Rotation t) const {
  return Rotation(theta_ + t.theta());
}

AffineTransformation Rotation::then(AffineTransformation t) const {
  return AffineTransformation(t.a11() * cos_theta_ + t.a12() * sin_theta_,
                              -t.a11() * sin_theta_ + t.a12() * cos_theta_,
                              t.a21() * cos_theta_ + t.a22() * sin_theta_,
                              -t.a21() * sin_theta_ + t.a22() * cos_theta_,
                              t.tx(), t.ty());
}

ProjectiveTransformation Rotation::then(ProjectiveTransformation t) const {
  return ProjectiveTransformation(*this).then(t);
}

Box AffineTransformation::transformExtents(Box extents) const {
  return TransformExtents(*this, extents);
}

AffineTransformation AffineTransformation::then(Translation t) const {
  return AffineTransformation(a11_, a12_, a21_, a22_, tx_ + t.dx(),
                              ty_ + t.dy());
}

AffineTransformation AffineTransformation::then(Scaling t) const {
  return AffineTransformation(t.sx() * a11_, t.sx() * a12_, t.sy() * a21_,
                              t.sy() * a22_, t.sx() * tx_, t.sy() * ty_);
}

AffineTransformation AffineTransformation::then(Rotation t) const {
  return AffineTransformation(t.cos_theta() * a11_ - t.sin_theta() * a21_,
                              t.cos_theta() * a12_ - t.sin_theta() * a22_,
                              t.sin_theta() * a11_ + t.cos_theta() * a21_,
                              t.sin_theta() * a12_ + t.cos_theta() * a22_,
                              t.cos_theta() * tx_ - t.sin_theta() * ty_,
                              t.sin_theta() * tx_ + t.cos_theta() * ty_);
}

AffineTransformation AffineTransformation::then(AffineTransformation t) const {
  return AffineTransformation(
      t.a11() * a11_ + t.a12() * a21_, t.a11() * a12_ + t.a12() * a22_,
      t.a21() * a11_ + t.a22() * a21_, t.a21() * a12_ + t.a22() * a22_,
      t.a11() * tx_ + t.a12() * ty_ + t.tx(),
      t.a21() * tx_ + t.a22() * ty_ + t.ty());
}

ProjectiveTransformation AffineTransformation::then(
    ProjectiveTransformation t) const {
  return ProjectiveTransformation(*this).then(t);
}

ProjectiveTransformation ProjectiveTransformation::then(Translation t) const {
  return then(ProjectiveTransformation(t));
}

ProjectiveTransformation ProjectiveTransformation::then(Scaling t) const {
  return then(ProjectiveTransformation(t));
}

ProjectiveTransformation ProjectiveTransformation::then(Rotation t) const {
  return then(ProjectiveTransformation(t));
}

ProjectiveTransformation ProjectiveTransformation::then(
    AffineTransformation t) const {
  return then(ProjectiveTransformation(t));
}

ProjectiveTransformation ProjectiveTransformation::then(
    ProjectiveTransformation t) const {
  return ProjectiveTransformation(
      t.m11() * m11_ + t.m12() * m21_ + t.m13() * m31_,
      t.m11() * m12_ + t.m12() * m22_ + t.m13() * m32_,
      t.m11() * m13_ + t.m12() * m23_ + t.m13() * m33_,
      t.m21() * m11_ + t.m22() * m21_ + t.m23() * m31_,
      t.m21() * m12_ + t.m22() * m22_ + t.m23() * m32_,
      t.m21() * m13_ + t.m22() * m23_ + t.m23() * m33_,
      t.m31() * m11_ + t.m32() * m21_ + t.m33() * m31_,
      t.m31() * m12_ + t.m32() * m22_ + t.m33() * m32_,
      t.m31() * m13_ + t.m32() * m23_ + t.m33() * m33_);
}

ProjectiveTransformation ProjectiveTransformation::inversion() const {
  float c11 = m22_ * m33_ - m23_ * m32_;
  float c12 = -(m21_ * m33_ - m23_ * m31_);
  float c13 = m21_ * m32_ - m22_ * m31_;
  float c21 = -(m12_ * m33_ - m13_ * m32_);
  float c22 = m11_ * m33_ - m13_ * m31_;
  float c23 = -(m11_ * m32_ - m12_ * m31_);
  float c31 = m12_ * m23_ - m13_ * m22_;
  float c32 = -(m11_ * m23_ - m13_ * m21_);
  float c33 = m11_ * m22_ - m12_ * m21_;

  float det = m11_ * c11 + m12_ * c12 + m13_ * c13;
  float inv_det = 1.0f / det;
  return ProjectiveTransformation(c11 * inv_det, c21 * inv_det, c31 * inv_det,
                                  c12 * inv_det, c22 * inv_det, c32 * inv_det,
                                  c13 * inv_det, c23 * inv_det, c33 * inv_det);
}

Box ProjectiveTransformation::transformExtents(Box extents) const {
  return TransformExtents(*this, extents);
}

}  // namespace roo_display
