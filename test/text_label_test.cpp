#include "roo_display/ui/text_label.h"

#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/font/font.h"
#include "roo_fonts/NotoSerif_Italic/12.h"
#include "testing_drawable.h"

using namespace testing;

namespace roo_display {

const Font& font12() { return font_NotoSerif_Italic_12(); }

TEST(SmoothFontTest, FontMetrics) {
  EXPECT_EQ(15, font12().metrics().linespace());
  EXPECT_EQ(10, font12().metrics().ascent());
  EXPECT_EQ(-3, font12().metrics().descent());
  EXPECT_EQ(2, font12().metrics().linegap());
  EXPECT_EQ(-3, font12().metrics().glyphXMin());
  EXPECT_EQ(-2, font12().metrics().glyphYMin());
  EXPECT_EQ(14, font12().metrics().glyphXMax());
  EXPECT_EQ(12, font12().metrics().glyphYMax());
  EXPECT_EQ(18, font12().metrics().maxWidth());
  EXPECT_EQ(15, font12().metrics().maxHeight());
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

TEST(StringViewLabel, SimpleTextNoBackground) {
  FakeScreen<Argb4444> screen(26, 18, color::Black);
  StringViewLabel label("Aftp", font12(), color::White);
  screen.Draw(label, 2, 14);
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

}  // namespace roo_display
