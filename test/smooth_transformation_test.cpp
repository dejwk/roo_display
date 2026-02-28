#include "roo_display/shape/smooth_transformation.h"

#include "gtest/gtest.h"

namespace roo_display {

namespace {

constexpr float kEps = 1e-4f;
constexpr float kHalfPi = 1.57079632679f;

void ExpectPointNear(FpPoint p, float x, float y) {
  EXPECT_NEAR(p.x, x, kEps);
  EXPECT_NEAR(p.y, y, kEps);
}

}  // namespace

TEST(SmoothTransformation, IdentityApply) {
  auto t = IdentityTransformation();
  ExpectPointNear(t.apply({2.5f, -1.5f}), 2.5f, -1.5f);
}

TEST(SmoothTransformation, TranslationApply) {
  auto t = Translate(3.0f, -2.0f);
  ExpectPointNear(t.apply({1.0f, 4.0f}), 4.0f, 2.0f);
}

TEST(SmoothTransformation, ScalingApply) {
  auto t = Scale(2.0f, -3.0f);
  ExpectPointNear(t.apply({1.5f, 2.0f}), 3.0f, -6.0f);
}

TEST(SmoothTransformation, RotationApply) {
  auto t = RotateRight(kHalfPi);
  ExpectPointNear(t.apply({1.0f, 0.0f}), 0.0f, 1.0f);
}

TEST(SmoothTransformation, ScaleAboutApply) {
  auto t = ScaleAbout(2.0f, 3.0f, FpPoint{2.0f, 3.0f});
  ExpectPointNear(t.apply({3.0f, 5.0f}), 4.0f, 9.0f);
}

TEST(SmoothTransformation, RotateRightAboutApply) {
  auto t = RotateRightAbout(kHalfPi, FpPoint{1.0f, 1.0f});
  ExpectPointNear(t.apply({2.0f, 1.0f}), 1.0f, 2.0f);
}

TEST(SmoothTransformation, ShearApply) {
  auto t = Shear(0.2f, -0.3f);
  ExpectPointNear(t.apply({2.0f, 3.0f}), 2.48f, 2.4f);
}

TEST(SmoothTransformation, ShearHorizontallyAboutApply) {
  auto t = ShearHorizontallyAbout(2.0f, 3.0f);
  ExpectPointNear(t.apply({1.0f, 4.0f}), 3.0f, 4.0f);
}

TEST(SmoothTransformation, ShearVerticallyAboutApply) {
  auto t = ShearVerticallyAbout(-1.0f, 2.0f);
  ExpectPointNear(t.apply({3.0f, 1.0f}), 3.0f, 0.0f);
}

TEST(SmoothTransformation, AffineApply) {
  AffineTransformation t(2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f);
  ExpectPointNear(t.apply({1.0f, 2.0f}), 14.0f, 21.0f);
}

TEST(SmoothTransformation, ProjectiveApply) {
  ProjectiveTransformation t(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.1f, 0.0f,
                             1.0f);
  ExpectPointNear(t.apply({2.0f, 1.0f}), 1.6666666f, 0.8333333f);
}

TEST(SmoothTransformation, PerspectiveApply) {
  auto t = Perspective(0.25f, 0.1f);
  ExpectPointNear(t.apply({2.0f, 4.0f}), 1.0526316f, 2.1052632f);
}

TEST(SmoothTransformation, PerspectiveAboutApply) {
  auto t = PerspectiveAbout(0.1f, 0.0f, FpPoint{3.0f, 0.0f});
  ExpectPointNear(t.apply({4.0f, 2.0f}), 3.9090909f, 1.8181818f);
}

TEST(SmoothTransformation, ThenTranslationScaling) {
  auto t = Translate(1.0f, 2.0f).then(Scale(3.0f, 4.0f));
  ExpectPointNear(t.apply({5.0f, 6.0f}), 18.0f, 32.0f);
}

TEST(SmoothTransformation, ThenScalingRotation) {
  auto t = Scale(2.0f, 3.0f).then(RotateRight(kHalfPi));
  ExpectPointNear(t.apply({1.0f, 2.0f}), -6.0f, 2.0f);
}

TEST(SmoothTransformation, ThenAffineAffineMatchesSequential) {
  AffineTransformation a(1.2f, -0.7f, 0.4f, 1.1f, 3.0f, -2.0f);
  AffineTransformation b(0.8f, 0.3f, -0.2f, 1.5f, -1.0f, 4.0f);
  auto chained = a.then(b);
  FpPoint p{2.5f, -1.0f};
  FpPoint expected = b.apply(a.apply(p));
  FpPoint actual = chained.apply(p);
  ExpectPointNear(actual, expected.x, expected.y);
}

TEST(SmoothTransformation, ThenAffineProjectiveMatchesSequential) {
  AffineTransformation a(1.1f, 0.2f, -0.1f, 0.9f, 2.0f, -3.0f);
  auto p = Perspective(0.08f, -0.03f);
  auto chained = a.then(p);
  FpPoint point{3.0f, 1.5f};
  FpPoint expected = p.apply(a.apply(point));
  FpPoint actual = chained.apply(point);
  ExpectPointNear(actual, expected.x, expected.y);
}

TEST(SmoothTransformation, ThenProjectiveAffineMatchesSequential) {
  ProjectiveTransformation p(1.0f, 0.0f, 0.5f, 0.0f, 1.0f, -0.25f, 0.06f,
                             0.02f, 1.0f);
  auto a = RotateRight(0.35f).then(Translate(1.0f, -2.0f));
  auto chained = p.then(a);
  FpPoint point{-1.0f, 4.0f};
  FpPoint expected = a.apply(p.apply(point));
  FpPoint actual = chained.apply(point);
  ExpectPointNear(actual, expected.x, expected.y);
}

TEST(SmoothTransformation, ThenProjectiveProjectiveMatchesSequential) {
  auto p1 = Perspective(0.05f, 0.01f);
  auto p2 = PerspectiveAbout(-0.02f, 0.03f, FpPoint{1.5f, -2.0f});
  auto chained = p1.then(p2);
  FpPoint point{2.2f, -0.8f};
  FpPoint expected = p2.apply(p1.apply(point));
  FpPoint actual = chained.apply(point);
  ExpectPointNear(actual, expected.x, expected.y);
}

TEST(SmoothTransformation, AffineInversionRoundTrip) {
  AffineTransformation a(1.3f, -0.4f, 0.7f, 1.1f, -2.5f, 4.0f);
  auto inv = a.inversion();
  FpPoint point{3.2f, -1.7f};
  FpPoint round_trip = inv.apply(a.apply(point));
  ExpectPointNear(round_trip, point.x, point.y);
}

TEST(SmoothTransformation, ProjectiveInversionRoundTrip) {
  ProjectiveTransformation p(1.1f, 0.2f, -0.4f, -0.1f, 0.9f, 0.7f, 0.03f,
                             -0.02f, 1.0f);
  auto inv = p.inversion();
  FpPoint point{2.4f, -1.3f};
  FpPoint round_trip = inv.apply(p.apply(point));
  ExpectPointNear(round_trip, point.x, point.y);
}

}  // namespace roo_display
