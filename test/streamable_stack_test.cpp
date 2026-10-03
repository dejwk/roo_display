
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

}  // namespace roo_display
