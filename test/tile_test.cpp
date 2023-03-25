
#include "roo_display/ui/tile.h"

#include "roo_display.h"
#include "roo_display/color/color.h"
#include "testing_drawable.h"

using namespace testing;

namespace roo_display {

TEST(TileTest, Trivial) {
  FakeScreen<Argb8888> screen(5, 3, color::Black);
  auto interior = SolidRect(0, 0, 2, 1, color::White);
  Tile tile(&interior, Box(0, 0, 2, 1), kLeft | kTop);
  screen.Draw(tile, 0, 0);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 3,
                                     "***  "
                                     "***  "
                                     "     "));
}

TEST(TileTest, CenteredBasic) {
  FakeScreen<Argb4444> screen(10, 10, color::Black);
  auto interior = SolidRect(0, 0, 2, 1, color::Gray);
  Tile tile(&interior, Box(3, 2, 7, 6), kCenter | kMiddle,
            color::White);
  screen.Draw(tile, 0, 0);
  EXPECT_THAT(screen, MatchesContent(Grayscale4(), 10, 10,
                                     "          "
                                     "          "
                                     "   *****  "
                                     "   *****  "
                                     "   *777*  "
                                     "   *777*  "
                                     "   *****  "
                                     "          "
                                     "          "
                                     "          "));
}

TEST(TileTest, CenteredOffset) {
  FakeScreen<Argb4444> screen(10, 10, color::Black);
  auto interior = SolidRect(32, -15, 34, -14, color::Gray);
  Tile tile(&interior, Box(3, 2, 8, 6), kCenter | kMiddle,
            color::White);
  screen.Draw(tile, -1, 1);
  EXPECT_THAT(screen, MatchesContent(Grayscale4(), 10, 10,
                                     "          "
                                     "          "
                                     "          "
                                     "  ******  "
                                     "  *777**  "
                                     "  *777**  "
                                     "  ******  "
                                     "  ******  "
                                     "          "
                                     "          "));
}

TEST(TileTest, BottomRightOffset) {
  FakeScreen<Argb4444> screen(10, 10, color::Black);
  auto interior = SolidRect(32, -15, 34, -14, color::Gray);
  Tile tile(&interior, Box(3, 2, 8, 6), kRight | kBottom,
            color::White);
  screen.Draw(tile, -1, 1);
  EXPECT_THAT(screen, MatchesContent(Grayscale4(), 10, 10,
                                     "          "
                                     "          "
                                     "          "
                                     "  ******  "
                                     "  ******  "
                                     "  ******  "
                                     "  ***777  "
                                     "  ***777  "
                                     "          "
                                     "          "));
}

TEST(TileTest, InteriorStaysClipped) {
  FakeScreen<Argb4444> screen(10, 10, color::Black);
  auto interior = SolidRect(7, 7, 30, 30, color::Gray);
  Tile tile(&interior, Box(3, 2, 8, 6), kCenter | kMiddle,
            color::White);
  screen.Draw(tile, 0, 0);
  EXPECT_THAT(screen, MatchesContent(Grayscale4(), 10, 10,
                                     "          "
                                     "          "
                                     "   777777 "
                                     "   777777 "
                                     "   777777 "
                                     "   777777 "
                                     "   777777 "
                                     "          "
                                     "          "
                                     "          "));
}

}  // namespace roo_display
