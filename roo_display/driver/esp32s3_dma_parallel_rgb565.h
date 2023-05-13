#pragma once

#include <Arduino.h>

#if !defined(ESP32) || !(CONFIG_IDF_TARGET_ESP32S3)
#error Compilation target must be ESP32_S3 for this device.
#else

#include <memory>

#include "rom/cache.h"
#include "roo_display/color/blending.h"
#include "roo_display/core/device.h"
#include "roo_display/core/offscreen.h"

namespace roo_display {

namespace internal {

class Rgb565Dma : public Rgb565 {};

}  // namespace internal

template <>
struct RawBlender<internal::Rgb565Dma, BLENDING_MODE_SOURCE_OVER> {
  inline uint16_t operator()(uint16_t bg, Color color,
                             const internal::Rgb565Dma &mode) const {
    return mode.fromArgbColor(
        AlphaBlendOverOpaque(mode.toArgbColor(bg), color));
  }
};

namespace internal {

template <BlendingMode blending_mode>
class Rgb565DmaBlendingWriterOperator {
 public:
  Rgb565DmaBlendingWriterOperator(Rgb565Dma color_mode, const Color *color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    RawBlender<Rgb565Dma, blending_mode> blender;
    itr.write(blender(itr.read(), *color_++, color_mode_));
    Cache_WriteBack_Addr((uint32_t)(p + offset * 2), 2);
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    RawBlender<Rgb565Dma, blending_mode> blender;
    while (count-- > 0) {
      itr.write(blender(itr.read(), *color_++, color_mode_));
      ++itr;
    }
    Cache_WriteBack_Addr((uint32_t)(p + offset * 2), count * 2);
  }

 private:
  Rgb565Dma color_mode_;
  const Color *color_;
};

template <>
struct BlendingWriter<Rgb565Dma, COLOR_PIXEL_ORDER_MSB_FIRST, BYTE_ORDER_LITTLE_ENDIAN> {
  template <BlendingMode blending_mode>
  using Operator =
      Rgb565DmaBlendingWriterOperator<blending_mode>;
};

template <>
class GenericWriter<Rgb565Dma, COLOR_PIXEL_ORDER_MSB_FIRST,
                    BYTE_ORDER_LITTLE_ENDIAN, 1, uint16_t> {
 public:
  GenericWriter(const Rgb565Dma &color_mode, BlendingMode blending_mode,
                Color *color)
      : color_(color), blending_mode_(blending_mode) {}

  void operator()(uint8_t *p, uint32_t offset) {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    itr.write(
        ApplyRawBlending(blending_mode_, itr.read(), *color_++, Rgb565Dma()));
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    while (count-- > 0) {
      itr.write(ApplyRawBlending(blending_mode_, itr.read(), *color_++, Rgb565Dma()));
      ++itr;
    }
  }

 private:
  Color *color_;
  BlendingMode blending_mode_;
};

template <BlendingMode blending_mode>
class Rgb565DmaBlendingFillerOperator {
 public:
  Rgb565DmaBlendingFillerOperator(Rgb565Dma color_mode, Color color)
      : color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) const {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    RawBlender<Rgb565Dma, blending_mode> blender;
    itr.write(blender(itr.read(), color_, Rgb565Dma()));
    Cache_WriteBack_Addr((uint32_t)(p + offset * 2), 2);
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) const {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    RawBlender<Rgb565Dma, blending_mode> blender;
    while (count-- > 0) {
      itr.write(blender(itr.read(), color_, Rgb565Dma()));
      ++itr;
    }
    Cache_WriteBack_Addr((uint32_t)(p + offset * 2), count * 2);
  }

 private:
  Color color_;
  BlendingMode blending_mode_;
};

template <>
struct BlendingFiller<Rgb565Dma, COLOR_PIXEL_ORDER_MSB_FIRST, BYTE_ORDER_LITTLE_ENDIAN> {
  template <BlendingMode blending_mode>
  using Operator =
      Rgb565DmaBlendingFillerOperator<blending_mode>;
};

template <>
class GenericFiller<Rgb565Dma, COLOR_PIXEL_ORDER_MSB_FIRST,
                    BYTE_ORDER_LITTLE_ENDIAN, 1, uint16_t> {
 public:
  GenericFiller(const Rgb565Dma &color_mode, BlendingMode blending_mode,
                Color color)
      : color_(color), blending_mode_(blending_mode) {}

  void operator()(uint8_t *p, uint32_t offset) const {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    itr.write(
        ApplyRawBlending(blending_mode_, itr.read(), color_, Rgb565Dma()));
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) const {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    while (count-- > 0) {
      itr.write(
          ApplyRawBlending(blending_mode_, itr.read(), color_, Rgb565Dma()));
      ++itr;
    }
  }

 private:
  Color color_;
  BlendingMode blending_mode_;
};

}  // namespace internal

// Note: this one is mainly for performance tests and it doesn't support screen
// rotation or alpha-blending.
// #define FLUSH_MODE_HARDCODED

namespace esp32s3_dma {

enum FlushMode {
  // Writes back after every memory write. Slow.
  FLUSH_MODE_AGGRESSIVE = 0,

  // Writes back after every driver call, i.e. per some number of pixels or
  // rectangles.
  FLUSH_MODE_BUFFERED = 1,

  // Only writes back at the end of transaction (in device.end()). In theory,
  // that sounds like good
  // idea. And indeed, it seems the fastest. In practice, for some reason it
  // causes displays to lose synchronization quite a lot.
  FLUSH_MODE_LAZY = 2,
};

namespace internal {

template <FlushMode>
struct Traits {
  using ColorMode = Rgb565;
};

template <>
struct Traits<FLUSH_MODE_AGGRESSIVE> {
  using ColorMode = ::roo_display::internal::Rgb565Dma;
};

}  // namespace internal

struct Config {
  uint16_t width;
  uint16_t height;
  int8_t de;
  int8_t hsync;
  int8_t vsync;
  int8_t pclk;
  uint16_t hsync_pulse_width;
  uint16_t hsync_back_porch;
  uint16_t hsync_front_porch;
  uint16_t hsync_polarity;
  uint16_t vsync_pulse_width;
  uint16_t vsync_back_porch;
  uint16_t vsync_front_porch;
  uint16_t vsync_polarity;
  uint16_t pclk_active_neg;
  int32_t prefer_speed;

  int8_t r0;
  int8_t r1;
  int8_t r2;
  int8_t r3;
  int8_t r4;
  int8_t g0;
  int8_t g1;
  int8_t g2;
  int8_t g3;
  int8_t g4;
  int8_t g5;
  int8_t b0;
  int8_t b1;
  int8_t b2;
  int8_t b3;
  int8_t b4;
  bool bswap;
};

uint8_t *AllocateBuffer(const Config &config);

template <FlushMode flush_mode>
class ParallelRgb565 : public DisplayDevice {
 public:
  ParallelRgb565(Config cfg)
      : DisplayDevice(cfg.width, cfg.height), cfg_(std::move(cfg)) {}

  void init() override;

  void begin() override {}

  void end() override;

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    buffer_->setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color *color, uint32_t pixel_count) override;

  void writePixels(BlendingMode mode, Color *color, int16_t *x, int16_t *y,
                   uint16_t pixel_count) override;

  void fillPixels(BlendingMode mode, Color color, int16_t *x, int16_t *y,
                  uint16_t pixel_count) override;

  void writeRects(BlendingMode mode, Color *color, int16_t *x0, int16_t *y0,
                  int16_t *x1, int16_t *y1, uint16_t count) override;

  void fillRects(BlendingMode mode, Color color, int16_t *x0, int16_t *y0,
                 int16_t *x1, int16_t *y1, uint16_t count) override;

  void orientationUpdated() override {
    if (buffer_ != nullptr) {
      buffer_->orientationUpdated();
    }
  }

 private:
  using Dev =
      OffscreenDevice<typename internal::Traits<flush_mode>::ColorMode,
                      COLOR_PIXEL_ORDER_MSB_FIRST, BYTE_ORDER_LITTLE_ENDIAN>;

  Config cfg_;
  std::unique_ptr<Dev> buffer_;
};

using ParallelRgb565Buffered = ParallelRgb565<FLUSH_MODE_BUFFERED>;

}  // namespace esp32s3_dma

}  // namespace roo_display

#endif  // #if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)
