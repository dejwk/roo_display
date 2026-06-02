
#include "roo_display/core/streamable.h"

#include <vector>

#include "roo_display/color/color.h"
#include "testing.h"

// Tests drawing and clipping streamables via their drawTo method, which
// internally creates a stream and possibly clips it using SubRectangleStream.

using namespace testing;

namespace roo_display {

namespace {

class ScriptedRunStream : public PixelStream {
 public:
  using PixelStream::read;

  ScriptedRunStream(std::vector<Color> pixels,
                    std::vector<uint32_t> run_lengths)
      : pixels_(std::move(pixels)), run_lengths_(std::move(run_lengths)) {}

  void read(Color* buf, uint16_t size, uint32_t& run_length) override {
    EXPECT_LE(idx_ + size, pixels_.size());
    memcpy(buf, pixels_.data() + idx_, size * sizeof(Color));
    idx_ += size;
    if (read_count_ < run_lengths_.size()) {
      run_length = run_lengths_[read_count_++];
    } else {
      run_length = 0;
    }
  }

  void skip(uint32_t count) override {
    ++skip_calls_;
    skipped_pixels_ += count;
    idx_ += count;
  }

  uint32_t skip_calls() const { return skip_calls_; }
  uint32_t skipped_pixels() const { return skipped_pixels_; }

 private:
  std::vector<Color> pixels_;
  std::vector<uint32_t> run_lengths_;
  size_t idx_ = 0;
  size_t read_count_ = 0;
  uint32_t skip_calls_ = 0;
  uint32_t skipped_pixels_ = 0;
};

class CountingOffscreen : public FakeOffscreen<Argb8888> {
 public:
  using Base = FakeOffscreen<Argb8888>;
  using Base::Base;

  void write(Color* color, uint32_t pixel_count) override {
    ++write_calls_;
    Base::write(color, pixel_count);
  }

  void fill(Color color, uint32_t pixel_count) override {
    ++fill_calls_;
    fill_lengths_.push_back(pixel_count);
    fill_colors_.push_back(color);
    DisplayOutput::fill(color, pixel_count);
  }

  uint32_t write_calls() const { return write_calls_; }
  uint32_t fill_calls() const { return fill_calls_; }
  const std::vector<uint32_t>& fill_lengths() const { return fill_lengths_; }
  const std::vector<Color>& fill_colors() const { return fill_colors_; }

 private:
  uint32_t write_calls_ = 0;
  uint32_t fill_calls_ = 0;
  std::vector<uint32_t> fill_lengths_;
  std::vector<Color> fill_colors_;
};

void ExpectBufferEquals(const CountingOffscreen& output,
                        const std::vector<Color>& expected) {
  ASSERT_EQ(expected.size(),
            static_cast<size_t>(output.raw_width()) * output.raw_height());
  const Color* actual = output.buffer();
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

}  // namespace

void Draw(DisplayDevice& output, int16_t x, int16_t y, const Box& clip_box,
          const Streamable& object, FillMode fill_mode = FillMode::kVisible,
          BlendingMode blending_mode = BlendingMode::kSourceOver,
          Color bgcolor = color::Transparent) {
  output.begin();
  Surface s(output, x, y, clip_box, false, bgcolor, fill_mode, blending_mode);
  s.drawObject(object);
  output.end();
}

void Draw(DisplayDevice& output, int16_t x, int16_t y, const Streamable& object,
          FillMode fill_mode = FillMode::kVisible,
          BlendingMode blending_mode = BlendingMode::kSourceOver,
          Color bgcolor = color::Transparent) {
  Box clip_box(0, 0, output.effective_width() - 1,
               output.effective_height() - 1);
  Draw(output, x, y, clip_box, object, fill_mode, blending_mode, bgcolor);
}

TEST(Streamable, DrawingArbitraryStreamable) {
  auto input = MakeTestStreamable(Rgb565(), Box(0, 0, 1, 2),
                                  "1W3 D1R"
                                  "LQD TOL"
                                  "F9F N_N");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(test_screen, 1, 2, input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ 1W3 D1R ___ ___"
                                          "___ LQD TOL ___ ___"
                                          "___ F9F N_N ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, ClippingFromTop) {
  auto input = MakeTestStreamable(Rgb565(), Box(0, 0, 1, 2),
                                  "1W3 D1R"
                                  "LQD TOL"
                                  "F9F N_N");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(test_screen, 1, 2, Box(2, 3, 4, 5), input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ TOL ___ ___"
                                          "___ ___ N_N ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, ClippingFromBottom) {
  auto input = MakeTestStreamable(Rgb565(), Box(0, 0, 1, 2),
                                  "1W3 D1R"
                                  "LQD TOL"
                                  "F9F N_N");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(test_screen, 1, 2, Box(0, 0, 1, 3), input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ 1W3 ___ ___ ___"
                                          "___ LQD ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, ClippingFromSeveralSides) {
  auto input = MakeTestStreamable(Rgb565(), Box(0, 0, 3, 2),
                                  "1W3 D1R CA4 B11"
                                  "LQD TOL 114 5AF"
                                  "F9F N_N F45 567");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(test_screen, 1, 2, Box(2, 3, 3, 5), input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ TOL 114 ___"
                                          "___ ___ N_N F45 ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, ClippingFromLeft) {
  auto input = MakeTestStreamable(Rgb565(), Box(0, 0, 3, 2),
                                  "1W3 D1R CA4 B11"
                                  "LQD TOL 114 5AF"
                                  "F9F N_N F45 567");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(test_screen, -2, 2, Box(1, 0, 1, 5), input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ B11 ___ ___ ___"
                                          "___ 5AF ___ ___ ___"
                                          "___ 567 ___ ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, ClippingFromRight) {
  auto input = MakeTestStreamable(Rgb565(), Box(0, 0, 2, 2),
                                  "1W3 D1R CA4"
                                  "LOD TOL 114"
                                  "F9F N_N F45");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(test_screen, 1, 2, Box(1, 0, 1, 5), input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ 1W3 ___ ___ ___"
                                          "___ LOD ___ ___ ___"
                                          "___ F9F ___ ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, Transparency) {
  auto input = MakeTestStreamable(Rgb565WithTransparency(1), Box(0, 0, 1, 2),
                                  "... D1R"
                                  "LQD TOL"
                                  "F9F ...");
  FakeOffscreen<Rgb565> test_screen(5, 6, color::Black);
  Draw(test_screen, 1, 2, input);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 5, 6,
                                          "___ ___ ___ ___ ___"
                                          "___ ___ ___ ___ ___"
                                          "___ ___ D1R ___ ___"
                                          "___ LQD TOL ___ ___"
                                          "___ F9F ___ ___ ___"
                                          "___ ___ ___ ___ ___"));
}

TEST(Streamable, AlphaTransparency) {
  auto input =
      MakeTestStreamable(Argb4444(), Box(0, 0, 3, 0), "4488 F678 F1A3 73E3");
  FakeOffscreen<Argb4444> test_screen(6, 1, color::Black);
  Draw(test_screen, 1, 0, input);
  EXPECT_THAT(test_screen, MatchesContent(Argb4444(), 6, 1,
                                          "F000 F122 F678 F1A3 F161 F000"));
}

TEST(Streamable, AlphaTransparencyOverriddenReplace) {
  auto input =
      MakeTestStreamable(Argb4444(), Box(0, 0, 3, 0), "4488 F678 F1A3 73E3");
  FakeOffscreen<Argb4444> test_screen(6, 1, color::Black);
  Draw(test_screen, 1, 0, input, FillMode::kVisible, BlendingMode::kSource);
  EXPECT_THAT(test_screen, MatchesContent(Argb4444(), 6, 1,
                                          "F000 4488 F678 F1A3 73E3 F000"));
}

TEST(Streamable, AlphaTransparencyFillWithOpaqueBackground) {
  auto input =
      MakeTestStreamable(Argb4444(), Box(0, 0, 1, 1), "4488 F678 73E3 0000");
  FakeOffscreen<Argb4444> test_screen(3, 2, color::Black);
  Draw(test_screen, 1, 0, input, FillMode::kExtents, BlendingMode::kSourceOver,
       color::White);
  EXPECT_THAT(test_screen, MatchesContent(Argb4444(), 3, 2,
                                          "F000 FCDD F678"
                                          "F000 F9F9 FFFF"));
}

TEST(Streamable, AlphaTransparencyFillWithTranslucentBackground) {
  auto input =
      MakeTestStreamable(Argb4444(), Box(0, 0, 1, 1), "4488 F678 73E3 0000");
  FakeOffscreen<Argb4444> test_screen(3, 2, color::Black);
  Draw(test_screen, 1, 0, input, FillMode::kExtents, BlendingMode::kSourceOver,
       Color(0x7FFFFFFF));
  EXPECT_THAT(test_screen, MatchesContent(Argb4444(), 3, 2,
                                          "F000 F677 F678"
                                          "F000 F5A5 F777"));
}

TEST(Streamable, AlphaTransparencyWriteWithOpaqueBackground) {
  auto input =
      MakeTestStreamable(Argb4444(), Box(0, 0, 1, 1), "4488 F678 73E3 0000");
  FakeOffscreen<Argb4444> test_screen(3, 2, color::Black);
  Draw(test_screen, 1, 0, input, FillMode::kVisible, BlendingMode::kSourceOver,
       color::White);
  EXPECT_THAT(test_screen, MatchesContent(Argb4444(), 3, 2,
                                          "F000 FCDD F678"
                                          "F000 F9F9 F000"));
}

TEST(Streamable, AlphaTransparencyWriteWithTranslucentBackground) {
  auto input =
      MakeTestStreamable(Argb4444(), Box(0, 0, 1, 1), "4488 F678 73E3 0000");
  FakeOffscreen<Argb4444> test_screen(3, 2, color::Black);
  Draw(test_screen, 1, 0, input, FillMode::kVisible, BlendingMode::kSourceOver,
       Color(0x7FFFFFFF));
  EXPECT_THAT(test_screen, MatchesContent(Argb4444(), 3, 2,
                                          "F000 F677 F678"
                                          "F000 F5A5 F000"));
}

TEST(Streamable, FillReplaceRectUsesSingleFillAndSkipForLongExactRun) {
  std::vector<Color> source(100, Color(0xFF336699));
  for (int i = 90; i < 100; ++i) {
    source[i] = Color(0xFF550000 + i);
  }
  ScriptedRunStream stream(source, {90, 0});
  CountingOffscreen output(100, 1, color::Transparent);

  internal::fillReplaceRect(output, Box(0, 0, 99, 0), &stream,
                            BlendingMode::kSource);

  ASSERT_EQ(output.fill_calls(), 1u);
  EXPECT_EQ(output.fill_lengths()[0], 90u);
  EXPECT_EQ(stream.skip_calls(), 1u);
  EXPECT_EQ(stream.skipped_pixels(),
            static_cast<uint32_t>(90 - kPixelWritingBufferSize));
  ExpectBufferEquals(output, source);
}

TEST(Streamable, FillPaintRectOverBgUsesSingleFillForLongExactRun) {
  Color bg = Color(0x7F010203);
  Color prefix_source = Color(0x80405060);
  std::vector<Color> source(100, prefix_source);
  for (int i = 90; i < 100; ++i) {
    source[i] = Color(0x70010200 + i);
  }
  ScriptedRunStream stream(source, {90, 0});
  CountingOffscreen output(100, 1, color::Transparent);

  internal::fillPaintRectOverBg(output, Box(0, 0, 99, 0), bg, &stream,
                                BlendingMode::kSource);

  std::vector<Color> expected = source;
  for (int i = 0; i < 90; ++i) {
    expected[i] = AlphaBlend(bg, prefix_source);
  }
  for (int i = 90; i < 100; ++i) {
    expected[i] = AlphaBlend(bg, source[i]);
  }

  ASSERT_EQ(output.fill_calls(), 1u);
  EXPECT_EQ(output.fill_lengths()[0], 90u);
  EXPECT_EQ(output.fill_colors()[0], AlphaBlend(bg, prefix_source));
  EXPECT_EQ(stream.skip_calls(), 1u);
  EXPECT_EQ(stream.skipped_pixels(),
            static_cast<uint32_t>(90 - kPixelWritingBufferSize));
  ExpectBufferEquals(output, expected);
}

}  // namespace roo_display
