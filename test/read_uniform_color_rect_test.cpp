
#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/composition/rasterizable_stack.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/shape/basic.h"
#include "roo_display/shape/shadow.h"
#include "roo_display/shape/smooth.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

// Fill tests.

TEST(ReadUniformColorRect, FillReturnsColor) {
  Fill fill(color::Red);
  Color result;
  EXPECT_TRUE(fill.readUniformColorRect(0, 0, 10, 10, &result));
  EXPECT_EQ(result, color::Red);
}

TEST(ReadUniformColorRect, FillTransparent) {
  Fill fill(color::Transparent);
  Color result;
  EXPECT_TRUE(fill.readUniformColorRect(-100, -100, 100, 100, &result));
  EXPECT_EQ(result, color::Transparent);
}

TEST(ReadUniformColorRect, FillSinglePixel) {
  Fill fill(color::Blue);
  Color result;
  EXPECT_TRUE(fill.readUniformColorRect(5, 5, 5, 5, &result));
  EXPECT_EQ(result, color::Blue);
}

// Clear tests.

TEST(ReadUniformColorRect, ClearAlwaysTransparent) {
  Clear clear;
  Color result;
  EXPECT_TRUE(clear.readUniformColorRect(0, 0, 10, 10, &result));
  EXPECT_EQ(result, color::Transparent);
}

TEST(ReadUniformColorRect, ClearLargeRect) {
  Clear clear;
  Color result;
  EXPECT_TRUE(clear.readUniformColorRect(-1000, -1000, 1000, 1000, &result));
  EXPECT_EQ(result, color::Transparent);
}

// FilledRect tests.

TEST(ReadUniformColorRect, FilledRectReturnsColor) {
  FilledRect rect(0, 0, 20, 20, color::Green);
  Color result;
  EXPECT_TRUE(rect.readUniformColorRect(5, 5, 15, 15, &result));
  EXPECT_EQ(result, color::Green);
}

TEST(ReadUniformColorRect, FilledRectSinglePixel) {
  FilledRect rect(0, 0, 10, 10, color::Navy);
  Color result;
  EXPECT_TRUE(rect.readUniformColorRect(3, 3, 3, 3, &result));
  EXPECT_EQ(result, color::Navy);
}

// SmoothShape tests.

TEST(ReadUniformColorRect, SmoothFilledRoundRectInterior) {
  auto shape = SmoothFilledRoundRect(0, 0, 100, 100, 5, color::Red);
  Color result;
  // Center region should be uniform (interior).
  EXPECT_TRUE(shape.readUniformColorRect(20, 20, 80, 80, &result));
  EXPECT_EQ(result, color::Red);
}

TEST(ReadUniformColorRect, SmoothFilledRoundRectOutside) {
  auto shape = SmoothFilledRoundRect(10, 10, 50, 50, 3, color::Red);
  Color result;
  // Completely outside the shape should be transparent.
  EXPECT_TRUE(shape.readUniformColorRect(0, 0, 5, 5, &result));
  EXPECT_EQ(result, color::Transparent);
}

TEST(ReadUniformColorRect, SmoothFilledRoundRectEdge) {
  auto shape = SmoothFilledRoundRect(10, 10, 50, 50, 3, color::Red);
  Color result;
  // Straddling the edge - should not be uniform.
  EXPECT_FALSE(shape.readUniformColorRect(8, 20, 12, 30, &result));
}

TEST(ReadUniformColorRect, SmoothFilledTriangleInterior) {
  // Large triangle (CCW vertex order) for a well-defined interior region.
  auto shape =
      SmoothFilledTriangle({500, 0}, {1000, 1000}, {0, 1000}, color::Red);
  Color result;
  // Well inside the triangle's interior.
  EXPECT_TRUE(shape.readUniformColorRect(480, 700, 520, 720, &result));
  EXPECT_EQ(result, color::Red);
}

TEST(ReadUniformColorRect, SmoothFilledTriangleOutside) {
  auto shape =
      SmoothFilledTriangle({500, 0}, {1000, 1000}, {0, 1000}, color::Red);
  Color result;
  // Below the triangle.
  EXPECT_TRUE(shape.readUniformColorRect(0, 1002, 10, 1010, &result));
  EXPECT_EQ(result, color::Transparent);
}

TEST(ReadUniformColorRect, SmoothArcOutside) {
  auto shape = SmoothArc({50, 50}, 20, 0, 3.14159f, color::Red);
  Color result;
  // Far away from the arc.
  EXPECT_TRUE(shape.readUniformColorRect(0, 0, 5, 5, &result));
  EXPECT_EQ(result, color::Transparent);
}

// RoundRectShadow tests.

TEST(ReadUniformColorRect, ShadowInterior) {
  RoundRectShadow shadow(Box(10, 10, 110, 110), color::Black, 5, 0, 0, 10);
  Color result;
  // Center should be uniform.
  EXPECT_TRUE(shadow.readUniformColorRect(30, 20, 90, 100, &result));
  EXPECT_EQ(result, color::Black);
}

TEST(ReadUniformColorRect, ShadowEdge) {
  RoundRectShadow shadow(Box(10, 10, 110, 110), color::Black, 5, 0, 0, 10);
  Color result;
  // At the border of the shadow (where alpha varies).
  EXPECT_FALSE(shadow.readUniformColorRect(0, 0, 20, 20, &result));
}

// RasterizableStack tests.

TEST(ReadUniformColorRect, StackSingleUniformLayer) {
  Fill red_fill(color::Red);
  RasterizableStack stack(Box(0, 0, 100, 100));
  stack.addInput(&red_fill, Box(0, 0, 100, 100));
  Color result;
  EXPECT_TRUE(stack.readUniformColorRect(10, 10, 50, 50, &result));
  EXPECT_EQ(result, color::Red);
}

TEST(ReadUniformColorRect, StackEmptyTransparent) {
  RasterizableStack stack(Box(0, 0, 100, 100));
  Color result;
  EXPECT_TRUE(stack.readUniformColorRect(10, 10, 50, 50, &result));
  EXPECT_EQ(result, color::Transparent);
}

TEST(ReadUniformColorRect, StackPartialCoverage) {
  Fill red_fill(color::Red);
  RasterizableStack stack(Box(0, 0, 100, 100));
  stack.addInput(&red_fill, Box(0, 0, 50, 50));
  Color result;
  EXPECT_FALSE(stack.readUniformColorRect(0, 0, 100, 100, &result));
}

TEST(ReadUniformColorRect, StackPartialCoverageTransparent) {
  // A transparent layer with partial coverage should be ignored.
  Clear clear;
  Fill red_fill(color::Red);
  RasterizableStack stack(Box(0, 0, 100, 100));
  stack.addInput(&red_fill, Box(0, 0, 100, 100));
  stack.addInput(&clear, Box(0, 0, 50, 50));
  Color result;
  EXPECT_TRUE(stack.readUniformColorRect(10, 10, 90, 90, &result));
  EXPECT_EQ(result, color::Red);
}

TEST(ReadUniformColorRect, StackMultipleUniformLayers) {
  Fill red_fill(color::Red);
  Fill green_fill(color::Green);
  RasterizableStack stack(Box(0, 0, 100, 100));
  stack.addInput(&red_fill, Box(0, 0, 100, 100));
  stack.addInput(&green_fill, Box(0, 0, 100, 100));
  Color result;
  EXPECT_TRUE(stack.readUniformColorRect(10, 10, 50, 50, &result));
  EXPECT_EQ(result, color::Green);
}

}  // namespace roo_display
