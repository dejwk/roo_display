
#include "roo_display/core/streamable.h"

#include "roo_display/core/color.h"
#include "testing.h"

// Tests drawing and clipping streamables via their drawTo method, which
// internally creates a stream and possibly clips it using SubRectangleStream.

using namespace testing;

namespace roo_display {

void Draw(DisplayDevice& output, int16_t x, int16_t y, const Box& clip_box,
          const Streamable& object, FillMode fill_mode = FILL_MODE_VISIBLE,
          PaintMode paint_mode = PAINT_MODE_BLEND,
          Color bgcolor = color::Transparent) {
  output.begin();
  Surface s(output, x, y, clip_box, false, bgcolor, fill_mode, paint_mode);
  s.drawObject(object);
  output.end();
}

void Draw(DisplayDevice& output, int16_t x, int16_t y, const Streamable& object,
          FillMode fill_mode = FILL_MODE_VISIBLE,
          PaintMode paint_mode = PAINT_MODE_BLEND,
          Color bgcolor = color::Transparent) {
  Box clip_box(0, 0, output.effective_width() - 1,
               output.effective_height() - 1);
  Draw(output, x, y, clip_box, object, fill_mode, paint_mode, bgcolor);
}

TEST(Streamable, DrawingArbitraryStreamable) {
  auto input = MakeTestStreamable(Rgb565(), 2, 3,
                                  "1W3 D1R"
                                  "LQD TOL"
                                  "F9F N_N");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(test_screen, 1, 2, input);
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
  Draw(test_screen, 1, 2, Box(2, 3, 4, 5), input);
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
  Draw(test_screen, 1, 2, Box(0, 0, 1, 3), input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ 1W3 ___ ___ ___"
                                          "___ LQD ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, ClippingFromSeveralSides) {
  auto input = MakeTestStreamable(Rgb565(), 4, 3,
                                  "1W3 D1R CA4 B11"
                                  "LQD TOL 114 5AF"
                                  "F9F N_N F45 567");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(test_screen, 1, 2, Box(2, 3, 3, 5), input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ TOL 114 ___"
                                          "___ ___ N_N F45 ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, ClippingFromLeft) {
  auto input = MakeTestStreamable(Rgb565(), 4, 3,
                                  "1W3 D1R CA4 B11"
                                  "LQD TOL 114 5AF"
                                  "F9F N_N F45 567");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(test_screen, -2, 2, Box(1, 0, 1, 5), input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ B11 ___ ___ ___"
                                          "___ 5AF ___ ___ ___"
                                          "___ 567 ___ ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, ClippingFromRight) {
  auto input = MakeTestStreamable(Rgb565(), 3, 3,
                                  "1W3 D1R CA4"
                                  "LOD TOL 114"
                                  "F9F N_N F45");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(test_screen, 1, 2, Box(1, 0, 1, 5), input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ 1W3 ___ ___ ___"
                                          "___ LOD ___ ___ ___"
                                          "___ F9F ___ ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, Transparency) {
  auto input = MakeTestStreamable(Rgb565WithTransparency(1), 2, 3,
                                  "... D1R"
                                  "LQD TOL"
                                  "F9F ...");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(test_screen, 1, 2, input);
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
  Draw(test_screen, 1, 0, input);
  EXPECT_THAT(test_screen, MatchesContent(Argb4444(), 6, 1,
                                          "F000 F122 F678 F1A3 F161 F000"));
}

TEST(Streamable, AlphaTransparencyOverriddenReplace) {
  auto input = MakeTestStreamable(Argb4444(), 4, 1, "4488 F678 F1A3 73E3");
  FakeOffscreen<Argb4444> test_screen(6, 1, color::Black);
  Draw(test_screen, 1, 0, input, FILL_MODE_VISIBLE, PAINT_MODE_REPLACE);
  EXPECT_THAT(test_screen, MatchesContent(Argb4444(), 6, 1,
                                          "F000 4488 F678 F1A3 73E3 F000"));
}

TEST(Streamable, AlphaTransparencyFillWithOpaqueBackground) {
  auto input = MakeTestStreamable(Argb4444(), 2, 2, "4488 F678 73E3 0000");
  FakeOffscreen<Argb4444> test_screen(3, 2, color::Black);
  Draw(test_screen, 1, 0, input, FILL_MODE_RECTANGLE, PAINT_MODE_BLEND,
       color::White);
  EXPECT_THAT(test_screen, MatchesContent(Argb4444(), 3, 2,
                                          "F000 FCDD F678"
                                          "F000 F9F9 FFFF"));
}

TEST(Streamable, AlphaTransparencyFillWithTranslucentBackground) {
  auto input = MakeTestStreamable(Argb4444(), 2, 2, "4488 F678 73E3 0000");
  FakeOffscreen<Argb4444> test_screen(3, 2, color::Black);
  Draw(test_screen, 1, 0, input, FILL_MODE_RECTANGLE, PAINT_MODE_BLEND,
       Color(0x7FFFFFFF));
  EXPECT_THAT(test_screen, MatchesContent(Argb4444(), 3, 2,
                                          "F000 F677 F678"
                                          "F000 F5A5 F777"));
}

TEST(Streamable, AlphaTransparencyWriteWithOpaqueBackground) {
  auto input = MakeTestStreamable(Argb4444(), 2, 2, "4488 F678 73E3 0000");
  FakeOffscreen<Argb4444> test_screen(3, 2, color::Black);
  Draw(test_screen, 1, 0, input, FILL_MODE_VISIBLE, PAINT_MODE_BLEND,
       color::White);
  EXPECT_THAT(test_screen, MatchesContent(Argb4444(), 3, 2,
                                          "F000 FCDD F678"
                                          "F000 F9F9 F000"));
}

TEST(Streamable, AlphaTransparencyWriteWithTranslucentBackground) {
  auto input = MakeTestStreamable(Argb4444(), 2, 2, "4488 F678 73E3 0000");
  FakeOffscreen<Argb4444> test_screen(3, 2, color::Black);
  Draw(test_screen, 1, 0, input, FILL_MODE_VISIBLE, PAINT_MODE_BLEND,
       Color(0x7FFFFFFF));
  EXPECT_THAT(test_screen, MatchesContent(Argb4444(), 3, 2,
                                          "F000 F677 F678"
                                          "F000 F5A5 F000"));
}

}  // namespace roo_display
