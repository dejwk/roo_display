#include "roo_display/filter/clip_exclude_rects.h"

#include <array>
#include <vector>

#include "testing.h"

namespace roo_display {

namespace {

using TestOffscreen = FakeOffscreen<Argb8888>;

class CountingOutput : public DisplayOutput {
 public:
  explicit CountingOutput(TestOffscreen& output) : output_(output) {}

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    ++set_address_count_;
    output_.setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    output_.write(color, pixel_count);
  }

  void fill(Color color, uint32_t pixel_count) override {
    output_.fill(color, pixel_count);
  }

  void writePixels(BlendingMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    output_.writePixels(mode, color, x, y, pixel_count);
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    output_.fillPixels(mode, color, x, y, pixel_count);
  }

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    output_.writeRects(mode, color, x0, y0, x1, y1, count);
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    output_.fillRects(mode, color, x0, y0, x1, y1, count);
  }

  const ColorFormat& getColorFormat() const override {
    return output_.getColorFormat();
  }

  const Capabilities& getCapabilities() const override {
    return output_.getCapabilities();
  }

  int setAddressCount() const { return set_address_count_; }

 private:
  TestOffscreen& output_;
  int set_address_count_ = 0;
};

void AdvanceWindowCursor(const Box& window, int16_t& x, int16_t& y) {
  ++x;
  if (x > window.xMax()) {
    x = window.xMin();
    ++y;
  }
}

void WriteExpected(TestOffscreen& expected, const RectUnion& exclusion,
                   const Box& window, const std::vector<Color>& colors,
                   BlendingMode mode = BlendingMode::kSource) {
  int16_t x = window.xMin();
  int16_t y = window.yMin();
  for (Color color : colors) {
    if (!exclusion.contains(x, y)) {
      expected.writePixel(mode, x, y, color);
    }
    AdvanceWindowCursor(window, x, y);
  }
}

void FillExpected(TestOffscreen& expected, const RectUnion& exclusion,
                  const Box& window, Color color, uint32_t pixel_count,
                  BlendingMode mode = BlendingMode::kSource) {
  int16_t x = window.xMin();
  int16_t y = window.yMin();
  for (uint32_t i = 0; i < pixel_count; ++i) {
    if (!exclusion.contains(x, y)) {
      expected.writePixel(mode, x, y, color);
    }
    AdvanceWindowCursor(window, x, y);
  }
}

void ExpectSameRaster(const TestOffscreen& actual,
                      const TestOffscreen& expected) {
  EXPECT_THAT(RasterOf(actual), MatchesContent(RasterOf(expected)));
}

std::vector<Color> MakeGradient(size_t count) {
  std::vector<Color> colors;
  colors.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    colors.emplace_back(static_cast<uint8_t>(16 + i * 7),
                        static_cast<uint8_t>(48 + i * 11),
                        static_cast<uint8_t>(96 + i * 13));
  }
  return colors;
}

}  // namespace

TEST(RectUnion, EmptyUnionReturnsFalseWithPositiveCount) {
  RectUnion exclusion(nullptr, nullptr);
  size_t same_count = 0;

  EXPECT_FALSE(exclusion.contains(4, 7, &same_count));
  EXPECT_GE(same_count, 1u);
}

TEST(RectUnion, OutsidePointTracksNearestFutureBoxOnSameRow) {
  std::array<Box, 2> boxes = {Box(5, 2, 8, 4), Box(12, 2, 13, 4)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  size_t same_count = 0;

  EXPECT_FALSE(exclusion.contains(1, 3, &same_count));
  EXPECT_EQ(same_count, 4u);
}

TEST(RectUnion, InsidePointReturnsExtentOfFirstContainingBox) {
  std::array<Box, 1> boxes = {Box(3, 5, 8, 7)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  size_t same_count = 0;

  EXPECT_TRUE(exclusion.contains(3, 6, &same_count));
  EXPECT_EQ(same_count, 6u);
}

TEST(RectUnion, OverlappingBoxesRespectEarlyReturn) {
  std::array<Box, 2> boxes = {Box(5, 1, 7, 3), Box(6, 1, 10, 3)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  size_t same_count = 0;

  EXPECT_TRUE(exclusion.contains(6, 2, &same_count));
  EXPECT_EQ(same_count, 2u);
}

TEST(RectUnion, OtherRowsDoNotAffectFalseRunCount) {
  std::array<Box, 2> boxes = {Box(1, 0, 3, 0), Box(4, 3, 8, 3)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  size_t same_count = 0;

  EXPECT_FALSE(exclusion.contains(0, 3, &same_count));
  EXPECT_EQ(same_count, 4u);
}

// Verifies visible row starts can absorb clear rows plus the next-row prefix.
TEST(RectUnion, VisiblePixelsFromRowStartSpansClearRowsAndNextPrefix) {
  std::array<Box, 2> boxes = {Box(2, 2, 4, 2), Box(0, 4, 4, 4)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());

  EXPECT_EQ(exclusion.visiblePixelsFromRowStart(Box(0, 0, 4, 4), 0), 12u);
}

// Verifies excluded row starts can absorb full rows and the next-row prefix.
TEST(RectUnion, ExcludedPixelsFromRowStartSpansCoveredRowsAndNextPrefix) {
  std::array<Box, 2> boxes = {Box(0, 0, 4, 1), Box(0, 2, 1, 2)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());

  EXPECT_EQ(exclusion.excludedPixelsFromRowStart(Box(0, 0, 4, 3), 0), 12u);
}

TEST(RectUnionFilter, WritePartiallyExcludedSingleRowConsumesSource) {
  std::array<Box, 1> boxes = {Box(2, 0, 3, 0)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  Box window(0, 0, 5, 0);
  std::vector<Color> colors = MakeGradient(static_cast<size_t>(window.area()));

  TestOffscreen actual(6, 2, color::White);
  TestOffscreen expected(6, 2, color::White);

  RectUnionFilter filter(actual, &exclusion);
  filter.setAddress(window.xMin(), window.yMin(), window.xMax(), window.yMax(),
                    BlendingMode::kSource);
  filter.write(colors.data(), colors.size());

  WriteExpected(expected, exclusion, window, colors);
  ExpectSameRaster(actual, expected);
}

TEST(RectUnionFilter, WriteAcrossRowsMatchesReference) {
  std::array<Box, 2> boxes = {Box(3, 0, 3, 0), Box(0, 1, 1, 1)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  Box window(0, 0, 3, 1);
  std::vector<Color> colors = MakeGradient(static_cast<size_t>(window.area()));

  TestOffscreen actual(4, 2, color::White);
  TestOffscreen expected(4, 2, color::White);

  RectUnionFilter filter(actual, &exclusion);
  filter.setAddress(window.xMin(), window.yMin(), window.xMax(), window.yMax(),
                    BlendingMode::kSource);
  filter.write(colors.data(), colors.size());

  WriteExpected(expected, exclusion, window, colors);
  ExpectSameRaster(actual, expected);
}

TEST(RectUnionFilter, WriteSequentialChunksPreserveCursorAndSourceAdvance) {
  std::array<Box, 2> boxes = {Box(1, 0, 1, 0), Box(3, 0, 4, 0)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  Box window(0, 0, 5, 0);
  std::vector<Color> colors = MakeGradient(static_cast<size_t>(window.area()));

  TestOffscreen actual(6, 1, color::White);
  TestOffscreen expected(6, 1, color::White);

  RectUnionFilter filter(actual, &exclusion);
  filter.setAddress(window.xMin(), window.yMin(), window.xMax(), window.yMax(),
                    BlendingMode::kSource);
  filter.write(colors.data(), 2);
  filter.write(colors.data() + 2, colors.size() - 2);

  WriteExpected(expected, exclusion, window, colors);
  ExpectSameRaster(actual, expected);
}

TEST(RectUnionFilter, WriteSequentialChunksReuseVisibleRunAcrossCalls) {
  std::array<Box, 1> boxes = {Box(4, 0, 4, 0)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  Box window(0, 0, 5, 0);
  std::vector<Color> colors = MakeGradient(static_cast<size_t>(window.area()));

  TestOffscreen actual(6, 1, color::White);
  TestOffscreen expected(6, 1, color::White);

  RectUnionFilter filter(actual, &exclusion);
  filter.setAddress(window.xMin(), window.yMin(), window.xMax(), window.yMax(),
                    BlendingMode::kSource);
  filter.write(colors.data(), 2);
  filter.write(colors.data() + 2, colors.size() - 2);

  WriteExpected(expected, exclusion, window, colors);
  ExpectSameRaster(actual, expected);
}

TEST(RectUnionFilter, WriteSequentialChunksReuseExcludedRunAcrossCalls) {
  std::array<Box, 1> boxes = {Box(1, 0, 3, 0)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  Box window(0, 0, 5, 0);
  std::vector<Color> colors = MakeGradient(static_cast<size_t>(window.area()));

  TestOffscreen actual(6, 1, color::White);
  TestOffscreen expected(6, 1, color::White);

  RectUnionFilter filter(actual, &exclusion);
  filter.setAddress(window.xMin(), window.yMin(), window.xMax(), window.yMax(),
                    BlendingMode::kSource);
  filter.write(colors.data(), 2);
  filter.write(colors.data() + 2, colors.size() - 2);

  WriteExpected(expected, exclusion, window, colors);
  ExpectSameRaster(actual, expected);
}

TEST(RectUnionFilter, FillAcrossRowsMatchesReference) {
  std::array<Box, 2> boxes = {Box(0, 0, 0, 0), Box(2, 1, 3, 1)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  Box window(0, 0, 3, 1);
  Color fill = Color(0xFF2255AA);

  TestOffscreen actual(4, 2, color::White);
  TestOffscreen expected(4, 2, color::White);

  RectUnionFilter filter(actual, &exclusion);
  filter.setAddress(window.xMin(), window.yMin(), window.xMax(), window.yMax(),
                    BlendingMode::kSource);
  filter.fill(fill, static_cast<uint32_t>(window.area()));

  FillExpected(expected, exclusion, window, fill,
               static_cast<uint32_t>(window.area()));
  ExpectSameRaster(actual, expected);
}

TEST(RectUnionFilter, FillSequentialChunksReuseExcludedRunAcrossCalls) {
  std::array<Box, 1> boxes = {Box(1, 0, 3, 0)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  Box window(0, 0, 5, 0);
  Color fill = Color(0xFF2255AA);

  TestOffscreen actual(6, 1, color::White);
  TestOffscreen expected(6, 1, color::White);

  RectUnionFilter filter(actual, &exclusion);
  filter.setAddress(window.xMin(), window.yMin(), window.xMax(), window.yMax(),
                    BlendingMode::kSource);
  filter.fill(fill, 2);
  filter.fill(fill, static_cast<uint32_t>(window.area()) - 2);

  FillExpected(expected, exclusion, window, fill,
               static_cast<uint32_t>(window.area()));
  ExpectSameRaster(actual, expected);
}

TEST(RectUnionFilter, FullyCoveredWindowProducesNoOutput) {
  std::array<Box, 1> boxes = {Box(0, 0, 3, 1)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  Box window(0, 0, 3, 1);
  std::vector<Color> colors = MakeGradient(static_cast<size_t>(window.area()));

  TestOffscreen actual(4, 2, color::White);
  TestOffscreen expected(4, 2, color::White);

  RectUnionFilter filter(actual, &exclusion);
  filter.setAddress(window.xMin(), window.yMin(), window.xMax(), window.yMax(),
                    BlendingMode::kSource);
  filter.write(colors.data(), colors.size());

  ExpectSameRaster(actual, expected);
}

TEST(RectUnionFilter, NonIntersectingWindowPassesThrough) {
  std::array<Box, 1> boxes = {Box(6, 1, 7, 1)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  Box window(0, 0, 3, 1);
  std::vector<Color> colors = MakeGradient(static_cast<size_t>(window.area()));

  TestOffscreen actual(4, 2, color::White);
  TestOffscreen expected(4, 2, color::White);

  RectUnionFilter filter(actual, &exclusion);
  filter.setAddress(window.xMin(), window.yMin(), window.xMax(), window.yMax(),
                    BlendingMode::kSource);
  filter.write(colors.data(), colors.size());

  WriteExpected(expected, exclusion, window, colors);
  ExpectSameRaster(actual, expected);
}

// Verifies a left-edge visible batch spanning multiple rows reuses one window.
TEST(RectUnionFilter, WriteMultilineVisibleBatchUsesSingleAddressWindow) {
  std::array<Box, 1> boxes = {Box(2, 2, 3, 2)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  Box window(0, 0, 3, 2);
  std::vector<Color> colors = MakeGradient(static_cast<size_t>(window.area()));

  TestOffscreen actual(4, 3, color::White);
  CountingOutput counted(actual);
  TestOffscreen expected(4, 3, color::White);

  RectUnionFilter filter(counted, &exclusion);
  filter.setAddress(window.xMin(), window.yMin(), window.xMax(), window.yMax(),
                    BlendingMode::kSource);
  filter.write(colors.data(), colors.size());

  WriteExpected(expected, exclusion, window, colors);
  ExpectSameRaster(actual, expected);
  EXPECT_EQ(counted.setAddressCount(), 1);
}

// Verifies a multiline visible batch survives a row wrap across write calls.
TEST(RectUnionFilter, WriteSequentialChunksReuseMultilineVisibleBatch) {
  std::array<Box, 1> boxes = {Box(2, 2, 3, 2)};
  RectUnion exclusion(boxes.data(), boxes.data() + boxes.size());
  Box window(0, 0, 3, 2);
  std::vector<Color> colors = MakeGradient(static_cast<size_t>(window.area()));

  TestOffscreen actual(4, 3, color::White);
  CountingOutput counted(actual);
  TestOffscreen expected(4, 3, color::White);

  RectUnionFilter filter(counted, &exclusion);
  filter.setAddress(window.xMin(), window.yMin(), window.xMax(), window.yMax(),
                    BlendingMode::kSource);
  filter.write(colors.data(), 4);
  filter.write(colors.data() + 4, colors.size() - 4);

  WriteExpected(expected, exclusion, window, colors);
  ExpectSameRaster(actual, expected);
  EXPECT_EQ(counted.setAddressCount(), 1);
}

}  // namespace roo_display