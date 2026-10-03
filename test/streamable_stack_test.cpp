
#include "roo_display/composition/streamable_stack.h"

#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/shape/basic.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

TEST(StreamableStack, Empty) {
  StreamableStack stack(Box(3, 4, 5, 7));
  EXPECT_EQ(stack.naturalExtents(), Box(0, 0, -1, -1));
  FakeOffscreen<Rgb565> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(StreamableStack, SingleUnclipped) {
  auto input = MakeTestStreamable(Grayscale4(), Box(0, 0, 3, 3),
                                  "1234"
                                  "2345"
                                  "3456"
                                  "4567");
  StreamableStack stack(Box(3, 4, 9, 10));
  stack.addInput(&input, 5, 6);
  EXPECT_EQ(stack.naturalExtents(), Box(5, 6, 8, 9));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "     1234 "
                                          "     2345 "
                                          "     3456 "
                                          "     4567 "
                                          "          "));
}

TEST(StreamableStack, SingleNegativeOffset) {
  auto input = MakeTestStreamable(Grayscale4(), Box(0, 0, 2, 1),
                                  "123"
                                  "456");
  StreamableStack stack(Box(0, 0, 3, 4));
  stack.addInput(&input, -1, 2);
  EXPECT_EQ(stack.naturalExtents(), Box(-1, 2, 1, 3));
  FakeOffscreen<Argb4444> test_screen(4, 5, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 4, 5,
                                          "    "
                                          "    "
                                          "23  "
                                          "56  "
                                          "    "));
}

TEST(StreamableStack, UnsignedCallerVariables) {
  auto input = MakeTestStreamable(Grayscale4(), Box(0, 0, 3, 3),
                                  "1234"
                                  "2345"
                                  "3456"
                                  "4567");
  StreamableStack stack(Box(3, 4, 9, 10));
  uint16_t dx = 5;
  uint16_t dy = 6;
  stack.addInput(&input, dx, dy);
  EXPECT_EQ(stack.naturalExtents(), Box(5, 6, 8, 9));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "     1234 "
                                          "     2345 "
                                          "     3456 "
                                          "     4567 "
                                          "          "));
}

TEST(StreamableStack, SingleClipped) {
  auto input = MakeTestStreamable(Grayscale4(), Box(0, 0, 3, 3),
                                  "1234"
                                  "2345"
                                  "3456"
                                  "4567");
  StreamableStack stack(Box(3, 4, 9, 10));
  stack.addInput(&input, 5, 6);
  EXPECT_EQ(stack.naturalExtents(), Box(5, 6, 8, 9));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.setClipBox(6, 7, 7, 8);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "      34  "
                                          "      45  "
                                          "          "
                                          "          "));
}

TEST(StreamableStack, SingleSelfClipped) {
  auto input = MakeTestStreamable(Grayscale4(), Box(0, 0, 3, 3),
                                  "1234"
                                  "2345"
                                  "3456"
                                  "4567");
  StreamableStack stack(Box(3, 4, 9, 10));
  stack.addInput(&input, Box(1, 1, 2, 2), 5, 6);
  EXPECT_EQ(stack.naturalExtents(), Box(6, 7, 7, 8));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "      34  "
                                          "      45  "
                                          "          "
                                          "          "));
}

TEST(StreamableStack, TwoDisjoint) {
  auto input1 = MakeTestStreamable(Grayscale4(), Box(0, 0, 2, 2),
                                   "123"
                                   "234"
                                   "345");
  auto input2 = MakeTestStreamable(Grayscale4(), Box(0, 0, 2, 2),
                                   "666"
                                   "777"
                                   "888");
  StreamableStack stack(Box(1, 1, 9, 10));
  stack.addInput(&input1, 2, 2);
  stack.addInput(&input2, 6, 6);
  EXPECT_EQ(stack.naturalExtents(), Box(2, 2, 8, 8));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "  123     "
                                          "  234     "
                                          "  345     "
                                          "          "
                                          "      666 "
                                          "      777 "
                                          "      888 "
                                          "          "
                                          "          "));
}

TEST(StreamableStack, TwoDisjointInherentlyClipped) {
  auto input1 = MakeTestStreamable(Grayscale4(), Box(0, 0, 2, 2),
                                   "123"
                                   "234"
                                   "345");
  auto input2 = MakeTestStreamable(Grayscale4(), Box(0, 0, 2, 2),
                                   "666"
                                   "777"
                                   "888");
  StreamableStack stack(Box(3, 3, 7, 7));
  stack.addInput(&input1, 2, 2);
  stack.addInput(&input2, 6, 6);
  EXPECT_EQ(stack.naturalExtents(), Box(2, 2, 8, 8));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "          "
                                          "   34     "
                                          "   45     "
                                          "          "
                                          "      66  "
                                          "      77  "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(StreamableStack, TwoHorizontalOverlap) {
  auto input1 = MakeTestStreamable(Grayscale4(), Box(0, 0, 2, 2),
                                   "123"
                                   "234"
                                   "345");
  auto input2 = MakeTestStreamable(Grayscale4(), Box(0, 0, 2, 2),
                                   "666"
                                   "777"
                                   "888");
  StreamableStack stack(Box(1, 1, 9, 10));
  stack.addInput(&input1, 2, 2);
  stack.addInput(&input2, 6, 3);
  EXPECT_EQ(stack.naturalExtents(), Box(2, 2, 8, 5));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "  123     "
                                          "  234 666 "
                                          "  345 777 "
                                          "      888 "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(StreamableStack, TwoVerticalOverlap) {
  auto input1 = MakeTestStreamable(Grayscale4(), Box(0, 0, 2, 2),
                                   "123"
                                   "234"
                                   "345");
  auto input2 = MakeTestStreamable(Grayscale4(), Box(0, 0, 2, 2),
                                   "666"
                                   "777"
                                   "888");
  StreamableStack stack(Box(1, 1, 9, 10));
  stack.addInput(&input1, 2, 2);
  stack.addInput(&input2, 3, 6);
  EXPECT_EQ(stack.naturalExtents(), Box(2, 2, 5, 8));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "  123     "
                                          "  234     "
                                          "  345     "
                                          "          "
                                          "   666    "
                                          "   777    "
                                          "   888    "
                                          "          "
                                          "          "));
}

TEST(StreamableStack, TwoOverlap) {
  auto input1 = MakeTestStreamable(Grayscale4(), Box(0, 0, 2, 2),
                                   "123"
                                   "234"
                                   "345");
  auto input2 = MakeTestStreamable(Grayscale4(), Box(0, 0, 2, 2),
                                   "666"
                                   "777"
                                   "888");
  StreamableStack stack(Box(1, 1, 9, 10));
  stack.addInput(&input1, 2, 2);
  stack.addInput(&input2, 3, 3);
  EXPECT_EQ(stack.naturalExtents(), Box(2, 2, 5, 5));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "  123     "
                                          "  2666    "
                                          "  3777    "
                                          "   888    "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

TEST(StreamableStack, TwoOverlapAlphaBlend) {
  auto input1 = MakeTestStreamable(Alpha4(color::White), Box(0, 0, 2, 2),
                                   "567"
                                   "678"
                                   "789");
  auto input2 = MakeTestStreamable(Alpha4(color::White), Box(0, 0, 2, 2),
                                   "666"
                                   "777"
                                   "888");
  StreamableStack stack(Box(1, 1, 9, 10));
  stack.addInput(&input1, 2, 2);
  stack.addInput(&input2, 3, 3);
  EXPECT_EQ(stack.naturalExtents(), Box(2, 2, 5, 5));
  FakeOffscreen<Argb4444> test_screen(10, 11, color::Black);
  Display display(test_screen);
  {
    DrawingContext dc(display);
    dc.draw(stack);
  }
  EXPECT_THAT(test_screen, MatchesContent(Grayscale4(), 10, 11,
                                          "          "
                                          "          "
                                          "  567     "
                                          "  6AB6    "
                                          "  7BC7    "
                                          "   888    "
                                          "          "
                                          "          "
                                          "          "
                                          "          "
                                          "          "));
}

// Verifies createStream() reports exact BLANK-prefix run metadata.
TEST(StreamableStack, StreamReportsRunLengthForBlankPrefix) {
  FilledRect input(Box(2, 0, 3, 0), color::Red);
  StreamableStack stack(Box(0, 0, 7, 0));
  stack.addInput(&input);

  std::unique_ptr<PixelStream> stream = stack.createStream();
  Color pixel[1];
  uint32_t run_length = 0;

  stream->read(pixel, 1, run_length);
  EXPECT_EQ(run_length, 2u);
  EXPECT_EQ(pixel[0], color::Transparent);

  stream->read(pixel, 1, run_length);
  EXPECT_EQ(run_length, 1u);
  EXPECT_EQ(pixel[0], color::Transparent);

  stream->read(pixel, 1, run_length);
  EXPECT_EQ(run_length, 2u);
  EXPECT_EQ(pixel[0], color::Red);

  stream->read(pixel, 1, run_length);
  EXPECT_EQ(run_length, 1u);
  EXPECT_EQ(pixel[0], color::Red);

  stream->read(pixel, 1, run_length);
  EXPECT_EQ(run_length, 4u);
  EXPECT_EQ(pixel[0], color::Transparent);
}

// Verifies createStream() propagates exact WRITE_SINGLE runs for disjoint
// inputs.
TEST(StreamableStack, StreamReportsRunLengthForDisjointSingleInputSpans) {
  FilledRect left(Box(1, 0, 2, 0), color::Blue);
  FilledRect right(Box(6, 0, 7, 0), color::Green);
  StreamableStack stack(Box(0, 0, 9, 0));
  stack.addInput(&left);
  stack.addInput(&right);

  std::unique_ptr<PixelStream> stream = stack.createStream();
  Color pixel[1];
  uint32_t run_length = 0;

  stream->read(pixel, 1, run_length);
  EXPECT_EQ(run_length, 1u);
  EXPECT_EQ(pixel[0], color::Transparent);

  stream->read(pixel, 1, run_length);
  EXPECT_EQ(run_length, 2u);
  EXPECT_EQ(pixel[0], color::Blue);

  stream->read(pixel, 1, run_length);
  EXPECT_EQ(run_length, 1u);
  EXPECT_EQ(pixel[0], color::Blue);

  stream->read(pixel, 1, run_length);
  EXPECT_EQ(run_length, 3u);
  EXPECT_EQ(pixel[0], color::Transparent);

  stream->read(pixel, 1, run_length);
  EXPECT_EQ(run_length, 2u);
  EXPECT_EQ(pixel[0], color::Transparent);

  stream->read(pixel, 1, run_length);
  EXPECT_EQ(run_length, 1u);
  EXPECT_EQ(pixel[0], color::Transparent);

  stream->read(pixel, 1, run_length);
  EXPECT_EQ(run_length, 2u);
  EXPECT_EQ(pixel[0], color::Green);
}

namespace {

// Checks complete initialization, sample order, and any advertised uniform
// runs.
template <typename Expected>
void CheckStackStream(PixelStream& stream, const Box& bounds,
                      Expected expected) {
  Color buffer[131];
  uint32_t offset = 0;
  uint32_t pending_run = 0;
  Color run_color;
  const uint32_t area = bounds.area();
  while (offset < area) {
    uint32_t run = 123;
    stream.read(buffer, 0, run);
    ASSERT_EQ(run, 0u);
    FillColor(buffer, 131, Color(0xDEADBEEF));
    uint16_t batch = std::min<uint32_t>(131, area - offset);
    stream.read(buffer, batch, run);
    ASSERT_LE(run, area - offset);
    for (uint16_t i = 0; i < batch; ++i) {
      int16_t x = bounds.xMin() + (offset + i) % bounds.width();
      int16_t y = bounds.yMin() + (offset + i) / bounds.width();
      ASSERT_EQ(buffer[i], expected(x, y)) << "at " << x << ", " << y;
      if (pending_run > 0) {
        ASSERT_EQ(buffer[i], run_color);
        --pending_run;
      }
      if (i < run) {
        ASSERT_EQ(buffer[i], buffer[0]);
      }
    }
    if (run > batch) {
      pending_run = run - batch;
      run_color = buffer[0];
    }
    offset += batch;
  }
  uint32_t run = 123;
  stream.read(buffer, 0, run);
  EXPECT_EQ(run, 0u);
}

// Uses heap-backed output and checks the entire surface, including untouched
// edges.
template <typename Expected>
void CheckStackDrawing(const StreamableStack& stack, const Box& clip,
                       FillMode fill, Color background, Expected expected,
                       BlendingMode output_mode = BlendingMode::kSource) {
  FakeOffscreen<Argb8888> screen(clip.xMax() + 2, clip.yMax() + 2,
                                 color::Magenta);
  Display display(screen);
  {
    DrawingContext dc(display);
    dc.setClipBox(clip);
    dc.setFillMode(fill);
    dc.setBackgroundColor(background);
    dc.setBlendingMode(output_mode);
    dc.draw(stack);
  }
  Box bounds = Box::Intersect(stack.extents(), clip);
  for (int16_t y = 0; y < screen.raw_height(); ++y) {
    for (int16_t x = 0; x < screen.raw_width(); ++x) {
      Color want = color::Magenta;
      if (bounds.contains(x, y)) {
        Color composed = expected(x, y);
        if (fill == FillMode::kExtents || composed.a() != 0 ||
            composed == color::Background) {
          Color resolved = composed == color::Background ? background
                           : background == color::Transparent
                               ? composed
                               : AlphaBlend(background, composed);
          want = ApplyBlending(output_mode, want, resolved);
        }
      }
      ASSERT_EQ(screen.buffer()[y * screen.raw_width() + x], want)
          << "at " << x << ", " << y;
    }
  }
}

}  // namespace

class LargeStackTest : public TestWithParam<std::tuple<int, int>> {};

// Verifies large blank, single-input, and multi-input spans through every
// executor.
TEST_P(LargeStackTest, AllExecutorsInitializeEveryPixel) {
  const int width = std::get<0>(GetParam());
  const int height = std::get<1>(GetParam());
  Box bounds(7, 9, 7 + width - 1, 9 + height - 1);
  FilledRect red(bounds, color::Red);
  FilledRect blue(bounds, color::Blue);
  for (int inputs = 0; inputs <= 2; ++inputs) {
    SCOPED_TRACE(inputs);
    StreamableStack stack(bounds);
    if (inputs >= 1) stack.addInput(&red);
    if (inputs >= 2) stack.addInput(&blue);
    Color want = inputs == 0   ? color::Transparent
                 : inputs == 1 ? color::Red
                               : color::Blue;
    auto expected = [want](int16_t, int16_t) { return want; };
    CheckStackStream(*stack.createStream(), bounds, expected);
    CheckStackStream(*stack.createStream(bounds), bounds, expected);
    Box clipped(bounds.xMin() + 1, bounds.yMin(), bounds.xMax(), bounds.yMax());
    CheckStackStream(*stack.createStream(clipped), clipped, expected);
    for (FillMode fill : {FillMode::kExtents, FillMode::kVisible}) {
      CheckStackDrawing(stack, bounds, fill, color::Green, expected);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(OperandBoundaries, LargeStackTest,
                         Values(std::make_tuple(255, 257),
                                std::make_tuple(256, 256),
                                std::make_tuple(320, 240)));

// Verifies blank-run coordinate arithmetic with a translated 65,536-pixel
// prefix.
TEST(StreamableStack, LargeBlankPrefixFollowedByContent) {
  Box bounds(7, 9, 262, 265);
  FilledRect input(Box(7, 265, 262, 265), color::Red);
  StreamableStack stack(bounds);
  stack.addInput(&input);
  auto expected = [](int16_t, int16_t y) {
    return y == 265 ? color::Red : color::Transparent;
  };
  CheckStackStream(*stack.createStream(), bounds, expected);
  CheckStackStream(*stack.createStream(bounds), bounds, expected);
  for (FillMode fill : {FillMode::kExtents, FillMode::kVisible}) {
    CheckStackDrawing(stack, bounds, fill, color::Green, expected);
  }
}

// Verifies empty extents and disjoint clips create no child stream or pixel
// demand.
TEST(StreamableStack, EmptyOutputAndZeroSizeReads) {
  for (Box bounds : {Box(3, 4, 2, 8), Box(3, 4, 8, 3)}) {
    StreamableStack stack(bounds);
    Color sentinel(0xDEADBEEF);
    for (bool clipped : {false, true}) {
      std::unique_ptr<PixelStream> stream =
          clipped ? stack.createStream(bounds) : stack.createStream();
      for (int i = 0; i < 3; ++i) {
        uint32_t run = 123;
        stream->read(&sentinel, 0, run);
        EXPECT_EQ(run, 0u);
        EXPECT_EQ(sentinel, Color(0xDEADBEEF));
      }
    }
    CheckStackDrawing(stack, Box(0, 0, 9, 9), FillMode::kExtents, color::Green,
                      [](int16_t, int16_t) { return color::Transparent; });
  }
  StreamableStack stack(Box(0, 0, 9, 9));
  std::unique_ptr<PixelStream> stream = stack.createStream(Box(20, 20, 29, 29));
  uint32_t run = 123;
  stream->read(nullptr, 0, run);
  EXPECT_EQ(run, 0u);
}

// Verifies all sixteen registered bits survive, including the final translucent
// layer.
TEST(StreamableStack, InputCapacityBoundary) {
  Box bounds(0, 0, 19, 9);
  FilledRect red(bounds, color::Red);
  FilledRect blue(bounds, Color(0x800000FF));
  StreamableStack stack(bounds);
  for (int i = 0; i < 15; ++i) stack.addInput(&red);
  stack.addInput(&blue);
  Color want = AlphaBlend(color::Red, Color(0x800000FF));
  auto expected = [want](int16_t, int16_t) { return want; };
  CheckStackStream(*stack.createStream(), bounds, expected);
  CheckStackStream(*stack.createStream(bounds), bounds, expected);
  for (FillMode fill : {FillMode::kExtents, FillMode::kVisible}) {
    CheckStackDrawing(stack, bounds, fill, color::Green, expected);
  }
}

// Verifies release-enabled rejection counts clipped-out inputs in every entry
// point.
TEST(StreamableStackDeathTest, InputCapacityRejectedBeforeCompilation) {
  Box bounds(0, 0, 19, 9);
  FilledRect input(bounds, color::Red);
  for (int count : {17, 32}) {
    for (int visible : {0, 1, count}) {
      StreamableStack stack(bounds);
      for (int i = 0; i < count; ++i) {
        stack.addInput(&input, i < visible ? 0 : 100, 0);
      }
      std::string diagnostic =
          "StreamableStack.*" + std::to_string(count) + ".*16";
      EXPECT_DEATH(stack.createStream(), diagnostic);
      EXPECT_DEATH(stack.createStream(Box(1, 0, 18, 9)), diagnostic);
      for (FillMode fill : {FillMode::kExtents, FillMode::kVisible}) {
        EXPECT_DEATH(
            CheckStackDrawing(stack, bounds, fill, color::Green,
                              [](int16_t, int16_t) { return color::Red; }),
            diagnostic);
      }
    }
  }
}

// Verifies empty output returns before the compiled-input limit is checked.
TEST(StreamableStack, EmptyOutputAllowsExcessRegisteredInputs) {
  FilledRect input(Box(0, 0, 19, 9), color::Red);
  StreamableStack stack(Box(0, 0, -1, -1));
  for (int i = 0; i < 32; ++i) stack.addInput(&input);
  uint32_t run = 123;
  stack.createStream()->read(nullptr, 0, run);
  EXPECT_EQ(run, 0u);
  stack.setExtents(input.extents());
  stack.createStream(Box(30, 30, 39, 39))->read(nullptr, 0, run);
  EXPECT_EQ(run, 0u);
  CheckStackDrawing(stack, Box(30, 30, 39, 39), FillMode::kVisible,
                    color::Green, [](int16_t, int16_t) { return color::Red; });
}

namespace {

Color CoordinateColor(int16_t x, int16_t y) {
  return Color(0xFF000000u | (static_cast<uint32_t>(x + 1) << 8) | y);
}

// Checks logical consumption while allowing BufferingStream to read ahead.
class CoordinateStream : public PixelStream {
 public:
  explicit CoordinateStream(Box bounds) : bounds_(bounds), position_(0) {}

  void read(Color* buffer, uint16_t size, uint32_t& run) override {
    run = 0;
    ASSERT_LE(position_ + size, static_cast<uint32_t>(bounds_.area()));
    for (uint16_t i = 0; i < size; ++i, ++position_) {
      buffer[i] = CoordinateColor(bounds_.xMin() + position_ % bounds_.width(),
                                  bounds_.yMin() + position_ / bounds_.width());
    }
  }

  void skip(uint32_t count) override {
    ASSERT_LE(position_ + count, static_cast<uint32_t>(bounds_.area()));
    position_ += count;
  }

 private:
  Box bounds_;
  uint32_t position_;
};

class CoordinateSource : public Streamable {
 public:
  explicit CoordinateSource(Box bounds) : bounds_(bounds) {}

  Box extents() const override { return bounds_; }

  std::unique_ptr<PixelStream> createStream() const override {
    return createStream(bounds_);
  }

  std::unique_ptr<PixelStream> createStream(const Box& clip) const override {
    return std::unique_ptr<PixelStream>(
        new CoordinateStream(Box::Intersect(bounds_, clip)));
  }

 private:
  Box bounds_;
};

}  // namespace

// Verifies consecutive and sparse skip masks preserve samples across chunks and
// rows.
TEST(StreamableStack, SkipIndicesPreserveCoordinateSamples) {
  Box bounds(0, 0, 8, 19);
  FilledRect red(bounds, color::Red);
  CoordinateSource ramp(bounds);
  FilledRect hole(Box(8, 0, 8, 19), color::Blue);
  FilledRect mask(Box(3, 0, 5, 19), color::White);
  for (bool sparse : {false, true}) {
    SCOPED_TRACE(sparse);
    StreamableStack stack(bounds);
    stack.addInput(&red);
    if (sparse) stack.addInput(&hole);
    stack.addInput(&ramp);
    stack.addInput(&mask).withMode(BlendingMode::kDestinationIn);
    auto expected = [](int16_t x, int16_t y) {
      return x >= 3 && x <= 5 ? CoordinateColor(x, y) : color::Transparent;
    };
    CheckStackStream(*stack.createStream(), bounds, expected);
    CheckStackStream(*stack.createStream(Box(1, 1, 7, 18)), Box(1, 1, 7, 18),
                     expected);
    for (FillMode fill : {FillMode::kExtents, FillMode::kVisible}) {
      CheckStackDrawing(stack, bounds, fill, color::Green, expected);
    }
  }
}

namespace {

// Varies alpha and placeholder values across rows and stream-buffer boundaries.
Color VariedStackColor(int16_t x, int16_t y) {
  const Color colors[] = {color::Transparent, Color(0x00123456),
                          Color(0x80654321),  color::Red,
                          color::Background,  color::Blue};
  return colors[(x + 3 * y) % 6];
}

class VariedStackSource : public Rasterizable {
 public:
  Box extents() const override { return Box(0, 0, 16, 9); }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    for (uint32_t i = 0; i < count; ++i) {
      result[i] = VariedStackColor(x[i], y[i]);
    }
  }
};

}  // namespace

class StackBlendTest : public TestWithParam<BlendingMode> {};

// Verifies exact first-input evaluation and ordered two-input blends in every
// executor.
TEST_P(StackBlendTest, ExactColorsWithAndWithoutCoalescing) {
  const Color colors[] = {color::Transparent, Color(0x00123456),
                          Color(0x80654321), color::Red, color::Background};
  const BlendingMode mode = GetParam();
  Box interior(0, 0, 6, 2);
  for (Color first : colors) {
    for (int layers : {1, 2}) {
      for (int second_index = 0;
           second_index <= static_cast<int>(BlendingMode::kXor);
           ++second_index) {
        if (layers == 1 && second_index != 0) continue;
        BlendingMode second_mode = static_cast<BlendingMode>(second_index);
        SCOPED_TRACE(second_index);
        for (Color second : colors) {
          if (layers == 1 && second != colors[0]) continue;
          SCOPED_TRACE(first.asArgb());
          SCOPED_TRACE(second.asArgb());
          SCOPED_TRACE(layers);
          for (bool chunked : {false, true}) {
            SCOPED_TRACE(chunked);
            Box bounds(0, 0, chunked ? 8 : 6, 2);
            FilledRect input1(interior, first);
            FilledRect input2(interior, second);
            FilledRect disjoint(Box(8, 0, 8, 2), color::Blue);
            StreamableStack stack(bounds);
            stack.addInput(&input1).withMode(mode);
            if (layers == 2) stack.addInput(&input2).withMode(second_mode);
            if (chunked) stack.addInput(&disjoint);
            Color want = color::Transparent;
            for (int i = 0; i < layers; ++i) {
              want = ApplyBlending(i == 0 ? mode : second_mode, want,
                                   i == 0 ? first : second);
            }
            auto expected = [want](int16_t x, int16_t) {
              return x < 7 ? want : x == 7 ? color::Transparent : color::Blue;
            };
            CheckStackStream(*stack.createStream(), bounds, expected);
            Box clip(1, 1, bounds.xMax(), 2);
            CheckStackStream(*stack.createStream(clip), clip, expected);
            for (FillMode fill : {FillMode::kExtents, FillMode::kVisible}) {
              for (Color background : {color::Transparent, color::Green,
                                       Color(0x40778899), Color(0x00123456)}) {
                CheckStackDrawing(stack, bounds, fill, background, expected);
              }
            }
          }
        }
      }
    }
  }
}

// Verifies direct single-input reads and batched multi-input reads keep exact
// colors across changing alpha, placeholders, clips, and background branches.
TEST_P(StackBlendTest, VaryingSamplesWithEveryBackground) {
  VariedStackSource input;
  const Box bounds = input.extents();
  const BlendingMode mode = GetParam();
  const Color overlay_color(0x80432165);
  FilledRect overlay(bounds, overlay_color);
  for (bool multiple : {false, true}) {
    SCOPED_TRACE(multiple);
    StreamableStack stack(bounds);
    stack.addInput(&input).withMode(mode);
    if (multiple) stack.addInput(&overlay);
    auto expected = [mode, multiple, overlay_color](int16_t x, int16_t y) {
      Color result =
          ApplyBlending(mode, color::Transparent, VariedStackColor(x, y));
      return multiple ? ApplyBlending(BlendingMode::kSourceOver, result,
                                      overlay_color)
                      : result;
    };
    CheckStackStream(*stack.createStream(), bounds, expected);
    for (Box clip : {bounds, Box(2, 1, 14, 8)}) {
      CheckStackStream(*stack.createStream(clip), clip, expected);
      for (FillMode fill : {FillMode::kExtents, FillMode::kVisible}) {
        for (Color background : {color::Transparent, color::Green,
                                 Color(0x40778899), Color(0x00123456)}) {
          for (BlendingMode output_mode :
               {BlendingMode::kSource, BlendingMode::kSourceOver}) {
            CheckStackDrawing(stack, clip, fill, background, expected,
                              output_mode);
          }
        }
      }
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    OrdinaryModes, StackBlendTest,
    Values(BlendingMode::kSource, BlendingMode::kSourceOver,
           BlendingMode::kSourceIn, BlendingMode::kSourceAtop,
           BlendingMode::kDestination, BlendingMode::kDestinationOver,
           BlendingMode::kDestinationIn, BlendingMode::kDestinationAtop,
           BlendingMode::kClear, BlendingMode::kSourceOut,
           BlendingMode::kDestinationOut, BlendingMode::kXor));

// Verifies the source-over initialization shortcut preserves every nonzero
// alpha and clears zero-alpha RGB in streams and both drawing executors.
TEST(StreamableStack, SourceOverFirstInputMatchesEveryAlpha) {
  const Box bounds(0, 0, 16, 2);
  for (uint32_t alpha = 0; alpha <= 255; ++alpha) {
    SCOPED_TRACE(alpha);
    const Color sample((alpha << 24) | 0x00123456);
    FilledRect input(bounds, sample);
    StreamableStack stack(bounds);
    stack.addInput(&input);
    const Color want =
        ApplyBlending(BlendingMode::kSourceOver, color::Transparent, sample);
    auto expected = [want](int16_t, int16_t) { return want; };
    CheckStackStream(*stack.createStream(), bounds, expected);
    for (FillMode fill : {FillMode::kExtents, FillMode::kVisible}) {
      for (Color background : {color::Transparent, color::Green,
                               Color(0x40778899), Color(0x00123456)}) {
        CheckStackDrawing(stack, bounds, fill, background, expected);
      }
    }
  }
}

// Verifies the device's blend mode is applied after internal composition and
// background.
TEST(StreamableStack, OutputModeIsSeparateFromInputMode) {
  Box bounds(0, 0, 8, 2);
  FilledRect input(bounds, Color(0x80334455));
  StreamableStack stack(bounds);
  stack.addInput(&input).withMode(BlendingMode::kSource);
  for (FillMode fill : {FillMode::kExtents, FillMode::kVisible}) {
    CheckStackDrawing(
        stack, bounds, fill, Color(0x40778899),
        [](int16_t, int16_t) { return Color(0x80334455); },
        BlendingMode::kDestinationOver);
  }
}

// Verifies a maximum-sized public read crosses split operands without
// truncation.
TEST(StreamableStack, LargeReadCrossesInstructionBoundary) {
  Box bounds(0, 0, 319, 239);
  FilledRect input(bounds, color::Red);
  StreamableStack stack(bounds);
  stack.addInput(&input);
  std::unique_ptr<PixelStream> stream = stack.createStream();
  std::vector<Color> buffer(65535, Color(0xDEADBEEF));
  stream->read(buffer.data(), 65535);
  for (Color pixel : buffer) ASSERT_EQ(pixel, color::Red);
  FillColor(buffer.data(), 11265, Color(0xDEADBEEF));
  stream->read(buffer.data(), 11265);
  for (int i = 0; i < 11265; ++i) ASSERT_EQ(buffer[i], color::Red);
}

// Verifies instruction positions beyond 65,535 words without a large pixel
// buffer.
TEST(StreamableStack, ProgramPositionsExceedWordRange) {
  Box bounds(0, 0, 32766, 32766);
  FilledRect input(bounds, color::Blue);
  StreamableStack stack(bounds);
  // Each eliminated full input needs over 49,000 words of split SKIPs.
  stack.addInput(&input).withMode(BlendingMode::kDestination);
  stack.addInput(&input).withMode(BlendingMode::kDestination);
  stack.addInput(&input);
  std::unique_ptr<PixelStream> stream = stack.createStream();
  Color pixels[7];
  stream->read(pixels, 7);
  for (Color pixel : pixels) EXPECT_EQ(pixel, color::Blue);
}

// Verifies clipping and translation keep coordinate samples in row-major order.
TEST(StreamableStack, ClippedTranslatedCoordinateSamples) {
  CoordinateSource input(Box(0, 0, 19, 19));
  StreamableStack stack(Box(3, 4, 13, 16));
  stack.addInput(&input, Box(2, 3, 14, 17), 1, 1);
  auto expected = [](int16_t x, int16_t y) {
    return CoordinateColor(x - 1, y - 1);
  };
  CheckStackStream(*stack.createStream(), stack.extents(), expected);
  Box clip(4, 6, 11, 15);
  CheckStackStream(*stack.createStream(clip), clip, expected);
  for (FillMode fill : {FillMode::kExtents, FillMode::kVisible}) {
    CheckStackDrawing(stack, clip, fill, color::Green, expected);
  }
}

namespace {

// Makes accidental child-stream preparation observable in empty and invalid
// stacks.
class ForbiddenStreamSource : public Streamable {
 public:
  Box extents() const override { return Box(0, 0, 19, 9); }

  std::unique_ptr<PixelStream> createStream() const override {
    CHECK(false) << "Unexpected child stream creation";
    return nullptr;
  }

  std::unique_ptr<PixelStream> createStream(const Box&) const override {
    return createStream();
  }
};

}  // namespace

// Verifies limit validation happens before preparing even the first child
// stream.
TEST(StreamableStackDeathTest, InputCapacityCheckedBeforeChildStreams) {
  ForbiddenStreamSource input;
  StreamableStack stack(input.extents());
  for (int i = 0; i < 17; ++i) stack.addInput(&input);
  EXPECT_DEATH(stack.createStream(), "StreamableStack.*17.*16");
  EXPECT_DEATH(stack.createStream(input.extents()), "StreamableStack.*17.*16");
  EXPECT_DEATH(CheckStackDrawing(stack, input.extents(), FillMode::kVisible,
                                 color::Green,
                                 [](int16_t, int16_t) { return color::Red; }),
               "StreamableStack.*17.*16");
}

// Verifies empty output prepares no child streams, including with excess
// inputs.
TEST(StreamableStack, EmptyOutputDoesNotPrepareChildren) {
  ForbiddenStreamSource input;
  StreamableStack stack(Box(0, 0, -1, -1));
  for (int i = 0; i < 17; ++i) stack.addInput(&input);
  uint32_t run = 123;
  stack.createStream()->read(nullptr, 0, run);
  EXPECT_EQ(run, 0u);
  stack.setExtents(input.extents());
  stack.createStream(Box(30, 30, 39, 39))->read(nullptr, 0, run);
  EXPECT_EQ(run, 0u);
  CheckStackDrawing(stack, Box(30, 30, 39, 39), FillMode::kVisible,
                    color::Green, [](int16_t, int16_t) { return color::Red; });
}

// Verifies buffering a raster input does not reuse the previous batch's run
// metadata when the next batch varies. Uses the configured production/test
// size.
TEST(StreamableStack, UniformThenVaryingRunMetadata) {
  constexpr int kBatch = kPixelWritingBufferSize;
  Box bounds(0, 0, 4 * kBatch - 1, 0);
  auto layer = MakeRasterizable(bounds, [](int16_t x, int16_t y) {
    return x < kBatch ? color::Red : (x % 2 == 0 ? color::Blue : color::Green);
  });
  StreamableStack stack(bounds);
  stack.addInput(&layer);
  auto stream = stack.createStream();
  Color pixels[kBatch];
  uint32_t run = 0;
  stream->read(pixels, kBatch, run);
  ASSERT_EQ(run, static_cast<uint32_t>(kBatch));
  stream->read(pixels, kBatch, run);
  EXPECT_NE(pixels[0], pixels[1]);
  EXPECT_EQ(run, 0u);
}

// Verifies rebuilding and replacing inputs preserves stack/anchor bounds,
// refreshes clips and offsets, and resets an old mode without changing order.
TEST(StreamableStack, ReuseAndReplaceInputs) {
  Box bounds(0, 0, 19, 9);
  FilledRect red(bounds, color::Red);
  FilledRect blue(Box(10, 10, 29, 19), color::Blue);
  StreamableStack stack(bounds);
  stack.setAnchorExtents(Box(2, 2, 7, 7));
  stack.reserveInputs(4);
  stack.addInput(&red);
  stack.addInput(&blue).withMode(BlendingMode::kDestinationIn);
  EXPECT_EQ(stack.inputCount(), 2u);
  const auto& replaced =
      stack.setInput(1, &blue, Box(12, 12, 16, 16), -10, -10);
  EXPECT_EQ(replaced.extents(), Box(2, 2, 6, 6));
  EXPECT_EQ(replaced.blending_mode(), BlendingMode::kSourceOver);
  auto expected = [](int16_t x, int16_t y) {
    return Box(2, 2, 6, 6).contains(x, y) ? color::Blue : color::Red;
  };
  CheckStackStream(*stack.createStream(), bounds, expected);
  stack.clearInputs();
  EXPECT_EQ(stack.inputCount(), 0u);
  EXPECT_EQ(stack.extents(), bounds);
  EXPECT_EQ(stack.anchorExtents(), Box(2, 2, 7, 7));
  stack.addInput(&blue);
  stack.setInput(0, &red);
  CheckStackStream(*stack.createStream(), bounds,
                   [](int16_t, int16_t) { return color::Red; });
}

// Verifies replacing a missing layer is rejected before writing storage.
TEST(StreamableStackDeathTest, InvalidReplacementIndex) {
  FilledRect red(Box(0, 0, 1, 1), color::Red);
  StreamableStack stack(red.extents());
  EXPECT_DEATH(stack.setInput(0, &red), "index");
}

// Verifies capacity can be checked before drawing or creating child streams,
// while preserving the exemption for empty output and counting absent inputs.
TEST(StreamableStack, CapacityPreflight) {
  FilledRect source(Box(100, 100, 101, 101), color::Red);
  StreamableStack stack(Box(0, 0, 9, 9));
  for (size_t i = 0; i < StreamableStack::kMaxInputs; ++i)
    stack.addInput(&source);
  EXPECT_TRUE(stack.canCreateStream());
  stack.addInput(&source);
  EXPECT_FALSE(stack.canCreateStream());
  EXPECT_FALSE(stack.canCreateStream(Box(1, 1, 2, 2)));
  EXPECT_TRUE(stack.canCreateStream(Box(20, 20, 21, 21)));
  stack.clearInputs();
  EXPECT_TRUE(stack.canCreateStream());
}

namespace {

struct StreamConsumption {
  uint32_t read = 0;
  uint32_t skipped = 0;
};

class CountingCoordinateStream : public CoordinateStream {
 public:
  CountingCoordinateStream(Box bounds, StreamConsumption* counts)
      : CoordinateStream(bounds), counts_(counts) {}

  void read(Color* pixels, uint16_t count, uint32_t& run) override {
    counts_->read += count;
    CoordinateStream::read(pixels, count, run);
  }

  void skip(uint32_t count) override {
    counts_->skipped += count;
    CoordinateStream::skip(count);
  }

 private:
  StreamConsumption* counts_;
};

class CountingCoordinateSource : public Streamable {
 public:
  CountingCoordinateSource(Box bounds, StreamConsumption* counts)
      : bounds_(bounds), counts_(counts) {}

  Box extents() const override { return bounds_; }

  std::unique_ptr<PixelStream> createStream() const override {
    return createStream(bounds_);
  }

  std::unique_ptr<PixelStream> createStream(const Box& clip) const override {
    return std::unique_ptr<PixelStream>(
        new CountingCoordinateStream(Box::Intersect(bounds_, clip), counts_));
  }

 private:
  Box bounds_;
  StreamConsumption* counts_;
};

}  // namespace

// Verifies nested skipping advances source streams without evaluating the
// skipped pixels, including counts exceeding an instruction's 16-bit operand.
TEST(StreamableStack, NestedSkipAvoidsPixelReads) {
  Box bounds(0, 0, 299, 299);
  StreamConsumption counts;
  CountingCoordinateSource source(bounds, &counts);
  StreamableStack inner(bounds);
  inner.addInput(&source);
  StreamableStack outer(bounds);
  outer.addInput(&inner);
  auto stream = outer.createStream();
  stream->skip(0);
  EXPECT_EQ(counts.read, 0u);
  stream->skip(70000);
  EXPECT_EQ(counts.read, 0u);
  EXPECT_EQ(counts.skipped, 70000u);
  Color pixel;
  stream->read(&pixel, 1);
  EXPECT_EQ(pixel, CoordinateColor(70000 % 300, 70000 / 300));
}

// Verifies skipping part of a span and across blank, overlapping, and masked
// spans preserves child positions and later pixels through compiled row loops.
TEST(StreamableStack, InterleavedReadsAndSkips) {
  Box bounds(0, 0, 31, 19);
  CoordinateSource source(bounds);
  FilledRect tint(Box(3, 2, 27, 18), Color(0x800000FF));
  FilledRect mask(Box(6, 1, 29, 19), color::White);
  StreamableStack stack(bounds);
  stack.addInput(&source);
  stack.addInput(&tint);
  stack.addInput(&mask).withMode(BlendingMode::kDestinationIn);
  auto stream = stack.createStream();
  int offset = 0;
  for (int step = 0; offset < bounds.area(); ++step) {
    int skip = std::min((step * 17) % 43, bounds.area() - offset);
    stream->skip(skip);
    offset += skip;
    int count = std::min(7, bounds.area() - offset);
    Color pixels[7];
    stream->read(pixels, count);
    for (int i = 0; i < count; ++i) {
      int x = (offset + i) % bounds.width();
      int y = (offset + i) / bounds.width();
      Color expected = CoordinateColor(x, y);
      if (tint.extents().contains(x, y))
        expected = AlphaBlend(expected, tint.color());
      if (!mask.extents().contains(x, y)) expected = color::Transparent;
      ASSERT_EQ(pixels[i], expected) << offset + i;
    }
    offset += count;
  }
  stream->skip(0);
}

// Verifies wholly replaced sources are never opened, even when their bounds
// intersect the output; tests exact source replacement and opacity hints.
TEST(StreamableStack, WhollyReplacedSourcesAreNotOpened) {
  Box bounds(0, 0, 19, 9);
  ForbiddenStreamSource forbidden;
  for (BlendingMode mode : {BlendingMode::kSource, BlendingMode::kSourceOver}) {
    for (Color color : {color::Blue, color::Background, Color(0x00123456)}) {
      if (mode == BlendingMode::kSourceOver && !color.isOpaque()) continue;
      FilledRect upper(bounds, color);
      StreamableStack stack(bounds);
      stack.addInput(&forbidden);
      stack.addInput(&upper).withMode(mode);
      auto expected = [color](int16_t, int16_t) { return color; };
      CheckStackStream(*stack.createStream(), bounds, expected);
      CheckStackDrawing(stack, bounds, FillMode::kExtents, color::Transparent,
                        expected);
    }
  }
}

// Verifies a partially hidden coordinate source is skipped then resumes at the
// right pixel, without reading a long opaque prefix.
TEST(StreamableStack, OpaquePrefixSkipsHiddenSamples) {
  Box bounds(0, 0, 199, 0);
  StreamConsumption counts;
  CountingCoordinateSource source(bounds, &counts);
  FilledRect opaque(Box(0, 0, 99, 0), color::Red);
  StreamableStack stack(bounds);
  stack.addInput(&source);
  stack.addInput(&opaque);
  auto stream = stack.createStream();
  Color pixels[100];
  stream->read(pixels, 100);
  EXPECT_EQ(counts.read, 0u);
  EXPECT_EQ(counts.skipped, 100u);
  stream->read(pixels, 1);
  EXPECT_EQ(pixels[0], CoordinateColor(100, 0));
}

}  // namespace roo_display
