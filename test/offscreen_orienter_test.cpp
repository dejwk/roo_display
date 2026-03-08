#include <array>

#include "gtest/gtest.h"
#include "roo_display/core/offscreen.h"

namespace roo_display {
namespace internal {
namespace {

std::array<Orientation, 8> AllOrientations() {
  return {Orientation::RightDown(), Orientation::DownRight(),
          Orientation::LeftDown(),  Orientation::DownLeft(),
          Orientation::RightUp(),   Orientation::UpRight(),
          Orientation::LeftUp(),    Orientation::UpLeft()};
}

void TransformPoint(Orientation orientation, int16_t x_max, int16_t y_max,
                    int16_t& x, int16_t& y) {
  if (orientation.isXYswapped()) {
    std::swap(x, y);
  }
  if (orientation.isRightToLeft()) {
    x = x_max - x;
  }
  if (orientation.isBottomToTop()) {
    y = y_max - y;
  }
}

void TransformRectPointers(Orientation orientation, int16_t x_max,
                           int16_t y_max, int16_t*& x0, int16_t*& y0,
                           int16_t*& x1, int16_t*& y1) {
  if (orientation.isXYswapped()) {
    std::swap(x0, y0);
    std::swap(x1, y1);
  }
  if (orientation.isRightToLeft()) {
    *x0 = x_max - *x0;
    if (x0 != x1) {
      *x1 = x_max - *x1;
      std::swap(x0, x1);
    }
  }
  if (orientation.isBottomToTop()) {
    *y0 = y_max - *y0;
    if (y0 != y1) {
      *y1 = y_max - *y1;
      std::swap(y0, y1);
    }
  }
}

void TransformRectValues(Orientation orientation, int16_t x_max, int16_t y_max,
                         int16_t& x0, int16_t& y0, int16_t& x1, int16_t& y1) {
  TransformPoint(orientation, x_max, y_max, x0, y0);
  TransformPoint(orientation, x_max, y_max, x1, y1);
  if (x0 > x1) std::swap(x0, x1);
  if (y0 > y1) std::swap(y0, y1);
}

}  // namespace

TEST(OffscreenOrienterTest, OrientPixelsAllOrientations) {
  Orienter orienter(11, 13);
  for (Orientation orientation : AllOrientations()) {
    orienter.setOrientation(orientation);

    int16_t x[4] = {1, 2, 5, 8};
    int16_t y[4] = {0, 3, 6, 11};

    int16_t expected_x[4] = {1, 2, 5, 8};
    int16_t expected_y[4] = {0, 3, 6, 11};
    for (int i = 0; i < 4; ++i) {
      TransformPoint(orientation, 10, 12, expected_x[i], expected_y[i]);
    }

    int16_t* x_ptr = x;
    int16_t* y_ptr = y;
    orienter.orientPixels(x_ptr, y_ptr, 4);

    for (int i = 0; i < 4; ++i) {
      EXPECT_EQ(x_ptr[i], expected_x[i]);
      EXPECT_EQ(y_ptr[i], expected_y[i]);
    }
  }
}

TEST(OffscreenOrienterTest, OrientPixelsAliasedXYIsRejected) {
  Orienter orienter(11, 13);
  for (Orientation orientation : AllOrientations()) {
    EXPECT_DEATH(
        {
          int16_t values[3];
          values[0] = 1;
          values[1] = 2;
          values[2] = 3;
          int16_t* ptr = values;
          orienter.setOrientation(orientation);
          orienter.orientPixels(ptr, ptr, 3);
        },
        "orientPixels requires distinct x and y buffers");
  }
}

TEST(OffscreenOrienterTest, OrientRectAllOrientations) {
  Orienter orienter(11, 13);
  struct RectCase {
    int16_t x0, y0, x1, y1;
  };
  std::array<RectCase, 5> cases = {{{1, 2, 6, 9},
                                    {4, 4, 4, 4},
                                    {2, 1, 2, 10},
                                    {0, 8, 9, 8},
                                    {0, 0, 10, 10}}};
  for (Orientation orientation : AllOrientations()) {
    orienter.setOrientation(orientation);
    for (const auto& c : cases) {
      int16_t x0 = c.x0, y0 = c.y0, x1 = c.x1, y1 = c.y1;
      int16_t ex0 = c.x0, ey0 = c.y0, ex1 = c.x1, ey1 = c.y1;
      TransformRectValues(orientation, 10, 12, ex0, ey0, ex1, ey1);

      orienter.orientRect(x0, y0, x1, y1);

      EXPECT_EQ(x0, ex0);
      EXPECT_EQ(y0, ey0);
      EXPECT_EQ(x1, ex1);
      EXPECT_EQ(y1, ey1);
    }
  }
}

TEST(OffscreenOrienterTest, OrientRectEqualXNotAliasedAllOrientations) {
  Orienter orienter(11, 13);
  struct RectCase {
    int16_t x, y0, y1;
  };
  std::array<RectCase, 4> cases = {
      {{0, 0, 10}, {2, 1, 10}, {5, 3, 7}, {10, 0, 10}}};

  for (Orientation orientation : AllOrientations()) {
    orienter.setOrientation(orientation);
    for (const auto& c : cases) {
      int16_t x0 = c.x;
      int16_t x1 = c.x;
      int16_t y0 = c.y0;
      int16_t y1 = c.y1;

      int16_t ex0 = c.x;
      int16_t ex1 = c.x;
      int16_t ey0 = c.y0;
      int16_t ey1 = c.y1;
      TransformRectValues(orientation, 10, 12, ex0, ey0, ex1, ey1);

      orienter.orientRect(x0, y0, x1, y1);

      EXPECT_EQ(x0, ex0);
      EXPECT_EQ(x1, ex1);
      EXPECT_EQ(y0, ey0);
      EXPECT_EQ(y1, ey1);
    }
  }
}

TEST(OffscreenOrienterTest, OrientRectCrossAxisAliasRejected) {
  Orienter orienter(11, 13);
  for (Orientation orientation : AllOrientations()) {
    EXPECT_DEATH(
        {
          int16_t shared = 1;
          int16_t x1 = 8;
          int16_t y1 = 9;
          orienter.setOrientation(orientation);
          orienter.orientRect(shared, shared, x1, y1);
        },
        "OrientRects disallows cross-axis aliasing");

    EXPECT_DEATH(
        {
          int16_t x0 = 1;
          int16_t y0 = 2;
          int16_t shared = 9;
          orienter.setOrientation(orientation);
          orienter.orientRect(x0, y0, shared, shared);
        },
        "OrientRects disallows cross-axis aliasing");
  }
}

TEST(OffscreenOrienterTest, OrientRectsAllOrientationsNoAlias) {
  Orienter orienter(11, 13);
  for (Orientation orientation : AllOrientations()) {
    orienter.setOrientation(orientation);

    int16_t x0 = 1, y0 = 2, x1 = 6, y1 = 9;
    int16_t rx0 = x0, ry0 = y0, rx1 = x1, ry1 = y1;

    int16_t* qx0 = &rx0;
    int16_t* qy0 = &ry0;
    int16_t* qx1 = &rx1;
    int16_t* qy1 = &ry1;
    TransformRectPointers(orientation, 10, 12, qx0, qy0, qx1, qy1);

    int16_t* px0 = &x0;
    int16_t* py0 = &y0;
    int16_t* px1 = &x1;
    int16_t* py1 = &y1;
    orienter.orientRects(px0, py0, px1, py1, 1);

    EXPECT_EQ(*px0, *qx0);
    EXPECT_EQ(*py0, *qy0);
    EXPECT_EQ(*px1, *qx1);
    EXPECT_EQ(*py1, *qy1);
  }
}

TEST(OffscreenOrienterTest, OrientRectsAllOrientationsAliasedXAxisEndpoints) {
  Orienter orienter(11, 13);
  for (Orientation orientation : AllOrientations()) {
    orienter.setOrientation(orientation);

    int16_t x = 4, y0 = 2, y1 = 9;
    int16_t rx = x, ry0 = y0, ry1 = y1;

    int16_t* qx0 = &rx;
    int16_t* qy0 = &ry0;
    int16_t* qx1 = &rx;
    int16_t* qy1 = &ry1;
    TransformRectPointers(orientation, 10, 12, qx0, qy0, qx1, qy1);

    int16_t* px0 = &x;
    int16_t* py0 = &y0;
    int16_t* px1 = &x;
    int16_t* py1 = &y1;
    orienter.orientRects(px0, py0, px1, py1, 1);

    EXPECT_EQ(*px0, *qx0);
    EXPECT_EQ(*py0, *qy0);
    EXPECT_EQ(*px1, *qx1);
    EXPECT_EQ(*py1, *qy1);
  }
}

TEST(OffscreenOrienterTest, OrientRectsAllOrientationsAliasedYAxisEndpoints) {
  Orienter orienter(11, 13);
  for (Orientation orientation : AllOrientations()) {
    orienter.setOrientation(orientation);

    int16_t y = 7, x0 = 1, x1 = 8;
    int16_t ry = y, rx0 = x0, rx1 = x1;

    int16_t* qx0 = &rx0;
    int16_t* qy0 = &ry;
    int16_t* qx1 = &rx1;
    int16_t* qy1 = &ry;
    TransformRectPointers(orientation, 10, 12, qx0, qy0, qx1, qy1);

    int16_t* px0 = &x0;
    int16_t* py0 = &y;
    int16_t* px1 = &x1;
    int16_t* py1 = &y;
    orienter.orientRects(px0, py0, px1, py1, 1);

    EXPECT_EQ(*px0, *qx0);
    EXPECT_EQ(*py0, *qy0);
    EXPECT_EQ(*px1, *qx1);
    EXPECT_EQ(*py1, *qy1);
  }
}

TEST(OffscreenOrienterTest, OrientRectsAllOrientationsAllFourAliased) {
  Orienter orienter(11, 13);
  for (Orientation orientation : AllOrientations()) {
    EXPECT_DEATH(
        {
          int16_t v = 5;
          int16_t* p = &v;
          orienter.setOrientation(orientation);
          orienter.orientRects(p, p, p, p, 1);
        },
        "orientRects disallows cross-axis aliasing");
  }
}

TEST(OffscreenOrienterTest,
     OrientRectsCrossAxisAliasRejectedUnlessAllFourAliased) {
  Orienter orienter(11, 13);
  for (Orientation orientation : AllOrientations()) {
    EXPECT_DEATH(
        {
          int16_t x0 = 1;
          int16_t x1 = 8;
          int16_t y1 = 9;
          int16_t* px0 = &x0;
          int16_t* py0 = &x0;
          int16_t* px1 = &x1;
          int16_t* py1 = &y1;
          orienter.setOrientation(orientation);
          orienter.orientRects(px0, py0, px1, py1, 1);
        },
        "orientRects disallows cross-axis aliasing");

    EXPECT_DEATH(
        {
          int16_t x0 = 1;
          int16_t y0 = 2;
          int16_t y1 = 9;
          int16_t* px0 = &x0;
          int16_t* py0 = &y0;
          int16_t* px1 = &y1;
          int16_t* py1 = &y1;
          orienter.setOrientation(orientation);
          orienter.orientRects(px0, py0, px1, py1, 1);
        },
        "orientRects disallows cross-axis aliasing");
  }
}

}  // namespace internal
}  // namespace roo_display
