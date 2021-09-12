
#include "roo_display/core/rasterizable.h"

#include "roo_display/core/color.h"
#include "testing.h"

// Tests drawing and clipping rasterizables via their default drawTo method, and
// via CreateStream.

using namespace testing;

namespace roo_display {

class ForcedStreamable : public Streamable {
 public:
  ForcedStreamable(const Streamable* delegate) : delegate_(delegate) {}

  Box extents() const override { return delegate_->extents(); }

  std::unique_ptr<PixelStream> CreateStream() const override {
    return delegate_->CreateStream();
  }

 private:
  const Streamable* delegate_;
};

void Draw(DisplayDevice* output, int16_t x, int16_t y, const Box& clip_box,
          const Drawable& object, FillMode fill_mode = FILL_MODE_VISIBLE,
          PaintMode paint_mode = PAINT_MODE_BLEND,
          Color bgcolor = color::Transparent) {
  output->begin();
  Surface s(output, x, y, clip_box, bgcolor, fill_mode, paint_mode);
  s.drawObject(object);
  output->end();
}

void Draw(DisplayDevice* output, int16_t x, int16_t y,
          const Drawable& object, FillMode fill_mode = FILL_MODE_VISIBLE,
          PaintMode paint_mode = PAINT_MODE_BLEND,
          Color bgcolor = color::Transparent) {
  Box clip_box(0, 0, output->effective_width() - 1,
               output->effective_height() - 1);
  Draw(output, x, y, clip_box, object, fill_mode, paint_mode, bgcolor);
}

// typedef bool (*setter_fn)(int16_t, int16_t);

typedef std::function<Color(int16_t, int16_t)> setter_fn;

setter_fn circle(int16_t x0, int16_t y0, int16_t r) {
  return [x0, y0, r](int16_t x, int16_t y) -> Color {
    if (x < x0) x = x0 + x0 - x;
    if (y < y0) y = y0 + y0 - y;
    return (x - x0) * (x - x0) + (y - y0) * (y - y0) < r * r - 1 ? color::White
                                                                 : color::Black;
  };
}

TEST(Rasterizable, SimpleFilledCircle) {
  auto input =
      MakeRasterizable(Box(0, 0, 6, 7), circle(2, 2, 3), TRANSPARENCY_NONE);

  FakeOffscreen<Rgb565> test_screen(9, 10, color::Black);
  Draw(&test_screen, 1, 2, input);
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 9, 10,
                                          "         "
                                          "         "
                                          "  ***    "
                                          " *****   "
                                          " *****   "
                                          " *****   "
                                          "  ***    "
                                          "         "
                                          "         "
                                          "         "));
}

TEST(Rasterizable, SimpleFilledCircleClipped) {
  auto input =
      MakeRasterizable(Box(0, 0, 6, 7), circle(2, 2, 3), TRANSPARENCY_NONE);

  FakeOffscreen<Rgb565> test_screen(9, 10, color::Black);
  Draw(&test_screen, 1, 2, Box(0, 0, 3, 4), input);
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 9, 10,
                                          "         "
                                          "         "
                                          "  **     "
                                          " ***     "
                                          " ***     "
                                          "         "
                                          "         "
                                          "         "
                                          "         "
                                          "         "));
}

TEST(Rasterizable, SimpleFilledCircleAsStreamable) {
  auto input =
      MakeRasterizable(Box(0, 0, 6, 7), circle(2, 2, 3), TRANSPARENCY_NONE);

  FakeOffscreen<Rgb565> test_screen(9, 10, color::Black);
  Draw(&test_screen, 1, 2, ForcedStreamable(&input));
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 9, 10,
                                          "         "
                                          "         "
                                          "  ***    "
                                          " *****   "
                                          " *****   "
                                          " *****   "
                                          "  ***    "
                                          "         "
                                          "         "
                                          "         "));
}

TEST(Rasterizable, SimpleFilledCircleClippedAsStreamable) {
  auto input =
      MakeRasterizable(Box(0, 0, 6, 7), circle(2, 2, 3), TRANSPARENCY_NONE);

  FakeOffscreen<Rgb565> test_screen(9, 10, color::Black);
  Draw(&test_screen, 1, 2, Box(0, 0, 3, 4), ForcedStreamable(&input));
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 9, 10,
                                          "         "
                                          "         "
                                          "  **     "
                                          " ***     "
                                          " ***     "
                                          "         "
                                          "         "
                                          "         "
                                          "         "
                                          "         "));
}

}  // namespace roo_display
