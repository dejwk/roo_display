#pragma once

// Optimized 50%-alpha scrim fill operators for opaque color modes.
//
// When the overlay alpha is exactly 0x80, the blended result for each channel
// is floor((src + dst) / 2), computed without unpacking via the identity:
//   avg(a, b) = (a & b) + (((a ^ b) >> 1) & mask)
// where 'mask' clears the MSB of each channel to prevent carry leaking.
//
// Each specialization of ScrimFiller<ColorMode, byte_order> provides:
//   static constexpr bool kSupported = true;
//   ScrimFiller(Color color);                        // stores raw scrim
//   void operator()(roo::byte* p, uint32_t count);   // blends count pixels

#include "roo_backport/byte.h"
#include "roo_display/color/color.h"
#include "roo_display/color/color_modes.h"
#include "roo_io/data/byte_order.h"

namespace roo_display {

// Primary template: scrim not supported for this color mode.
template <typename ColorMode, roo_io::ByteOrder byte_order>
struct ScrimFiller {
  static constexpr bool kSupported = false;
};

// Grayscale8: single channel, byte order irrelevant.
template <roo_io::ByteOrder byte_order>
struct ScrimFiller<Grayscale8, byte_order> {
  static constexpr bool kSupported = true;

  ScrimFiller() = default;

  ScrimFiller(Color color)
      : scrim_(Grayscale8().fromArgbColor(
            Color(color.r(), color.g(), color.b()))) {}

  void operator()(roo::byte* p, uint32_t count) const {
    while (count-- > 0) {
      uint8_t dst = static_cast<uint8_t>(*p);
      *p = static_cast<roo::byte>((scrim_ & dst) +
                                  (((scrim_ ^ dst) >> 1) & 0x7F));
      ++p;
    }
  }

  uint8_t scrim_;
};

// Rgb565: always blend in native byte order. For native-endian buffers,
// Vectorizes two pixels at a time via uint32_t. For cross-endian buffers,
// toh/hto on uint32_t swaps pixel order, but hto restores it, and since both
// halves blend with the same scrim it doesn't matter. For native endian,
// toh/hto compile to no-ops.
template <roo_io::ByteOrder byte_order>
struct ScrimFiller<Rgb565, byte_order> {
  static constexpr bool kSupported = true;
  static constexpr uint16_t kMask16 = 0x7BEF;
  static constexpr uint32_t kMask32 = 0x7BEF7BEF;

  ScrimFiller() = default;
  ScrimFiller(Color color)
      : scrim16_(
            Rgb565().fromArgbColor(Color(color.r(), color.g(), color.b()))),
        scrim32_((uint32_t)scrim16_ << 16 | scrim16_) {}

  void operator()(roo::byte* p, uint32_t count) const {
    uint16_t* cursor = reinterpret_cast<uint16_t*>(p);
    // Align to 4-byte boundary.
    if ((reinterpret_cast<uintptr_t>(cursor) & 2) && count > 0) {
      uint16_t dst = roo_io::toh<uint16_t, byte_order>(*cursor);
      *cursor = roo_io::hto<uint16_t, byte_order>(
          (scrim16_ & dst) + (((scrim16_ ^ dst) >> 1) & kMask16));
      ++cursor;
      --count;
    }
    // Process two pixels at a time.
    uint32_t* cursor32 = reinterpret_cast<uint32_t*>(cursor);
    while (count >= 2) {
      uint32_t dst = roo_io::toh<uint32_t, byte_order>(*cursor32);
      *cursor32 = roo_io::hto<uint32_t, byte_order>(
          (scrim32_ & dst) + (((scrim32_ ^ dst) >> 1) & kMask32));
      ++cursor32;
      count -= 2;
    }
    // Handle trailing pixel.
    if (count > 0) {
      uint16_t* tail = reinterpret_cast<uint16_t*>(cursor32);
      uint16_t dst = roo_io::toh<uint16_t, byte_order>(*tail);
      *tail = roo_io::hto<uint16_t, byte_order>(
          (scrim16_ & dst) + (((scrim16_ ^ dst) >> 1) & kMask16));
    }
  }

  uint16_t scrim16_;  // native byte order
  uint32_t scrim32_;  // scrim16_ duplicated into both halves
};

// Rgb888: byte-by-byte, 0x7F mask per channel.
template <roo_io::ByteOrder byte_order>
struct ScrimFiller<Rgb888, byte_order> {
  static constexpr bool kSupported = true;

  ScrimFiller() = default;

  ScrimFiller(Color color) {
    uint32_t raw =
        Rgb888().fromArgbColor(Color(color.r(), color.g(), color.b()));
    if constexpr (byte_order == roo_io::kBigEndian) {
      s_[0] = raw >> 16;
      s_[1] = raw >> 8;
      s_[2] = raw;
    } else {
      s_[0] = raw;
      s_[1] = raw >> 8;
      s_[2] = raw >> 16;
    }
  }

  void operator()(roo::byte* p, uint32_t count) const {
    while (count-- > 0) {
      for (int i = 0; i < 3; ++i) {
        uint8_t dst = static_cast<uint8_t>(p[i]);
        p[i] = static_cast<roo::byte>((s_[i] & dst) +
                                      (((s_[i] ^ dst) >> 1) & 0x7F));
      }
      p += 3;
    }
  }

  uint8_t s_[3];
};

}  // namespace roo_display
