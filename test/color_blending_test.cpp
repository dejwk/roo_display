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

TEST(Color, AlphaBlendSourceOverFull) {
  for (uint8_t as = 0; as < 255; as += 17) {
    for (uint8_t ad = 0; ad < 255; ad += 17) {
      for (uint8_t cs = 0; cs < 255; cs += 17) {
        for (uint8_t cd = 0; cd < 255; cd += 17) {
          Color fg(as, cs, cs, cs);
          Color bg(ad, cd, cd, cd);
          EXPECT_PRED3(Near, BlendReferenceSourceOver(bg, fg),
                       AlphaBlend(bg, fg), 1)
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

}  // namespace roo_display