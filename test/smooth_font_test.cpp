
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

TEST(SmoothFontTest, KerningConsistency) {
  EXPECT_EQ(FontProperties::Kerning::kPairs, font().properties().kerning());

  struct Pair {
    char left;
    char right;
  };

  const Pair pairs[] = {
      {'A', 'V'}, {'A', 'W'}, {'A', 'Y'}, {'T', 'a'},
      {'T', 'o'}, {'V', 'a'}, {'W', 'a'}, {'Y', 'o'},
  };

  bool found_nonzero = false;
  for (const Pair& pair : pairs) {
    string left(1, pair.left);
    string right(1, pair.right);
    string together;
    together.push_back(pair.left);
    together.push_back(pair.right);

    int left_advance = font().getHorizontalStringMetrics(left).advance();
    int right_advance = font().getHorizontalStringMetrics(right).advance();
    int pair_advance = font().getHorizontalStringMetrics(together).advance();

    int observed_kerning = left_advance + right_advance - pair_advance;
    int reported_kerning =
        font().getKerning(static_cast<char32_t>(pair.left),
                          static_cast<char32_t>(pair.right));
    EXPECT_EQ(observed_kerning, reported_kerning)
        << "pair: " << pair.left << pair.right;
    if (observed_kerning != 0) {
      found_nonzero = true;
    }
  }

  // Ensure whitespace still reports no kerning.
  EXPECT_EQ(0, font().getKerning(U'A', U' '));
  EXPECT_EQ(0, font().getKerning(U' ', U'V'));

  // This font should contain at least some non-zero kerning pairs.
  EXPECT_TRUE(found_nonzero);
}

TEST(SmoothFontTest, SpaceGlyphMetrics) {
  GlyphMetrics space;
  ASSERT_TRUE(font().getGlyphMetrics(U' ', FontLayout::kHorizontal, &space));
  EXPECT_EQ(0, space.width());
  EXPECT_EQ(0, space.height());
  EXPECT_EQ(0, space.glyphXMin());
  EXPECT_EQ(-1, space.glyphXMax());

  int expected_space_advance = font().getHorizontalStringMetrics(" ").advance();
  EXPECT_EQ(expected_space_advance, space.advance());
  EXPECT_GT(space.advance(), 0);
}

// Verifies tracked measurement preserves empty and single-glyph geometry,
// while moving only inter-glyph boundaries.
TEST(SmoothFontTest, TrackedHorizontalStringMetrics) {
  Font::Options positive_tracking;
  positive_tracking.setTrackingPx(2);
  Font::Options negative_tracking;
  negative_tracking.setTrackingPx(-1);

  EXPECT_EQ(sizeof(int16_t), sizeof(Font::Options));
  EXPECT_EQ(2, positive_tracking.trackingPx());
  EXPECT_EQ(-1, negative_tracking.trackingPx());

  GlyphMetrics empty = font().getHorizontalStringMetrics("");
  GlyphMetrics tracked_empty =
      font().getHorizontalStringMetrics("", positive_tracking);
  EXPECT_EQ(empty.glyphXMin(), tracked_empty.glyphXMin());
  EXPECT_EQ(empty.glyphXMax(), tracked_empty.glyphXMax());
  EXPECT_EQ(empty.advance(), tracked_empty.advance());

  GlyphMetrics single = font().getHorizontalStringMetrics("A");
  GlyphMetrics tracked_single =
      font().getHorizontalStringMetrics("A", positive_tracking);
  EXPECT_EQ(single.glyphXMin(), tracked_single.glyphXMin());
  EXPECT_EQ(single.glyphXMax(), tracked_single.glyphXMax());
  EXPECT_EQ(single.advance(), tracked_single.advance());

  GlyphMetrics untracked_pair = font().getHorizontalStringMetrics("AV");
  GlyphMetrics tracked_pair =
      font().getHorizontalStringMetrics("AV", positive_tracking);
  EXPECT_NE(0, font().getKerning(U'A', U'V'));
  EXPECT_EQ(untracked_pair.advance() + positive_tracking.trackingPx(),
            tracked_pair.advance());

  GlyphMetrics untracked_whitespace = font().getHorizontalStringMetrics("A V");
  EXPECT_EQ(
      untracked_whitespace.advance() + 2 * positive_tracking.trackingPx(),
      font().getHorizontalStringMetrics("A V", positive_tracking).advance());
  EXPECT_EQ(
      untracked_whitespace.advance() + 2 * negative_tracking.trackingPx(),
      font().getHorizontalStringMetrics("A V", negative_tracking).advance());
}

// Verifies tracked per-glyph origins include prior boundaries and returned
// advances include only the following boundary.
TEST(SmoothFontTest, TrackedHorizontalStringGlyphMetrics) {
  Font::Options tracking;
  tracking.setTrackingPx(2);

  GlyphMetrics untracked[3];
  GlyphMetrics tracked[3];
  GlyphMetrics offset[1];
  ASSERT_EQ(3u, font().getHorizontalStringGlyphMetrics("A V", untracked, 0, 3));
  ASSERT_EQ(3u, font().getHorizontalStringGlyphMetrics("A V", tracked, 0, 3,
                                                       tracking));
  ASSERT_EQ(1u, font().getHorizontalStringGlyphMetrics("A V", offset, 1, 1,
                                                       tracking));

  EXPECT_EQ(untracked[0].glyphXMin(), tracked[0].glyphXMin());
  EXPECT_EQ(untracked[1].glyphXMin() + tracking.trackingPx(),
            tracked[1].glyphXMin());
  EXPECT_EQ(untracked[2].glyphXMin() + 2 * tracking.trackingPx(),
            tracked[2].glyphXMin());
  EXPECT_EQ(untracked[0].advance() + tracking.trackingPx(),
            tracked[0].advance());
  EXPECT_EQ(untracked[1].advance() + 2 * tracking.trackingPx(),
            tracked[1].advance());
  EXPECT_EQ(untracked[2].advance() + 2 * tracking.trackingPx(),
            tracked[2].advance());
  EXPECT_EQ(tracked[1].glyphXMin(), offset[0].glyphXMin());
  EXPECT_EQ(tracked[1].advance(), offset[0].advance());
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
