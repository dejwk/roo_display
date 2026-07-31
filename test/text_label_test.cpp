#include "roo_display/ui/text_label.h"

#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/font/font.h"
#include "roo_fonts/NotoSerif_Italic/12.h"
#include "testing_drawable.h"

using namespace testing;

namespace roo_display {

const Font& font12() { return font_NotoSerif_Italic_12(); }

// Draws text directly with the same layout options used to measure its bounds.
class TrackedTextReference : public Drawable {
 public:
  TrackedTextReference(roo::string_view text, const Font& font, Color color,
                       FillMode fill_mode, const Font::Options& options)
      : text_(text),
        font_(font),
        color_(color),
        fill_mode_(fill_mode),
        options_(options),
        metrics_(font.getHorizontalStringMetrics(text, options)) {}

 private:
  void drawTo(const Surface& s) const override {
    Surface news(s);
    if (fill_mode_ == FillMode::kExtents) {
      news.set_fill_mode(FillMode::kExtents);
    }
    font_.drawHorizontalString(news, text_, color_, options_);
  }

  Box extents() const override { return metrics_.screen_extents(); }

  roo::string_view text_;
  const Font& font_;
  Color color_;
  FillMode fill_mode_;
  Font::Options options_;
  GlyphMetrics metrics_;
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

// Verifies owned and borrowed labels cache tracked metrics and paint with the
// same options in visible and extents modes.
TEST(TextLabel, TrackingOptionsMatchMetricsAndRasterization) {
  Font::Options positive_tracking;
  positive_tracking.setTrackingPx(2);
  TextLabel owned("AV", font12(), color::White, positive_tracking,
                  FillMode::kVisible);
  GlyphMetrics positive_metrics =
      font12().getHorizontalStringMetrics("AV", positive_tracking);
  EXPECT_EQ(positive_tracking.trackingPx(), owned.options().trackingPx());
  EXPECT_EQ(positive_metrics.screen_extents().xMin(), owned.extents().xMin());
  EXPECT_EQ(positive_metrics.screen_extents().xMax(), owned.extents().xMax());
  EXPECT_EQ(positive_metrics.advance() - 1, owned.anchorExtents().xMax());

  FakeScreen<Argb4444> owned_actual(20, 14, color::Black);
  FakeScreen<Argb4444> owned_expected(20, 14, color::Black);
  owned_actual.Draw(owned, 1, 11);
  owned_expected.Draw(
      TrackedTextReference("AV", font12(), color::White, FillMode::kVisible,
                           positive_tracking),
      1, 11);
  ExpectSamePixels(owned_actual, owned_expected, 20 * 14);

  Font::Options negative_tracking;
  negative_tracking.setTrackingPx(-2);
  StringViewLabel borrowed("AV", font12(), color::White, negative_tracking,
                           FillMode::kExtents);
  GlyphMetrics negative_metrics =
      font12().getHorizontalStringMetrics("AV", negative_tracking);
  EXPECT_EQ(negative_tracking.trackingPx(), borrowed.options().trackingPx());
  EXPECT_EQ(negative_metrics.screen_extents().xMax(),
            borrowed.extents().xMax());
  EXPECT_EQ(negative_metrics.advance() - 1, borrowed.anchorExtents().xMax());

  FakeScreen<Argb4444> borrowed_actual(20, 14, color::Red);
  FakeScreen<Argb4444> borrowed_expected(20, 14, color::Red);
  borrowed_actual.Draw(borrowed, 1, 11, color::Black, FillMode::kExtents);
  borrowed_expected.Draw(
      TrackedTextReference("AV", font12(), color::White, FillMode::kExtents,
                           negative_tracking),
      1, 11, color::Black, FillMode::kExtents);
  ExpectSamePixels(borrowed_actual, borrowed_expected, 20 * 14);
}

// Verifies clipped owned and borrowed labels retain tracking in their tight
// extents and clipped raster output.
TEST(TextLabel, ClippedTrackingOptionsMatchMetricsAndRasterization) {
  Font::Options options;
  options.setTrackingPx(2);
  ClippedTextLabel owned("AV", font12(), color::White, options,
                         FillMode::kExtents);
  ClippedStringViewLabel borrowed("AV", font12(), color::White, options,
                                  FillMode::kExtents);
  GlyphMetrics metrics = font12().getHorizontalStringMetrics("AV", options);
  EXPECT_EQ(metrics.screen_extents().xMax(), owned.anchorExtents().xMax());
  EXPECT_EQ(metrics.screen_extents().xMax(), borrowed.extents().xMax());
  EXPECT_EQ(metrics.advance() - 1, borrowed.anchorExtents().xMax());

  Box clip(5, 2, 13, 11);
  FakeScreen<Argb4444> owned_actual(20, 14, color::Red);
  FakeScreen<Argb4444> owned_expected(20, 14, color::Red);
  owned_actual.Draw(owned, 1, 11, clip, color::Black, FillMode::kExtents);
  owned_expected.Draw(TrackedTextReference("AV", font12(), color::White,
                                           FillMode::kExtents, options),
                      1, 11, clip, color::Black, FillMode::kExtents);
  ExpectSamePixels(owned_actual, owned_expected, 20 * 14);

  FakeScreen<Argb4444> borrowed_actual(20, 14, color::Red);
  FakeScreen<Argb4444> borrowed_expected(20, 14, color::Red);
  borrowed_actual.Draw(borrowed, 1, 11, clip, color::Black, FillMode::kExtents);
  borrowed_expected.Draw(TrackedTextReference("AV", font12(), color::White,
                                              FillMode::kExtents, options),
                         1, 11, clip, color::Black, FillMode::kExtents);
  ExpectSamePixels(borrowed_actual, borrowed_expected, 20 * 14);
}

}  // namespace roo_display
