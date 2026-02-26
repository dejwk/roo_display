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
                       BlendOp<BlendingMode::kSourceAtop>()(bg, fg), 1)
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
                       BlendOp<BlendingMode::kSourceIn>()(bg, fg), 1)
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
                       BlendOp<BlendingMode::kSourceOut>()(bg, fg), 1)
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
                       BlendOp<BlendingMode::kXor>()(bg, fg), 1)
              << bg << ", " << fg;
        }
      }
    }
  }
}

}  // namespace roo_display