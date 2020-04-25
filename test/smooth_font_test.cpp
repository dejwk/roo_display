
#include "roo_display.h"
#include "roo_display/core/color.h"
#include "roo_display/font/font.h"
#include "roo_smooth_fonts/NotoSans_Italic/12.h"
#include "testing_drawable.h"

using namespace testing;

namespace roo_display {

const Font &font() { return font_NotoSans_Italic_12(); }

class Label : public Drawable {
 public:
  Label(const string &label) : label_(label) {}

 private:
  void drawTo(const Surface &s) const override {
    font().drawHorizontalString(s, (const uint8_t *)label_.c_str(),
                                label_.length(), color::White);
  }

  Box extents() const override {
    return font()
        .getHorizontalStringMetrics((const uint8_t *)label_.c_str(),
                                    label_.length())
        .screen_extents();
  }

  std::string label_;
};

TEST(SmoothFontTest, FontMetrics) {
  EXPECT_EQ(16, font().metrics().linespace());
  EXPECT_EQ(10, font().metrics().ascent());
  EXPECT_EQ(-3, font().metrics().descent());
  EXPECT_EQ(3, font().metrics().linegap());
  EXPECT_EQ(-3, font().metrics().glyphXMin());
  EXPECT_EQ(-2, font().metrics().glyphYMin());
  EXPECT_EQ(12, font().metrics().glyphXMax());
  EXPECT_EQ(13, font().metrics().glyphYMax());
  EXPECT_EQ(16, font().metrics().maxWidth());
  EXPECT_EQ(16, font().metrics().maxHeight());
}

TEST(SmoothFontTest, HorizontalStringMetrics) {
  string text = "Aftp";
  GlyphMetrics metrics =
      font().getHorizontalStringMetrics((const uint8_t *)text.c_str(), 4);
  EXPECT_EQ(-1, metrics.bearingX());
  EXPECT_EQ(-1, metrics.lsb());
  EXPECT_EQ(-1, metrics.glyphXMin());

  EXPECT_EQ(21, metrics.glyphXMax());

  EXPECT_EQ(10, metrics.bearingY());
  EXPECT_EQ(0, metrics.rsb());

  EXPECT_EQ(-2, metrics.glyphYMin());
  EXPECT_EQ(10, metrics.glyphYMax());

  EXPECT_EQ(23, metrics.width());
  EXPECT_EQ(13, metrics.height());

  EXPECT_EQ(22, metrics.advance());
}

TEST(SmoothFontTest, SimpleTextNoBackground) {
  FakeScreen<Argb4444> screen(26, 18, color::Black);
  screen.Draw(Label("Aftp"), 2, 14);
  EXPECT_THAT(screen, MatchesContent(Grayscale4(), 26, 18,
                                     "                          "
                                     "                          "
                                     "                          "
                                     "                          "
                                     "           3DE2           "
                                     "     4*2   B5  5          "
                                     "     CC4   *  1C          "
                                     "    5A96 3C*E5D*E4688EA   "
                                     "    C377  6A  97  9D4 D3  "
                                     "   5A 69  97  C4  C7  B5  "
                                     "   DEEEB  C3  E1  *1  D3  "
                                     "  6B114D  *1 2D  2E  2E   "
                                     "  D3  1* 3D  4C  5*2 B8   "
                                     " 6B    E16A  1CE397CE9    "
                                     "         97      C4       "  // Baseline.
                                     "         D2      E1       "
                                     "       4E8      2D        "
                                     "                          "));
}

TEST(SmoothFontTest, SimpleTextWithBackground) {
  FakeScreen<Argb4444> screen(26, 18, Color(0xFF111111));
  screen.Draw(Label("Aftp"), 2, 14, color::Black, FILL_MODE_RECTANGLE);
  // We expect that the background will fill the bbox that extends vertically
  // from font.metrics().glyphYMin() to font.metrics().glyphYMax() (in FreeType
  // coordinates). In other words, in screen coordinates, we expect that the top
  // of the background rect will have y coordinate = y - glyphYMax() = 14 - 13
  // = 1, and the bottom of the background will have y coordinate =  y -
  // glyphYMin() = 14 + 2 = 16.
  EXPECT_THAT(screen, MatchesContent(Grayscale4(), 26, 18,
                                     "11111111111111111111111111"
                                     "1                       11"
                                     "1                       11"
                                     "1                       11"
                                     "1          3DE2         11"
                                     "1    4*2   B5  5        11"
                                     "1    CC4   *  1C        11"
                                     "1   5A96 3C*E5D*E4688EA 11"
                                     "1   C377  6A  97  9D4 D311"
                                     "1  5A 69  97  C4  C7  B511"
                                     "1  DEEEB  C3  E1  *1  D311"
                                     "1 6B114D  *1 2D  2E  2E 11"
                                     "1 D3  1* 3D  4C  5*2 B8 11"
                                     "16B    E16A  1CE397CE9  11"
                                     "1        97      C4     11"
                                     "1        D2      E1     11"
                                     "1      4E8      1D      11"
                                     "11111111111111111111111111"));
}

TEST(SmoothFontTest, ClippedTextWithBackground) {
  FakeScreen<Argb4444> screen(26, 18, Color(0xFF111111));
  screen.Draw(Label("Aftp"), 2, 14, Box(6, 3, 17, 20), color::Black,
              FILL_MODE_RECTANGLE);
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
                                     "111111            11111111"
                                     "111111     3DE2   11111111"
                                     "111111*2   B5  5  11111111"
                                     "111111C4   *  1C  11111111"
                                     "11111196 3C*E5D*E411111111"
                                     "11111177  6A  97  11111111"
                                     "11111169  97  C4  11111111"
                                     "111111EB  C3  E1  11111111"
                                     "1111114D  *1 2D  211111111"
                                     "1111111* 3D  4C  511111111"
                                     "111111 E16A  1CE3911111111"
                                     "111111   97      C11111111"
                                     "111111   D2      E11111111"
                                     "111111 4E8      1D11111111"
                                     "11111111111111111111111111"));
}

}  // namespace roo_display
