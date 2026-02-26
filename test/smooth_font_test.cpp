
#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/font/font.h"
#include "roo_fonts/NotoSerif_Italic/12.h"
#include "testing_drawable.h"

using namespace testing;

namespace roo_display {

const Font &font() { return font_NotoSerif_Italic_12(); }

class Label : public Drawable {
 public:
  Label(const string &label) : label_(label) {}

 private:
  void drawTo(const Surface &s) const override {
    font().drawHorizontalString(s, label_, color::White);
  }

  Box extents() const override {
    return font().getHorizontalStringMetrics(label_).screen_extents();
  }

  std::string label_;
};

TEST(SmoothFontTest, FontMetrics) {
  EXPECT_EQ(15, font().metrics().linespace());
  EXPECT_EQ(10, font().metrics().ascent());
  EXPECT_EQ(-3, font().metrics().descent());
  EXPECT_EQ(2, font().metrics().linegap());
  EXPECT_EQ(-3, font().metrics().glyphXMin());
  EXPECT_EQ(-2, font().metrics().glyphYMin());
  EXPECT_EQ(14, font().metrics().glyphXMax());
  EXPECT_EQ(12, font().metrics().glyphYMax());
  EXPECT_EQ(18, font().metrics().maxWidth());
  EXPECT_EQ(15, font().metrics().maxHeight());
}

TEST(SmoothFontTest, HorizontalStringMetrics) {
  string text = "Aftp";
  GlyphMetrics metrics = font().getHorizontalStringMetrics(text);
  EXPECT_EQ(-1, metrics.bearingX());
  EXPECT_EQ(-1, metrics.lsb());
  EXPECT_EQ(-1, metrics.glyphXMin());

  EXPECT_EQ(22, metrics.glyphXMax());

  EXPECT_EQ(10, metrics.bearingY());
  EXPECT_EQ(0, metrics.rsb());

  EXPECT_EQ(-2, metrics.glyphYMin());
  EXPECT_EQ(10, metrics.glyphYMax());

  EXPECT_EQ(24, metrics.width());
  EXPECT_EQ(13, metrics.height());

  EXPECT_EQ(23, metrics.advance());
}

TEST(SmoothFontTest, SimpleTextNoBackground) {
  FakeScreen<Argb4444> screen(26, 18, color::Black);
  screen.Draw(Label("Aftp"), 2, 14);
  EXPECT_THAT(screen, MatchesContent(Grayscale4(), 26, 18,
                                     "                          "
                                     "                          "
                                     "                          "
                                     "                          "
                                     "            2BD8          "
                                     "      5E    B63922        "
                                     "      A*1   *1 2D1        "
                                     "     74E3 2BEB3CEB6EC5BC  "
                                     "    1A C6  6B  6B  6D4 E4 "
                                     "    92 A8  98  98  9A  D5 "
                                     "   3D99CA  C5  C5  C5  *3 "
                                     "   B1  5D  *1  *1  E1 5D  "
                                     "  58   4* 3E  1*  2E 1D5  "
                                     " 7*E4 4E*B6B   BC65DCC6   "
                                     "          A7      97      "  // Baseline.
                                     "         1E2      C5      "
                                     "        7C6       EE2     "
                                     "                          "));
}

TEST(SmoothFontTest, SimpleTextWithBackground) {
  FakeScreen<Argb4444> screen(26, 18, Color(0xFF111111));
  screen.Draw(Label("Aftp"), 2, 14, color::Black, FillMode::kExtents);
  // We expect that the background will fill the bbox that extends vertically
  // from font.metrics().glyphYMin() to font.metrics().glyphYMax() (in FreeType
  // coordinates). In other words, in screen coordinates, we expect that the top
  // of the background rect will have y coordinate = y - glyphYMax() = 14 - 13
  // = 1, and the bottom of the background will have y coordinate =  y -
  // glyphYMin() = 14 + 2 = 16.
  EXPECT_THAT(screen, MatchesContent(Grayscale4(), 26, 18,
                                     "11111111111111111111111111"
                                     "11111111111111111111111111"
                                     "11111111111111111111111111"
                                     "11111111111111111111111111"
                                     "1           2BD8         1"
                                     "1     5E    B63922       1"
                                     "1     A*1   *1 2D1       1"
                                     "1    74E3 2BEB3CEB6EC5BC 1"
                                     "1   1A C6  6B  6B  6D4 E41"
                                     "1   92 A8  98  98  9A  D51"
                                     "1  3D99CA  C5  C5  C5  *31"
                                     "1  B1  5D  *1  *1  E1 5D 1"
                                     "1 58   4* 3E  1*  2E 1D5 1"
                                     "17*E4 4E*B6B   BC65DCC6  1"
                                     "1         A7      97     1"
                                     "1        1E2      C5     1"
                                     "1       7C6       EE2    1"
                                     "11111111111111111111111111"));
}

TEST(SmoothFontTest, ClippedTextWithBackground) {
  FakeScreen<Argb4444> screen(26, 18, Color(0xFF111111));
  screen.Draw(Label("Aftp"), 2, 14, Box(6, 3, 17, 20), color::Black,
              FillMode::kExtents);
  // We expect that the background will fill the bbox that extends vertically
  // from font.metrics().glyphYMin() to font.metrics().glyphYMax() (in FreeType
  // coordinates).
  EXPECT_THAT(screen, MatchesContent(Grayscale4(), 26, 18,
                                     "11111111111111111111111111"
                                     "11111111111111111111111111"
                                     "11111111111111111111111111"
                                     "11111111111111111111111111"
                                     "111111      2BD8  11111111"
                                     "1111115E    B6392211111111"
                                     "111111A*1   *1 2D111111111"
                                     "1111114E3 2BEB3CEB11111111"
                                     "111111 C6  6B  6B 11111111"
                                     "111111 A8  98  98 11111111"
                                     "1111119CA  C5  C5 11111111"
                                     "111111 5D  *1  *1 11111111"
                                     "111111 4* 3E  1*  11111111"
                                     "1111114E*B6B   BC611111111"
                                     "111111    A7      11111111"
                                     "111111   1E2      11111111"
                                     "111111  7C6       11111111"
                                     "11111111111111111111111111"));
}

}  // namespace roo_display
