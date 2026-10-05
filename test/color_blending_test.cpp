#include <cmath>

#include "gtest/gtest.h"
#include "roo_display/color/blending.h"

namespace roo_display {

std::ostream& operator<<(std::ostream& os, const Color& color) {
  return os << "argb:" << std::hex << color.asArgb();
}

inline constexpr float AsF(uint8_t val) { return val / 255.0f; }

Color FromFloat(float a, float r, float g, float b) {
  return Color(roundf(a * 255), roundf(r), roundf(g), roundf(b));
  // return Color(floorf(a * 255), floorf(r), floorf(g), floorf(b));
}

bool Near(Color c1, Color c2, int precision_level) {
  if (c1.a() == 0 && c2.a() == 0) return true;
  if (std::abs((int)c1.a() - (int)c2.a()) > 0) return false;
  int tolerance = 3 - precision_level;
  if (std::abs((int)c1.r() - (int)c2.r()) > tolerance) return false;
  if (std::abs((int)c1.g() - (int)c2.g()) > tolerance) return false;
  if (std::abs((int)c1.b() - (int)c2.b()) > tolerance) return false;
  return true;
}

Color Mix(float a, float sc, Color bg, Color fg) {
  return FromFloat(a, sc * fg.r() + (1 - sc) * bg.r(),
                   sc * fg.g() + (1 - sc) * bg.g(),
                   sc * fg.b() + (1 - sc) * bg.b());
}

Color Blend(Color bg, Color fg, float fa, float fb) {
  float alpha = fa * AsF(fg.a()) + fb * AsF(bg.a());
  float sc = fa * AsF(fg.a()) / alpha;
  return Mix(alpha, sc, bg, fg);
}

Color BlendReferenceSourceOver(Color bg, Color fg) {
  return Blend(bg, fg, 1, 1 - AsF(fg.a()));
}

Color BlendReferenceSourceAtop(Color bg, Color fg) {
  return Blend(bg, fg, AsF(bg.a()), 1 - AsF(fg.a()));
}

Color BlendReferenceSourceIn(Color bg, Color fg) {
  return Blend(bg, fg, AsF(bg.a()), 0);
}

Color BlendReferenceSourceOut(Color bg, Color fg) {
  return Blend(bg, fg, 1 - AsF(bg.a()), 0);
}

Color BlendReferenceXor(Color bg, Color fg) {
  return Blend(bg, fg, 1 - AsF(bg.a()), 1 - AsF(fg.a()));
}

template <BlendingMode mode>
void ExpectTransparentSrcEquivalent(Color bg, Color src) {
  BlendOp<mode> op;
  Color transparent_src = src.withA(0);
  Color control = op.blend(bg, transparent_src);
  Color test = op.blendTransparentSrc(bg, transparent_src);
  if (control == test) return;
  if ((control == Color(0) || test == Color(0)) ||
      (control.a() != 0 || test.a() != 0)) {
    EXPECT_EQ(op.blend(bg, transparent_src).asArgb(),
              op.blendTransparentSrc(bg, transparent_src).asArgb())
        << bg << ", " << transparent_src << "; mode = " << (int)mode;
  }
}

template <BlendingMode mode>
void ExpectTransparentDstEquivalent(Color dst, Color src) {
  BlendOp<mode> op;
  Color transparent_dst = dst.withA(0);
  Color control = op.blend(transparent_dst, src);
  Color test = op.blendTransparentDst(transparent_dst, src);
  if (control == test) return;
  if ((control == Color(0) || test == Color(0)) ||
      (control.a() != 0 || test.a() != 0)) {
    EXPECT_EQ(op.blend(transparent_dst, src).asArgb(),
              op.blendTransparentDst(transparent_dst, src).asArgb())
        << transparent_dst << ", " << src << "; mode = " << (int)mode;
  }
}

// Verifies the constant-source optimization preserves exact transparent colors,
// including the Transparent and Background placeholders, just like blend().
TEST(Color, DestinationOverTransparentSourcePreservesExactColors) {
  for (Color src : {color::Transparent, color::Background, Color(0x00123456)}) {
    Color destinations[] = {color::Transparent, color::Background,
                            Color(0x00654321), Color(0x80654321),
                            Color(0xFFFF0000)};
    Color expected[5];
    for (int i = 0; i < 5; ++i) {
      expected[i] =
          ApplyBlending(BlendingMode::kDestinationOver, destinations[i], src);
    }
    ApplyBlendingSingleSourceInPlace(BlendingMode::kDestinationOver,
                                     destinations, src, 5);
    for (int i = 0; i < 5; ++i) EXPECT_EQ(destinations[i], expected[i]);
  }
}

TEST(Color, AlphaBlendSimple) {
  EXPECT_EQ(Color(0xFFFFFFFF),
            AlphaBlend(Color(0xFFFFFFFF), Color(0x00000000)));

  EXPECT_EQ(BlendReferenceSourceOver(Color(0xFFFFFFFF), Color(0x00000000)),
            AlphaBlend(Color(0xFFFFFFFF), Color(0x00000000)));

  EXPECT_EQ(Color(0xFF7F7F7F),
            AlphaBlend(Color(0xFFFFFFFF), Color(0x80000000)));
}

TEST(Color, AlphaBlendAlpha) {
  for (uint8_t as = 0; as < 255; as += 1) {
    for (uint8_t ad = 0; ad < 255; ad += 1) {
      Color fg(as, 0x80, 0x80, 0x80);
      Color bg(ad, 0x80, 0x80, 0x80);
      Color expected = BlendReferenceSourceOver(bg, fg);
      Color actual = AlphaBlend(bg, fg);
      EXPECT_EQ((int)expected.a(), (int)actual.a()) << bg << ", " << fg;
    }
  }
}

// Verifies SourceOver against the accuracy bound of the selected precision.
TEST(Color, AlphaBlendSourceOverFull) {
  for (uint8_t as = 0; as < 255; as += 17) {
    for (uint8_t ad = 0; ad < 255; ad += 17) {
      for (uint8_t cs = 0; cs < 255; cs += 17) {
        for (uint8_t cd = 0; cd < 255; cd += 17) {
          Color fg(as, cs, cs, cs);
          Color bg(ad, cd, cd, cd);
          EXPECT_PRED3(Near, BlendReferenceSourceOver(bg, fg),
                       AlphaBlend(bg, fg), ROO_DISPLAY_BLENDING_PRECISION)
              << bg << ", " << fg;
        }
      }
    }
  }
}

TEST(Color, AlphaBlendSourceAtopFull) {
  for (uint8_t as = 0; as < 255; as += 17) {
    for (uint8_t ad = 0; ad < 255; ad += 17) {
      for (uint8_t cs = 0; cs < 255; cs += 17) {
        for (uint8_t cd = 0; cd < 255; cd += 17) {
          Color fg(as, cs, cs, cs);
          Color bg(ad, cd, cd, cd);
          EXPECT_PRED3(Near, BlendReferenceSourceAtop(bg, fg),
                       BlendOp<BlendingMode::kSourceAtop>().blend(bg, fg), 1)
              << bg << ", " << fg;
        }
      }
    }
  }
}

// Verifies tinting an equal RGB color never darkens it and preserves coverage,
// including exact transparent RGB and the two special transparent values.
TEST(Color, SourceAtopPreservesEqualColorsAndCoverage) {
  const BlendOp<BlendingMode::kSourceAtop> op;
  for (int alpha = 0; alpha <= 255; ++alpha) {
    for (int channel = 0; channel <= 255; ++channel) {
      for (int coverage : {0, 1, 37, 128, 254, 255}) {
        const Color dst(coverage, channel, channel, channel);
        EXPECT_EQ(op.blend(dst, dst.withA(alpha)), dst);
      }
    }
  }
}

// Verifies exact rounded interpolation at precision 1/2 and a one-channel-unit
// error bound at precision 0, for every source alpha and channel pair.
TEST(Color, SourceAtopPrecisionAndOpaqueSourceOverAgreement) {
  const BlendOp<BlendingMode::kSourceAtop> op;
  for (int alpha = 0; alpha <= 255; ++alpha) {
    for (int src_channel = 0; src_channel <= 255; ++src_channel) {
      for (int dst_channel = 0; dst_channel <= 255; ++dst_channel) {
        const Color src(alpha, src_channel, 255 - src_channel, src_channel);
        const Color dst(73, dst_channel, 255 - dst_channel, dst_channel);
        const Color result = op.blend(dst, src);
        const int exact =
            (alpha * src_channel + (255 - alpha) * dst_channel + 127) / 255;
#if ROO_DISPLAY_BLENDING_PRECISION == 0
        const int expected = alpha == 0 ? dst_channel
                                        : ((alpha + 1) * src_channel +
                                           (255 - alpha) * dst_channel + 128) /
                                              256;
        ASSERT_EQ(result.r(), expected);
        ASSERT_LE(std::abs(result.r() - exact), 1);
        ASSERT_LE(std::abs(result.g() - (255 - exact)), 1);
#else
        ASSERT_EQ(result.r(), exact);
        ASSERT_EQ(result, AlphaBlend(dst.withA(255), src).withA(dst.a()));
#endif
        ASSERT_EQ(result.b(), result.r());
        ASSERT_EQ(result.a(), dst.a());
        ASSERT_EQ(op.blend(dst.withA(255), src),
                  AlphaBlend(dst.withA(255), src));
      }
    }
  }
}

// Verifies all bulk entry points agree with scalar SourceAtop, including zero
// alpha endpoints where Background and Transparent must stay distinguishable.
TEST(Color, SourceAtopBulkPreservesExactTransparentDestinations) {
  constexpr BlendingMode mode = BlendingMode::kSourceAtop;
  const BlendOp<mode> op;
  for (int alpha = 0; alpha <= 255; ++alpha) {
    const Color src(alpha, 210, 30, 100);
    const Color dst[] = {color::Transparent, color::Background,
                         Color(0x00123456),  Color(0x01406080),
                         Color(0x80406080),  Color(0xFF406080)};
    Color varying[6];
    Color constant[6];
    Color indexed[6];
    Color sources[6];
    const uint32_t indices[] = {5, 4, 3, 2, 1, 0};
    for (int i = 0; i < 6; ++i) {
      varying[i] = constant[i] = indexed[i] = dst[i];
      sources[i] = src;
    }
    ApplyBlendingInPlace(mode, varying, sources, 6);
    ApplyBlendingSingleSourceInPlace(mode, constant, src, 6);
    ApplyBlendingInPlaceIndexed(mode, indexed, sources, 6, indices);
    for (int i = 0; i < 6; ++i) {
      const Color expected = op.blend(dst[i], src);
      EXPECT_EQ(varying[i], expected);
      EXPECT_EQ(constant[i], expected);
      EXPECT_EQ(indexed[i], expected);
      Color over_background = src;
      ApplyBlendingOverBackground(mode, dst[i], &over_background, 1);
      EXPECT_EQ(over_background, expected);
      if (dst[i].a() == 0) {
        EXPECT_EQ(expected, dst[i]);
        EXPECT_EQ(op.blendTransparentDst(dst[i], src), dst[i]);
      }
    }
  }
}

// Verifies DestinationAtop remains the exact transpose of SourceAtop even
// when constant-source blending takes its transparent-source shortcut.
TEST(Color, SourceAtopTransposeMatchesBulkDestinationAtop) {
  for (Color src : {color::Transparent, color::Background, Color(0x00123456),
                    Color(0x80506070), Color(0xFF123456)}) {
    Color dst[] = {color::Transparent, color::Background, Color(0x00876543),
                   Color(0x01406080),  Color(0x80406080), Color(0xFF406080)};
    Color expected[6];
    for (int i = 0; i < 6; ++i) {
      expected[i] = ApplyBlending(BlendingMode::kSourceAtop, src, dst[i]);
      EXPECT_EQ(ApplyBlending(BlendingMode::kDestinationAtop, dst[i], src),
                expected[i]);
    }
    ApplyBlendingSingleSourceInPlace(BlendingMode::kDestinationAtop, dst, src,
                                     6);
    for (int i = 0; i < 6; ++i) EXPECT_EQ(dst[i], expected[i]);
  }
}

TEST(Color, AlphaBlendSourceInFull) {
  for (uint8_t as = 0; as < 255; as += 17) {
    for (uint8_t ad = 0; ad < 255; ad += 17) {
      for (uint8_t cs = 0; cs < 255; cs += 17) {
        for (uint8_t cd = 0; cd < 255; cd += 17) {
          Color fg(as, cs, cs, cs);
          Color bg(ad, cd, cd, cd);
          EXPECT_PRED3(Near, BlendReferenceSourceIn(bg, fg),
                       BlendOp<BlendingMode::kSourceIn>().blend(bg, fg), 1)
              << bg << ", " << fg;
        }
      }
    }
  }
}

TEST(Color, AlphaBlendSourceOutFull) {
  for (uint8_t as = 0; as < 255; as += 17) {
    for (uint8_t ad = 0; ad < 255; ad += 17) {
      for (uint8_t cs = 0; cs < 255; cs += 17) {
        for (uint8_t cd = 0; cd < 255; cd += 17) {
          Color fg(as, cs, cs, cs);
          Color bg(ad, cd, cd, cd);
          EXPECT_PRED3(Near, BlendReferenceSourceOut(bg, fg),
                       BlendOp<BlendingMode::kSourceOut>().blend(bg, fg), 1)
              << bg << ", " << fg;
        }
      }
    }
  }
}

TEST(Color, AlphaBlendSourceXorFull) {
  for (uint8_t as = 0; as < 255; as += 17) {
    for (uint8_t ad = 0; ad < 255; ad += 17) {
      for (uint8_t cs = 0; cs < 255; cs += 17) {
        for (uint8_t cd = 0; cd < 255; cd += 17) {
          Color fg(as, cs, cs, cs);
          Color bg(ad, cd, cd, cd);
          EXPECT_PRED3(Near, BlendReferenceXor(bg, fg),
                       BlendOp<BlendingMode::kXor>().blend(bg, fg), 1)
              << bg << ", " << fg;
        }
      }
    }
  }
}

TEST(Color, BlendTransparentSrcMatchesBlendWithZeroAlpha) {
  for (uint8_t ad = 0; ad < 255; ad += 17) {
    for (uint8_t as = 0; as < 255; as += 17) {
      for (uint8_t cd = 0; cd < 255; cd += 17) {
        for (uint8_t cs = 0; cs < 255; cs += 17) {
          Color bg(ad, cd, (uint8_t)(255 - cd), (uint8_t)(cd ^ 0x55));
          Color src(as, cs, (uint8_t)(255 - cs), (uint8_t)(cs ^ 0xAA));
          ExpectTransparentSrcEquivalent<BlendingMode::kSource>(bg, src);
          ExpectTransparentSrcEquivalent<BlendingMode::kSourceOver>(bg, src);
          ExpectTransparentSrcEquivalent<BlendingMode::kSourceIn>(bg, src);
          ExpectTransparentSrcEquivalent<BlendingMode::kSourceAtop>(bg, src);
          ExpectTransparentSrcEquivalent<BlendingMode::kDestination>(bg, src);
          ExpectTransparentSrcEquivalent<BlendingMode::kDestinationOver>(bg,
                                                                         src);
          // This one is excluded because it requires an opaque src.
          // ExpectTransparentSrcEquivalent<BlendingMode::kDestinationOverOpaque>(
          //     bg, src);
          ExpectTransparentSrcEquivalent<BlendingMode::kDestinationIn>(bg, src);
          ExpectTransparentSrcEquivalent<BlendingMode::kDestinationAtop>(bg,
                                                                         src);
          ExpectTransparentSrcEquivalent<BlendingMode::kClear>(bg, src);
          ExpectTransparentSrcEquivalent<BlendingMode::kSourceOut>(bg, src);
          ExpectTransparentSrcEquivalent<BlendingMode::kDestinationOut>(bg,
                                                                        src);
          ExpectTransparentSrcEquivalent<BlendingMode::kXor>(bg, src);
        }
      }
    }
  }
}

TEST(Color, BlendTransparentSrcMatchesBlendWithZeroAlphaOpaqueDst) {
  for (uint8_t as = 0; as < 255; as += 17) {
    for (uint8_t cd = 0; cd < 255; cd += 17) {
      for (uint8_t cs = 0; cs < 255; cs += 17) {
        Color bg(255, cd, (uint8_t)(255 - cd), (uint8_t)(cd ^ 0x55));
        Color src(as, cs, (uint8_t)(255 - cs), (uint8_t)(cs ^ 0xAA));
        ExpectTransparentSrcEquivalent<BlendingMode::kSourceOverOpaque>(bg,
                                                                        src);
      }
    }
  }
}

TEST(Color, BlendTransparentDstMatchesBlendWithZeroAlpha) {
  for (uint8_t ad = 0; ad < 255; ad += 17) {
    for (uint8_t as = 0; as < 255; as += 17) {
      for (uint8_t cd = 0; cd < 255; cd += 17) {
        for (uint8_t cs = 0; cs < 255; cs += 17) {
          Color dst(ad, cd, (uint8_t)(255 - cd), (uint8_t)(cd ^ 0x55));
          Color src(as, cs, (uint8_t)(255 - cs), (uint8_t)(cs ^ 0xAA));
          ExpectTransparentDstEquivalent<BlendingMode::kSource>(dst, src);
          ExpectTransparentDstEquivalent<BlendingMode::kSourceOver>(dst, src);
          ExpectTransparentDstEquivalent<BlendingMode::kSourceIn>(dst, src);
          ExpectTransparentDstEquivalent<BlendingMode::kSourceAtop>(dst, src);
          ExpectTransparentDstEquivalent<BlendingMode::kDestination>(dst, src);
          ExpectTransparentDstEquivalent<BlendingMode::kDestinationOver>(dst,
                                                                         src);
          // This one is excluded because it requires an opaque dst.
          // ExpectTransparentDstEquivalent<BlendingMode::kSourceOverOpaque>(
          //     dst, src);
          ExpectTransparentDstEquivalent<BlendingMode::kDestinationIn>(dst,
                                                                       src);
          ExpectTransparentDstEquivalent<BlendingMode::kDestinationAtop>(dst,
                                                                         src);
          ExpectTransparentDstEquivalent<BlendingMode::kClear>(dst, src);
          ExpectTransparentDstEquivalent<BlendingMode::kSourceOut>(dst, src);
          ExpectTransparentDstEquivalent<BlendingMode::kDestinationOut>(dst,
                                                                        src);
          ExpectTransparentDstEquivalent<BlendingMode::kXor>(dst, src);
        }
      }
    }
  }
}

TEST(Color, BlendTransparentDstMatchesBlendWithZeroAlphaOpaqueSrc) {
  for (uint8_t ad = 0; ad < 255; ad += 17) {
    for (uint8_t cd = 0; cd < 255; cd += 17) {
      for (uint8_t cs = 0; cs < 255; cs += 17) {
        Color dst(ad, cd, (uint8_t)(255 - cd), (uint8_t)(cd ^ 0x55));
        Color src(255, cs, (uint8_t)(255 - cs), (uint8_t)(cs ^ 0xAA));
        ExpectTransparentDstEquivalent<BlendingMode::kDestinationOverOpaque>(
            dst, src);
      }
    }
  }
}

}  // namespace roo_display