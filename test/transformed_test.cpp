
#include "roo_display/filter/transformed.h"
#include "roo_display.h"
#include "roo_display/core/color.h"
#include "testing_drawable.h"

using namespace testing;

namespace roo_display {

TEST(Transformed, PositiveShift) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), 3, 2,
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transform().translate(1, 2), &input);
  screen.Draw(transformed, 0, 0);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "     "
                                     "     "
                                     " *** "
                                     " *   "
                                     "     "));
}

TEST(Transformed, NegativeShift) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), 4, 3,
                                "****"
                                "*** "
                                "****");
  TransformedDrawable transformed(Transform().translate(-2, -1), &input);
  screen.Draw(transformed, 0, 0);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "*    "
                                     "**   "
                                     "     "
                                     "     "
                                     "     "));
}

TEST(Transformed, HorizontalFlip) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), 3, 2,
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transform().flipX().translate(2, 0), &input);
  screen.Draw(transformed, 1, 2);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "     "
                                     "     "
                                     " *** "
                                     "   * "
                                     "     "));
}

TEST(Transformed, VerticalFlip) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), 3, 2,
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transform().flipY().translate(0, 1), &input);
  screen.Draw(transformed, 1, 2);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "     "
                                     "     "
                                     " *   "
                                     " *** "
                                     "     "));
}

TEST(Transformed, HorizontalScale) {
  FakeScreen<Rgb565> screen(11, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), 3, 2,
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transform().scale(3, 1), &input);
  screen.Draw(transformed, 1, 2);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 11, 5,
                                     "           "
                                     "           "
                                     " ********* "
                                     " ***       "
                                     "           "));
}

TEST(Transformed, VerticalScale) {
  FakeScreen<Rgb565> screen(5, 9, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), 3, 2,
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transform().scale(1, 3), &input);
  screen.Draw(transformed, 1, 2);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 9,
                                     "     "
                                     "     "
                                     " *** "
                                     " *** "
                                     " *** "
                                     " *   "
                                     " *   "
                                     " *   "
                                     "     "));
}

TEST(Transformed, rotateRight) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), 3, 2,
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transform().rotateRight(), &input);
  screen.Draw(transformed, 2, 2);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "     "
                                     "     "
                                     " **  "
                                     "  *  "
                                     "  *  "));
}

TEST(Transformed, rotateLeft) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), 3, 2,
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transform().rotateLeft(), &input);
  screen.Draw(transformed, 1, 3);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "     "
                                     " *   "
                                     " *   "
                                     " **  "
                                     "     "));
}

TEST(Transformed, rotateUpsideDown) {
  FakeScreen<Rgb565> screen(5, 5, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), 3, 2,
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transform().rotateUpsideDown(), &input);
  screen.Draw(transformed, 3, 2);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 5, 5,
                                     "     "
                                     "   * "
                                     " *** "
                                     "     "
                                     "     "));
}

TEST(Transformed, SwapXY) {
  FakeScreen<Rgb565> screen(2, 3, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), 3, 2,
                                "***"
                                "*  ");
  TransformedDrawable transformed(Transform().swapXY(), &input);
  screen.Draw(transformed, 0, 0);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 2, 3,
                                     "**"
                                     "* "
                                     "* "));
}

TEST(Transformed, Complex) {
  FakeScreen<Rgb565> screen(6, 11, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), 3, 2,
                                "***"
                                "*  ");
  Transform t = Transform().translate(2, 3).scale(-3, -2).rotateRight();
  TransformedDrawable transformed(t, &input);
  screen.Draw(transformed, -5, 15);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 6, 11,
                                     "      "
                                     " **   "
                                     " **   "
                                     " **   "
                                     " **   "
                                     " **   "
                                     " **   "
                                     " **** "
                                     " **** "
                                     " **** "
                                     "      "));
}

TEST(Transformed, ComplexTruncated) {
  FakeScreen<Rgb565> screen(2, 2, color::Black);
  auto input = MakeTestDrawable(WhiteOnBlack(), 3, 2,
                                "***"
                                "*  ");
  Transform t = Transform().translate(2, 3).scale(-3, -2).rotateRight();
  TransformedDrawable transformed(t, &input);
  screen.Draw(transformed, -7, 9);
  EXPECT_THAT(screen, MatchesContent(WhiteOnBlack(), 2, 2,
                                     "* "
                                     "**"));
}

}  // namespace roo_display
