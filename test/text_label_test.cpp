#include "roo_display/ui/text_label.h"
#include "roo_display.h"
#include "roo_display/core/color.h"
#include "roo_display/font/font.h"
#include "roo_smooth_fonts/NotoSans_Italic/12.h"
#include "testing_drawable.h"

using namespace testing;

namespace roo_display {

const Font& font12() { return font_NotoSans_Italic_12(); }

TEST(SmoothFontTest, FontMetrics) {
  EXPECT_EQ(16, font12().metrics().linespace());
  EXPECT_EQ(10, font12().metrics().ascent());
  EXPECT_EQ(-3, font12().metrics().descent());
  EXPECT_EQ(3, font12().metrics().linegap());
  EXPECT_EQ(-3, font12().metrics().glyphXMin());
  EXPECT_EQ(-2, font12().metrics().glyphYMin());
  EXPECT_EQ(12, font12().metrics().glyphXMax());
  EXPECT_EQ(13, font12().metrics().glyphYMax());
  EXPECT_EQ(16, font12().metrics().maxWidth());
  EXPECT_EQ(16, font12().metrics().maxHeight());
}

// Absolute extents.

TEST(TextLabel, SimpleTextNoBackground) {
  FakeScreen<Argb4444> screen(26, 18, color::Black);
  TextLabel label("Aftp", font12(), color::White);
  screen.Draw(label, 2, 14);
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

TEST(StringViewLabel, SimpleTextNoBackground) {
  FakeScreen<Argb4444> screen(26, 18, color::Black);
  StringViewLabel label("Aftp", font12(), color::White);
  screen.Draw(label, 2, 14);
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

}  // namespace roo_display
