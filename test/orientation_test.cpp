#include "roo_display/core/orientation.h"
#include "gtest/gtest.h"

namespace roo_display {

std::ostream& operator<<(std::ostream& os, Orientation orientation) {
  return os << orientation.asString();
}

TEST(Orientation, Default) {
  EXPECT_EQ(Orientation(), Orientation::Default());
  EXPECT_EQ(Orientation::Default(), Orientation::RightDown());
}

TEST(Orientation, RightRotations) {
  Orientation o = Orientation::RightDown();
  o = o.rotateRight();
  EXPECT_EQ(Orientation::DownLeft(), o);
  o = o.rotateRight();
  EXPECT_EQ(Orientation::LeftUp(), o);
  o = o.rotateRight();
  EXPECT_EQ(Orientation::UpRight(), o);
  o = o.rotateRight();
  EXPECT_EQ(Orientation::RightDown(), o);
  o = o.flipHorizontally();
  EXPECT_EQ(Orientation::LeftDown(), o);
  o = o.rotateRight();
  EXPECT_EQ(Orientation::UpLeft(), o);
  o = o.rotateRight();
  EXPECT_EQ(Orientation::RightUp(), o);
  o = o.rotateRight();
  EXPECT_EQ(Orientation::DownRight(), o);
  o = o.rotateRight();
  EXPECT_EQ(Orientation::LeftDown(), o);
}

TEST(Orientation, LeftRotations) {
  Orientation o = Orientation::RightDown();
  o = o.rotateLeft();
  EXPECT_EQ(Orientation::UpRight(), o);
  o = o.rotateLeft();
  EXPECT_EQ(Orientation::LeftUp(), o);
  o = o.rotateLeft();
  EXPECT_EQ(Orientation::DownLeft(), o);
  o = o.rotateLeft();
  EXPECT_EQ(Orientation::RightDown(), o);
  o = o.flipHorizontally();
  EXPECT_EQ(Orientation::LeftDown(), o);
  o = o.rotateLeft();
  EXPECT_EQ(Orientation::DownRight(), o);
  o = o.rotateLeft();
  EXPECT_EQ(Orientation::RightUp(), o);
  o = o.rotateLeft();
  EXPECT_EQ(Orientation::UpLeft(), o);
  o = o.rotateLeft();
  EXPECT_EQ(Orientation::LeftDown(), o);
}

TEST(Orientation, UpsideDownRotations) {
  Orientation o = Orientation::RightDown();
  o = o.rotateUpsideDown();
  EXPECT_EQ(Orientation::LeftUp(), o);
  o = o.rotateUpsideDown();
  EXPECT_EQ(Orientation::RightDown(), o);
  o = Orientation::DownRight();
  o = o.rotateUpsideDown();
  EXPECT_EQ(Orientation::UpLeft(), o);
  o = o.rotateUpsideDown();
  EXPECT_EQ(Orientation::DownRight(), o);
  o = Orientation::LeftDown();
  o = o.rotateUpsideDown();
  EXPECT_EQ(Orientation::RightUp(), o);
  o = o.rotateUpsideDown();
  EXPECT_EQ(Orientation::LeftDown(), o);
  o = Orientation::DownLeft();
  o = o.rotateUpsideDown();
  EXPECT_EQ(Orientation::UpRight(), o);
  o = o.rotateUpsideDown();
  EXPECT_EQ(Orientation::DownLeft(), o);
}

TEST(Orientation, Rotations) {
  Orientation o = Orientation::RightDown();
  o = o.rotateClockwise(17);
  EXPECT_EQ(Orientation::DownLeft(), o);
  o = o.rotateClockwise(-2);
  EXPECT_EQ(Orientation::UpRight(), o);
  o = o.rotateCounterClockwise(6);
  EXPECT_EQ(Orientation::DownLeft(), o);
  o = o.rotateCounterClockwise(17);
  EXPECT_EQ(Orientation::RightDown(), o);
}

void TestFlips(Orientation o, Orientation hflip, Orientation vflip,
               Orientation xflip, Orientation yflip) {
  EXPECT_EQ(hflip, o.flipHorizontally());
  EXPECT_EQ(vflip, o.flipVertically());
  EXPECT_EQ(xflip, o.flipX());
  EXPECT_EQ(yflip, o.flipY());
}

TEST(Orientation, Flips) {
  TestFlips(Orientation::RightDown(), Orientation::LeftDown(),
            Orientation::RightUp(), Orientation::LeftDown(),
            Orientation::RightUp());
  TestFlips(Orientation::DownRight(), Orientation::DownLeft(),
            Orientation::UpRight(), Orientation::UpRight(),
            Orientation::DownLeft());
  TestFlips(Orientation::LeftDown(), Orientation::RightDown(),
            Orientation::LeftUp(), Orientation::RightDown(),
            Orientation::LeftUp());
  TestFlips(Orientation::DownLeft(), Orientation::DownRight(),
            Orientation::UpLeft(), Orientation::UpLeft(),
            Orientation::DownRight());
  TestFlips(Orientation::RightUp(), Orientation::LeftUp(),
            Orientation::RightDown(), Orientation::LeftUp(),
            Orientation::RightDown());
  TestFlips(Orientation::UpRight(), Orientation::UpLeft(),
            Orientation::DownRight(), Orientation::DownRight(),
            Orientation::UpLeft());
  TestFlips(Orientation::LeftUp(), Orientation::RightUp(),
            Orientation::LeftDown(), Orientation::RightUp(),
            Orientation::LeftDown());
  TestFlips(Orientation::UpLeft(), Orientation::UpRight(),
            Orientation::DownLeft(), Orientation::DownLeft(),
            Orientation::UpRight());
}

void TestProperties(Orientation o, Orientation::HorizontalDirection hdir,
                    Orientation::VerticalDirection vdir, bool xy_swapped,
                    bool mirrored) {
  EXPECT_EQ(hdir, o.getHorizontalDirection());
  EXPECT_EQ(vdir, o.getVerticalDirection());
  EXPECT_EQ(xy_swapped, o.isXYswapped());
  EXPECT_EQ(mirrored, o.isMirrored());
}

TEST(Orientation, Properties) {
  TestProperties(Orientation::RightDown(), Orientation::LEFT_TO_RIGHT,
                 Orientation::TOP_TO_BOTTOM, false, false);
  TestProperties(Orientation::DownRight(), Orientation::LEFT_TO_RIGHT,
                 Orientation::TOP_TO_BOTTOM, true, true);
  TestProperties(Orientation::LeftDown(), Orientation::RIGHT_TO_LEFT,
                 Orientation::TOP_TO_BOTTOM, false, true);
  TestProperties(Orientation::DownLeft(), Orientation::RIGHT_TO_LEFT,
                 Orientation::TOP_TO_BOTTOM, true, false);
  TestProperties(Orientation::RightUp(), Orientation::LEFT_TO_RIGHT,
                 Orientation::BOTTOM_TO_TOP, false, true);
  TestProperties(Orientation::UpRight(), Orientation::LEFT_TO_RIGHT,
                 Orientation::BOTTOM_TO_TOP, true, false);
  TestProperties(Orientation::LeftUp(), Orientation::RIGHT_TO_LEFT,
                 Orientation::BOTTOM_TO_TOP, false, false);
  TestProperties(Orientation::UpLeft(), Orientation::RIGHT_TO_LEFT,
                 Orientation::BOTTOM_TO_TOP, true, true);
}

}  // namespace roo_display