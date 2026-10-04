
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

TEST(Streamable, FillReplaceRectClampsUnlimitedRunLengthToRemainingArea) {
  std::vector<Color> source(100, Color(0xFF336699));
  ScriptedRunStream stream(source, {PixelStream::kUnlimitedRunLength});
  CountingOffscreen output(100, 1, color::Transparent);

  internal::fillReplaceRect(output, Box(0, 0, 99, 0), &stream,
                            BlendingMode::kSource);

  ASSERT_EQ(output.fill_calls(), 1u);
  EXPECT_EQ(output.fill_lengths()[0], 100u);
  EXPECT_EQ(stream.skip_calls(), 1u);
  EXPECT_EQ(stream.skipped_pixels(),
            static_cast<uint32_t>(100 - kPixelWritingBufferSize));
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

TEST(Streamable, SubRectangleStreamTruncatesRunAtRowBoundary) {
  std::vector<Color> source(12, Color(0xFF204060));
  ScriptedRunStream stream(source, {PixelStream::kUnlimitedRunLength});
  auto clipped = internal::MakeSubRectangle(std::move(stream), Box(0, 0, 3, 2),
                                            Box(1, 0, 2, 1));

  Color buf[4];
  uint32_t run_length = 0;
  clipped.read(buf, 4, run_length);

  EXPECT_EQ(run_length, 2u);
  EXPECT_EQ(buf[0], source[1]);
  EXPECT_EQ(buf[1], source[2]);
  EXPECT_EQ(buf[2], source[5]);
  EXPECT_EQ(buf[3], source[6]);
}

TEST(Streamable, SubRectangleStreamPreservesDelegateZeroRunMetadata) {
  std::vector<Color> source(12, Color(0xFF204060));
  ScriptedRunStream stream(source, {0});
  auto clipped = internal::MakeSubRectangle(std::move(stream), Box(0, 0, 3, 2),
                                            Box(1, 0, 2, 1));

  Color buf[2];
  uint32_t run_length = 0;
  clipped.read(buf, 2, run_length);

  EXPECT_EQ(run_length, 0u);
}

namespace {

class RunTrackingStream : public PixelStream {
 public:
  RunTrackingStream(const std::vector<Color>& pixels, bool report_runs)
      : pixels_(pixels), report_runs_(report_runs) {}

  void read(Color* result, uint16_t count, uint32_t& run) override {
    ASSERT_LE(position + count, pixels_.size());
    run = 0;
    if (report_runs_ && count > 0) {
      while (position + run < pixels_.size() &&
             pixels_[position + run] == pixels_[position])
        ++run;
    }
    std::copy_n(pixels_.data() + position, count, result);
    position += count;
    sampled += count;
  }

  void skip(uint32_t count) override {
    ASSERT_LE(position + count, pixels_.size());
    position += count;
    skipped += count;
  }

  size_t position = 0;
  size_t sampled = 0;
  size_t skipped = 0;

 private:
  const std::vector<Color>& pixels_;
  bool report_runs_;
};

}  // namespace

// Verifies uniform masks avoid delegate refills while retaining exact ordinary
// blend semantics, including alpha-zero RGB and the Background sentinel.
TEST(Streamable, BufferedUniformMasksReuseSamplesForNonuniformContent) {
  const int count = kPixelWritingBufferSize * 5 + 3;
  const Color colors[] = {color::Transparent, color::Background,
                          Color(0x00123456), Color(0x80987654), color::Blue};
  for (Color mask : {color::White, color::Transparent, color::Background,
                     Color(0x00123456), Color(0x80543210)}) {
    for (BlendingMode mode :
         {BlendingMode::kSource, BlendingMode::kSourceOver,
          BlendingMode::kSourceIn, BlendingMode::kSourceOut,
          BlendingMode::kSourceAtop, BlendingMode::kDestination,
          BlendingMode::kDestinationOver, BlendingMode::kDestinationIn,
          BlendingMode::kDestinationOut, BlendingMode::kDestinationAtop,
          BlendingMode::kClear, BlendingMode::kXor}) {
      std::vector<Color> pixels(count, mask);
      auto delegate = std::make_unique<RunTrackingStream>(pixels, true);
      RunTrackingStream* trace = delegate.get();
      internal::BufferingStream stream(std::move(delegate), count);
      std::vector<Color> result(count);
      for (int i = 0; i < count; ++i) result[i] = colors[i % 5];
      for (int offset = 0; offset < count;) {
        int batch = std::min(count - offset, kPixelWritingBufferSize + 3);
        stream.blend(result.data() + offset, batch, mode);
        offset += batch;
      }
      for (int i = 0; i < count; ++i) {
        EXPECT_EQ(result[i], ApplyBlending(mode, colors[i % 5], mask));
      }
      EXPECT_EQ(trace->sampled, kPixelWritingBufferSize);
      EXPECT_EQ(trace->skipped,
                static_cast<size_t>(count - kPixelWritingBufferSize));
      EXPECT_EQ(trace->position, static_cast<size_t>(count));
    }
  }
}

// Verifies replay stops at finite run boundaries, survives explicit skips and
// mixed read/next/blend calls, and also accepts sources with unknown runs.
TEST(Streamable, BufferedRunReplayPreservesMixedAccessAndBoundaries) {
  std::vector<Color> pixels(15 * kPixelWritingBufferSize + 17);
  const Color colors[] = {color::Background, color::Blue, Color(0x80543210),
                          Color(0x00123456), color::Transparent};
  for (size_t i = 0; i < pixels.size(); ++i) {
    pixels[i] = colors[(i / (3 * kPixelWritingBufferSize + 1)) % 5];
  }
  for (bool report_runs : {false, true}) {
    auto delegate = std::make_unique<RunTrackingStream>(pixels, report_runs);
    internal::BufferingStream stream(std::move(delegate), pixels.size());
    size_t offset = 0;
    while (offset < pixels.size()) {
      EXPECT_EQ(stream.next(), pixels[offset++]);
      int count =
          std::min<size_t>(kPixelWritingBufferSize + 7, pixels.size() - offset);
      std::vector<Color> result(count);
      uint32_t run = 0;
      stream.read(result.data(), count, run);
      for (int i = 0; i < count; ++i) EXPECT_EQ(result[i], pixels[offset + i]);
      for (size_t i = 0; i < std::min<size_t>(run, pixels.size() - offset);
           ++i) {
        EXPECT_EQ(pixels[offset + i], pixels[offset]);
      }
      offset += count;
      size_t skip = std::min<size_t>(offset % 41, pixels.size() - offset);
      stream.skip(skip);
      offset += skip;
      count =
          std::min<size_t>(kPixelWritingBufferSize + 2, pixels.size() - offset);
      result.assign(count, Color(0x80765432));
      stream.blend(result.data(), count, BlendingMode::kDestinationIn);
      for (int i = 0; i < count; ++i) {
        EXPECT_EQ(result[i],
                  ApplyBlending(BlendingMode::kDestinationIn, Color(0x80765432),
                                pixels[offset + i]));
      }
      offset += count;
    }
  }
}

}  // namespace roo_display
