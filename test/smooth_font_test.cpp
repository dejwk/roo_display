
#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/font/font.h"
#include "roo_display/font/font_adafruit_fixed_5x7.h"
#include "roo_fonts/NotoSerif_Italic/12.h"
#include "testing_drawable.h"

using namespace testing;

namespace roo_display {

const Font& font() { return font_NotoSerif_Italic_12(); }

class Label : public Drawable {
 public:
  Label(const string& label) : label_(label) {}

 private:
  void drawTo(const Surface& s) const override {
    font().drawHorizontalString(s, label_, color::White);
  }

  Box extents() const override {
    return font().getHorizontalStringMetrics(label_).screen_extents();
  }

  std::string label_;
};

class TrackingLabel : public Drawable {
 public:
  TrackingLabel(const Font& font, const string& label,
                const Font::Options& options)
      : font_(font), label_(label), options_(options) {}

 private:
  void drawTo(const Surface& s) const override {
    font_.drawHorizontalString(s, label_, color::White, options_);
  }

  Box extents() const override {
    return font_.getHorizontalStringMetrics(label_, options_).screen_extents();
  }

  const Font& font_;
  std::string label_;
  Font::Options options_;
};

// Draws each ASCII glyph at the tracked, kerning-adjusted origin independently.
class PositionedGlyphs : public Drawable {
 public:
  PositionedGlyphs(const Font& font, const string& label, int16_t tracking_px)
      : font_(font), label_(label), tracking_px_(tracking_px) {}

 private:
  void drawTo(const Surface& s) const override {
    int16_t x = 0;
    for (uint32_t i = 0; i < label_.size(); ++i) {
      char32_t code = static_cast<unsigned char>(label_[i]);
      Surface glyph_surface(s);
      glyph_surface.set_dx(s.dx() + x);
      font_.drawGlyph(glyph_surface, code, FontLayout::kHorizontal,
                      color::White);
      if (i + 1 < label_.size()) {
        char32_t next_code = static_cast<unsigned char>(label_[i + 1]);
        GlyphMetrics metrics;
        ASSERT_TRUE(
            font_.getGlyphMetrics(code, FontLayout::kHorizontal, &metrics));
        x += metrics.advance() - font_.getKerning(code, next_code) +
             tracking_px_;
      }
    }
  }

  Box extents() const override {
    Font::Options options;
    options.setTrackingPx(tracking_px_);
    return font_.getHorizontalStringMetrics(label_, options).screen_extents();
  }

  const Font& font_;
  std::string label_;
  int16_t tracking_px_;
};

// Verifies that two same-size test screens have identical pixels.
template <typename ColorMode>
void ExpectSamePixels(const FakeScreen<ColorMode>& actual,
                      const FakeScreen<ColorMode>& expected, uint32_t count) {
  std::unique_ptr<TestColorStream> actual_stream = actual.createRawStream();
  std::unique_ptr<TestColorStream> expected_stream = expected.createRawStream();
  for (uint32_t i = 0; i < count; ++i) {
    EXPECT_EQ(expected_stream->next(), actual_stream->next()) << "pixel " << i;
  }
}

// Returns one pixel from a test screen's row-major raw stream.
template <typename ColorMode>
Color PixelAt(const FakeScreen<ColorMode>& screen, uint32_t width, uint32_t x,
              uint32_t y) {
  std::unique_ptr<TestColorStream> stream = screen.createRawStream();
  Color result;
  for (uint32_t i = 0; i <= y * width + x; ++i) {
    result = stream->next();
  }
  return result;
}

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
    int reported_kerning = font().getKerning(static_cast<char32_t>(pair.left),
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
  EXPECT_EQ(expected_space_advance, font().metrics().defaultSpaceWidth());
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

// Verifies positive and negative tracking preserve kerning while moving the
// rasterized next-glyph origin by the tracked inter-glyph advance.
TEST(SmoothFontTest, TrackedVisibleRenderingMatchesPositionedGlyphs) {
  for (int16_t tracking_px : {int16_t(-2), int16_t(2)}) {
    Font::Options options;
    options.setTrackingPx(tracking_px);
    FakeScreen<Argb4444> actual(20, 14, color::Black);
    FakeScreen<Argb4444> expected(20, 14, color::Black);
    actual.Draw(TrackingLabel(font(), "AV", options), 1, 11);
    expected.Draw(PositionedGlyphs(font(), "AV", tracking_px), 1, 11);
    ExpectSamePixels(actual, expected, 20 * 14);
  }
}

// Verifies positive tracking fills the newly exposed extents gap and respects
// the caller's clip box.
TEST(SmoothFontTest, TrackedExtentsFillAndClipPositiveGap) {
  Font::Options options;
  options.setTrackingPx(2);
  TrackingLabel label(font(), "A ", options);
  int tracking_gap_x = font().getHorizontalStringMetrics("A").advance() +
                       options.trackingPx() - 1;

  FakeScreen<Argb4444> visible(20, 14, color::Red);
  FakeScreen<Argb4444> extents(20, 14, color::Red);
  visible.Draw(label, 0, 11);
  extents.Draw(label, 0, 11, color::Black, FillMode::kExtents);
  EXPECT_EQ(color::Red, PixelAt(visible, 20, tracking_gap_x, 5));
  EXPECT_EQ(color::Black, PixelAt(extents, 20, tracking_gap_x, 5));

  FakeScreen<Argb4444> clipped(20, 14, color::Red);
  clipped.Draw(label, 0, 11, Box(tracking_gap_x, 1, tracking_gap_x, 10),
               color::Black, FillMode::kExtents);
  EXPECT_EQ(color::Black, PixelAt(clipped, 20, tracking_gap_x, 5));
  EXPECT_EQ(color::Red, PixelAt(clipped, 20, tracking_gap_x - 1, 5));
}

// Verifies the fixed font honors positive and negative tracking, including
// background fill across a positive extents gap.
TEST(SmoothFontTest, FixedFontTrackedRendering) {
  FontAdafruitFixed5x7 fixed_font;
  for (int16_t tracking_px : {int16_t(-2), int16_t(2)}) {
    Font::Options options;
    options.setTrackingPx(tracking_px);
    FakeScreen<Argb4444> actual(18, 12, color::Black);
    FakeScreen<Argb4444> expected(18, 12, color::Black);
    actual.Draw(TrackingLabel(fixed_font, "AA", options), 1, 8);
    expected.Draw(PositionedGlyphs(fixed_font, "AA", tracking_px), 1, 8);
    ExpectSamePixels(actual, expected, 18 * 12);
  }

  Font::Options positive_tracking;
  positive_tracking.setTrackingPx(2);
  GlyphMetrics fixed_a;
  ASSERT_TRUE(
      fixed_font.getGlyphMetrics(U'A', FontLayout::kHorizontal, &fixed_a));
  int tracking_gap_x = fixed_a.advance() + positive_tracking.trackingPx() - 1;
  FakeScreen<Argb4444> extents(18, 12, color::Red);
  extents.Draw(TrackingLabel(fixed_font, "A ", positive_tracking), 0, 8,
               color::Black, FillMode::kExtents);
  EXPECT_EQ(color::Black, PixelAt(extents, 18, tracking_gap_x, 4));
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
