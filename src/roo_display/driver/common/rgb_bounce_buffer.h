#pragma once

#include <cstddef>
#include <cstdint>

namespace roo_display::esp32s3_dma::internal {

// Selects whole scanlines with an even number of bounce chunks per frame.
// Zero signals an unsupported height to the initialization code.
constexpr uint16_t SelectBounceBufferLines(uint16_t height) {
  if (height != 0 && height % 20 == 0) return 10;
  if (height != 0 && height % 16 == 0) return 8;
  return 0;
}

/// Routes refills using the source cursor repaired by SDK restart priming.
/// Bypasses SDK EOF counters that can retain the wrong destination parity.
class BounceBufferRouter {
 public:
  /// Creates an unbound router for equally sized bounce buffers.
  explicit BounceBufferRouter(size_t bounce_pixels)
      : bounce_pixels_(bounce_pixels) {}

  /// Learns buffers during initialization and selects by source-chunk parity.
  /// Requires an even number of chunks per frame and serialized SDK fills.
  __attribute__((always_inline)) void* destination(void* supplied,
                                                   size_t pos_px) {
    size_t index = (pos_px / bounce_pixels_) & 1;
    if (buffers_[index] == nullptr) buffers_[index] = supplied;
    return buffers_[index];
  }

  /// Returns the pixel count of each bounce buffer.
  __attribute__((always_inline)) size_t bounce_pixels() const {
    return bounce_pixels_;
  }

 private:
  size_t bounce_pixels_;
  void* buffers_[2] = {nullptr, nullptr};
};

}  // namespace roo_display::esp32s3_dma::internal
