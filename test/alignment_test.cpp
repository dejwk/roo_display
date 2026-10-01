#include "roo_display/ui/alignment.h"

#include "gtest/gtest.h"

namespace roo_display {
namespace {

// Verifies an ascent band above the baseline has equal top/bottom margins.
TEST(AlignmentTest, CentersNegativeGlyphCoordinatesExactly) {
  const Box outer(0, 0, 63, 63);
  const Box inner(-16, -16, -1, -1);
  const Offset offset = (kCenter | kMiddle).resolveOffset(outer, inner);
  EXPECT_EQ(40, offset.dx);
  EXPECT_EQ(40, offset.dy);
  EXPECT_EQ(24, inner.yMin() + offset.dy - outer.yMin());
  EXPECT_EQ(24, outer.yMax() - inner.yMax() - offset.dy);
  EXPECT_EQ(43, kMiddle.shiftBy(3).resolveOffset(0, 63, -16, -1));
}

// Verifies equal-parity spans center exactly regardless of their origins.
TEST(AlignmentTest, ExactCenteringDoesNotDependOnCoordinateSigns) {
  for (int outer_start : {-100, -1, 0, 1, 100}) {
    for (int inner_start : {-100, -16, -1, 0, 1, 100}) {
      for (int height : {16, 17}) {
        const int offset =
            kMiddle.resolveOffset(outer_start, outer_start + height + 47,
                                  inner_start, inner_start + height - 1);
        EXPECT_EQ(24, inner_start + offset - outer_start);
      }
    }
  }
}

// Verifies unavoidable half-pixel offsets round toward negative infinity in
// either direction, and explicit shifts apply after rounding.
TEST(AlignmentTest, FractionalOffsetsRoundOnce) {
  EXPECT_EQ(3, kMiddle.resolveOffset(1, 6, -2, 2));
  EXPECT_EQ(-4, kMiddle.resolveOffset(-2, 2, 1, 6));
  EXPECT_EQ(0, kMiddle.shiftBy(-3).resolveOffset(1, 6, -2, 2));
}

// Verifies integer and mixed anchor pairs use the same coordinate convention.
TEST(AlignmentTest, OtherAnchorPairs) {
  EXPECT_EQ(16, kTop.resolveOffset(0, 63, -16, -1));
  EXPECT_EQ(64, kBottom.resolveOffset(0, 63, -16, -1));
  EXPECT_EQ(0, kBaseline.resolveOffset(0, 63, -16, -1));
  EXPECT_EQ(31, kBaseline.toMiddle().resolveOffset(0, 63, -16, -1));
  EXPECT_EQ(8, kMiddle.toTop().resolveOffset(0, 63, -16, -1));
}

// Verifies all anchor pairs preserve integer translations, including when
// fractional offsets cross zero. Origin anchors intentionally ignore bounds.
TEST(AlignmentTest, TranslationIsUniformAcrossZeroForAllAnchorPairs) {
  for (Anchor dst :
       {Anchor::kOrigin, Anchor::kMin, Anchor::kMid, Anchor::kMax}) {
    for (Anchor src :
         {Anchor::kOrigin, Anchor::kMin, Anchor::kMid, Anchor::kMax}) {
      const VAlign alignment(dst, src, 3);
      const int initial = alignment.resolveOffset(0, 3, 0, 2);
      for (int translation = -10; translation <= 10; ++translation) {
        EXPECT_EQ(initial - (src == Anchor::kOrigin ? 0 : translation),
                  alignment.resolveOffset(0, 3, translation, translation + 2));
        EXPECT_EQ(initial + (dst == Anchor::kOrigin ? 0 : translation),
                  alignment.resolveOffset(translation, translation + 3, 0, 2));
      }
    }
  }
}

// Verifies moving one edge by two pixels always moves a midpoint by one pixel.
TEST(AlignmentTest, ResizingHasUniformHalfPixelCadence) {
  for (int edge = -9; edge <= 9; ++edge) {
    EXPECT_EQ(kMiddle.toTop().resolveOffset(0, 0, -10, edge) - 1,
              kMiddle.toTop().resolveOffset(0, 0, -10, edge + 2));
    EXPECT_EQ(kTop.toMiddle().resolveOffset(-10, edge, 0, 0) + 1,
              kTop.toMiddle().resolveOffset(-10, edge + 2, 0, 0));
  }
  EXPECT_EQ(-1, kTop.toMiddle().resolveOffset(0, 3, 2, 4));
  EXPECT_EQ(0, kMiddle.toTop().resolveOffset(2, 4, 0, 3));
}

// Verifies doubling valid 16-bit coordinates does not overflow intermediates.
TEST(AlignmentTest, LargeCoordinatesRetainPrecision) {
  EXPECT_EQ(40, kMiddle.resolveOffset<int16_t>(30000, 30063, 29984, 29999));
  EXPECT_EQ(40, kMiddle.resolveOffset<int16_t>(-30000, -29937, -30016, -30001));
}

}  // namespace
}  // namespace roo_display
