#include "roo_display/driver/common/rgb_bounce_buffer.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "gtest/gtest.h"

namespace roo_display::esp32s3_dma::internal {
namespace {

// Models the native driver's fill requests, including its unreset EOF counter.
// Each buffer holds a source-chunk number instead of a complete pixel array.
class ScanoutModel {
 public:
  ScanoutModel(size_t chunks_per_frame, bool route_by_source)
      : chunks_per_frame_(chunks_per_frame),
        route_by_source_(route_by_source),
        router_(kBouncePixels) {
    fill(&buffers_[0]);
    fill(&buffers_[1]);
  }

  // Advances one frame while dropping the EOF notifications selected by mask.
  void disruptFrame(uint64_t dropped_eofs) {
    for (size_t eof = 0; eof < chunks_per_frame_; ++eof) {
      if (eof >= 64 || (dropped_eofs & (uint64_t{1} << eof)) == 0) {
        completeEof();
      }
    }
  }

  // Drops one or two selected notifications without limiting the frame length.
  void disruptFrameAt(size_t first, size_t second) {
    for (size_t eof = 0; eof < chunks_per_frame_; ++eof) {
      if (eof != first && eof != second) completeEof();
    }
  }

  // Reproduces native VSYNC priming without repairing the native EOF counter.
  void restart() {
    if (next_source_chunk_ > 2) next_source_chunk_ = 0;
    if (next_source_chunk_ < 1) fill(&buffers_[0]);
    if (next_source_chunk_ < 2) fill(&buffers_[1]);
  }

  // Checks every transmitted chunk of the next frame and advances its EOFs.
  bool healthyFrame() {
    bool correct = true;
    for (size_t chunk = 0; chunk < chunks_per_frame_; ++chunk) {
      if (buffers_[chunk & 1] != chunk) correct = false;
      completeEof();
    }
    return correct;
  }

 private:
  void fill(void* supplied) {
    void* destination = supplied;
    if (route_by_source_) {
      destination =
          router_.destination(supplied, next_source_chunk_ * kBouncePixels);
    }
    *static_cast<size_t*>(destination) = next_source_chunk_;
    next_source_chunk_ = (next_source_chunk_ + 1) % chunks_per_frame_;
  }

  void completeEof() {
    const size_t buffer = native_eof_count_ & 1;
    ++native_eof_count_;
    fill(&buffers_[buffer]);
  }

  static constexpr size_t kBouncePixels = 8000;

  size_t chunks_per_frame_;
  bool route_by_source_;
  BounceBufferRouter router_;
  std::array<size_t, 2> buffers_ = {};
  size_t next_source_chunk_ = 0;
  size_t native_eof_count_ = 0;
};

// Verifies the requested priority when a height is divisible by both choices.
TEST(RgbBounceBufferTest, PrefersTenLinesForMultiplesOfTwenty) {
  EXPECT_EQ(SelectBounceBufferLines(20), 10);
  EXPECT_EQ(SelectBounceBufferLines(80), 10);
  EXPECT_EQ(SelectBounceBufferLines(320), 10);
  EXPECT_EQ(SelectBounceBufferLines(480), 10);
  EXPECT_EQ(SelectBounceBufferLines(600), 10);
}

// Verifies the eight-line fallback and rejection of unsupported dimensions.
TEST(RgbBounceBufferTest, SelectsEightLinesOrReportsUnsupportedHeight) {
  EXPECT_EQ(SelectBounceBufferLines(16), 8);
  EXPECT_EQ(SelectBounceBufferLines(272), 8);
  EXPECT_EQ(SelectBounceBufferLines(768), 8);
  EXPECT_EQ(SelectBounceBufferLines(0), 0);
  EXPECT_EQ(SelectBounceBufferLines(10), 0);
  EXPECT_EQ(SelectBounceBufferLines(479), 0);
}

// Verifies that every selected geometry consists of complete pairs of chunks.
TEST(RgbBounceBufferTest, EveryAcceptedHeightHasAnEvenChunkCount) {
  for (uint32_t height = 1; height <= UINT16_MAX; ++height) {
    const uint16_t lines =
        SelectBounceBufferLines(static_cast<uint16_t>(height));
    if (lines == 0) continue;
    EXPECT_EQ(height % lines, 0u) << "height=" << height;
    EXPECT_EQ((height / lines) % 2, 0u) << "height=" << height;
  }
}

// Verifies that a corrupted SDK destination cannot replace a learned buffer.
TEST(RgbBounceBufferTest, RoutesFromSourcePositionAfterInitialPriming) {
  std::array<uint8_t, 8> first = {};
  std::array<uint8_t, 8> second = {};
  std::array<uint8_t, 8> unrelated = {};
  BounceBufferRouter router(4);

  EXPECT_EQ(router.destination(first.data(), 0), first.data());
  EXPECT_EQ(router.destination(second.data(), 4), second.data());
  EXPECT_EQ(router.destination(second.data(), 8), first.data());
  EXPECT_EQ(router.destination(first.data(), 12), second.data());
  EXPECT_EQ(router.destination(unrelated.data(), 0), first.data());
  EXPECT_EQ(router.destination(unrelated.data(), 4), second.data());
}

// Verifies that the test fixture reproduces persistent stock-driver corruption.
TEST(RgbBounceBufferTest, StockDestinationRemainsWrongAfterOneLostEof) {
  ScanoutModel native(48, false);
  ASSERT_TRUE(native.healthyFrame());
  native.restart();
  native.disruptFrame(uint64_t{1} << 17);
  native.restart();
  EXPECT_FALSE(native.healthyFrame());
  native.restart();
  EXPECT_FALSE(native.healthyFrame());
}

// Verifies recovery for every possible lost-EOF combination in small frames.
TEST(RgbBounceBufferTest, RecoversAfterArbitraryMissingEofsInSmallFrames) {
  for (size_t chunks : {2u, 4u, 6u, 8u, 10u}) {
    const uint64_t combinations = uint64_t{1} << chunks;
    for (uint64_t missing = 0; missing < combinations; ++missing) {
      SCOPED_TRACE(::testing::Message()
                   << "chunks=" << chunks << " missing=" << missing);
      ScanoutModel routed(chunks, true);
      routed.disruptFrame(missing);
      routed.restart();
      ASSERT_TRUE(routed.healthyFrame());
      routed.restart();
      ASSERT_TRUE(routed.healthyFrame());
    }
  }
}

// Verifies Makerfabs-sized frames recover for every single or paired EOF loss.
TEST(RgbBounceBufferTest, RecoversMakerfabsFramesAfterSingleAndPairedLosses) {
  for (size_t chunks : {48u, 60u, 120u}) {
    for (size_t first = 0; first < chunks; ++first) {
      for (size_t second = first; second < chunks; ++second) {
        SCOPED_TRACE(::testing::Message() << "chunks=" << chunks << " first="
                                          << first << " second=" << second);
        ScanoutModel routed(chunks, true);
        routed.disruptFrameAt(first, second);
        routed.restart();
        ASSERT_TRUE(routed.healthyFrame());
      }
    }
  }
}

}  // namespace
}  // namespace roo_display::esp32s3_dma::internal
