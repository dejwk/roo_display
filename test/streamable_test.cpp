
#include "roo_display/core/streamable.h"
#include "roo_display/core/color.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

template <typename Streamable>
void Draw(DisplayDevice* output, int16_t x, int16_t y, const Box& clip_box,
          const Streamable& object) {
  output->begin();
  DrawableStreamable<Streamable> drawable(object);
  Surface s(output, x, y, clip_box);
  s.drawObject(drawable);
  output->end();
}

template <typename Streamable>
void Draw(DisplayDevice* output, int16_t x, int16_t y,
          const Streamable& object) {
  Box clip_box(0, 0, output->effective_width() - 1,
                 output->effective_height() - 1);
  Draw(output, x, y, clip_box, object);
}

TEST(Streamable, FilledRect) {
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  StreamableFilledRect rect(2, 3, color::White);
  Draw(&test_screen, 1, 2, rect);
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 6,
                                          "     "
                                          "     "
                                          " **  "
                                          " **  "
                                          " **  "
                                          "     "));
}

TEST(Streamable, ClippedFilledRect) {
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  StreamableFilledRect rect(2, 3, color::White);
  Draw(&test_screen, 1, 2, Box(1, 1, 2, 2), rect);
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 5, 6,
                                          "     "
                                          "     "
                                          " **  "
                                          "     "
                                          "     "
                                          "     "));
}

TEST(Streamable, DrawingArbitraryStreamable) {
  auto input = MakeTestStreamable(Rgb565(), 2, 3,
                                  "1W3 D1R"
                                  "LQD TOL"
                                  "F9F N_N");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(&test_screen, 1, 2, input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ 1W3 D1R ___ ___"
                                          "___ LQD TOL ___ ___"
                                          "___ F9F N_N ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, ClippingFromTop) {
  auto input = MakeTestStreamable(Rgb565(), 2, 3,
                                  "1W3 D1R"
                                  "LQD TOL"
                                  "F9F N_N");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(&test_screen, 1, 2, Box(2, 3, 4, 5), input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ TOL ___ ___"
                                          "___ ___ N_N ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, ClippingFromBottom) {
  auto input = MakeTestStreamable(Rgb565(), 2, 3,
                                  "1W3 D1R"
                                  "LQD TOL"
                                  "F9F N_N");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(&test_screen, 1, 2, Box(0, 0, 1, 3), input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ 1W3 ___ ___ ___"
                                          "___ LQD ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, NestedClipping) {
  auto input = Clipped(Box(1, 1, 6, 6), MakeTestStreamable(Rgb565(), 3, 3,
                                                            "1W3 D1R 2C5"
                                                            "LQD TOL DN_"
                                                            "F9F N_N 117"));
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(&test_screen, 1, 2, Box(0, 0, 2, 4), input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ TOL ___ ___"
                                          "___ ___ N_N ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, NestedClippingToNil) {
  auto input = Clipped(Box(1, 4, 6, 6), MakeTestStreamable(Rgb565(), 2, 3,
                                                            "1W3 D1R"
                                                            "LQD TOL"
                                                            "F9F N_N"));
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(&test_screen, 1, 2, Box(0, 0, 1, 3), input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, Transparency) {
  auto input = MakeTestStreamable(Rgb565WithTransparency(1), 2, 3,
                                  "... D1R"
                                  "LQD TOL"
                                  "F9F ...");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(&test_screen, 1, 2, input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ D1R ___ ___"
                                          "___ LQD TOL ___ ___"
                                          "___ F9F ___ ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, AlphaTransparency) {
  auto input = MakeTestStreamable(Argb4444(), 4, 1, "4488 F678 F1A3 73E3");
  FakeOffscreen<Argb4444> test_screen(6, 1, color::Black);
  Draw(&test_screen, 1, 0, input);
  EXPECT_THAT(test_screen, MatchesContent(Argb4444(), 6, 1,
                                          "F000 F122 F678 F1A3 F161 F000"));
}

}  // namespace roo_display