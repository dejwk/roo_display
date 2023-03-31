#pragma once

#include <Arduino.h>

#if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)

#include <memory>

#include "rom/cache.h"
#include "roo_display/core/device.h"
#include "roo_display/core/offscreen.h"

namespace roo_display {

namespace internal {

class Rgb565Dma : public Rgb565 {};

template <>
class ReplaceWriter<Rgb565Dma, COLOR_PIXEL_ORDER_MSB_FIRST,
                    BYTE_ORDER_LITTLE_ENDIAN, 1, uint16_t> {
 public:
  ReplaceWriter(Rgb565Dma color_mode, const Color *color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    itr.write(color_mode_.fromArgbColor(*color_++));
    Cache_WriteBack_Addr((uint32_t)(p + offset * 2), 2);
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    while (count-- > 0) {
      itr.write(color_mode_.fromArgbColor(*color_++));
      ++itr;
    }
    Cache_WriteBack_Addr((uint32_t)(p + offset * 2), count * 2);
  }

 private:
  Rgb565Dma color_mode_;
  const Color *color_;
};

template <>
class BlendWriter<Rgb565Dma, COLOR_PIXEL_ORDER_MSB_FIRST,
                  BYTE_ORDER_LITTLE_ENDIAN, 1, uint16_t> {
 public:
  BlendWriter(Rgb565Dma color_mode, const Color *color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    itr.write(color_mode_.rawAlphaBlend(itr.read(), *color_++));
    Cache_WriteBack_Addr((uint32_t)(p + offset * 2), 2);
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    while (count-- > 0) {
      itr.write(color_mode_.rawAlphaBlend(itr.read(), *color_++));
      ++itr;
    }
    Cache_WriteBack_Addr((uint32_t)(p + offset * 2), count * 2);
  }

 private:
  Rgb565Dma color_mode_;
  const Color *color_;
};

template <>
class ReplaceFiller<Rgb565Dma, COLOR_PIXEL_ORDER_MSB_FIRST,
                    BYTE_ORDER_LITTLE_ENDIAN, 1, uint16_t> {
 public:
  ReplaceFiller(Rgb565Dma color_mode, Color color) : color_mode_(color_mode) {
    internal::ReadRaw<uint16_t, 2, BYTE_ORDER_LITTLE_ENDIAN>(
        color_mode_.fromArgbColor(color), raw_color_);
  }

  void operator()(uint8_t *p, uint32_t offset) const {
    pattern_write<2>(p + offset * 2, raw_color_);
    Cache_WriteBack_Addr((uint32_t)(p + offset * 2), 2);
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) const {
    pattern_fill<2>(p + offset * 2, count, raw_color_);
    Cache_WriteBack_Addr((uint32_t)(p + offset * 2), count * 2);
  }

 private:
  Rgb565Dma color_mode_;
  uint8_t raw_color_[2];
};

template <>
class BlendFiller<Rgb565Dma, COLOR_PIXEL_ORDER_MSB_FIRST,
                  BYTE_ORDER_LITTLE_ENDIAN, 1, uint16_t> {
 public:
  BlendFiller(Rgb565Dma color_mode, Color color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) const {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    itr.write(color_mode_.rawAlphaBlend(itr.read(), color_));
    Cache_WriteBack_Addr((uint32_t)(p + offset * 2), 2);
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) const {
    internal::RawIterator<16, BYTE_ORDER_LITTLE_ENDIAN> itr(p, offset);
    while (count-- > 0) {
      itr.write(color_mode_.rawAlphaBlend(itr.read(), color_));
      ++itr;
    }
    Cache_WriteBack_Addr((uint32_t)(p + offset * 2), count * 2);
  }

 private:
  Rgb565Dma color_mode_;
  Color color_;
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

uint8_t* AllocateBuffer(const Config& config);

template <FlushMode flush_mode>
class ParallelRgb565 : public DisplayDevice {
 public:
  ParallelRgb565(Config cfg)
      : DisplayDevice(cfg.width, cfg.height), cfg_(std::move(cfg)) {}

  void init() override;

  void begin() override {}

  void end() override;

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  PaintMode mode) override {
    buffer_->setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color *color, uint32_t pixel_count) override;

  void writePixels(PaintMode mode, Color *color, int16_t *x, int16_t *y,
                   uint16_t pixel_count) override;

  void fillPixels(PaintMode mode, Color color, int16_t *x, int16_t *y,
                  uint16_t pixel_count) override;

  void writeRects(PaintMode mode, Color *color, int16_t *x0, int16_t *y0,
                  int16_t *x1, int16_t *y1, uint16_t count) override;

  void fillRects(PaintMode mode, Color color, int16_t *x0, int16_t *y0,
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

  void flush(int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1,
             uint16_t count) {
    bool swapped = orientation().isXYswapped();
    int16_t *begin = swapped ? x0 : y0;
    int16_t *end = swapped ? x1 : y1;
    int16_t ymin = *begin++;
    int16_t ymax = *end++;
    while (--count > 0) {
      ymin = std::min(ymin, *begin++);
      ymax = std::max(ymax, *end++);
    }
    if (orientation().isTopToBottom()) {
      Cache_WriteBack_Addr(
          (uint32_t)(buffer_->buffer() + ymin * cfg_.width * 2),
          (ymax - ymin + 1) * cfg_.width * 2);
    } else {
      Cache_WriteBack_Addr(
          (uint32_t)(buffer_->buffer() +
                     (raw_height() - ymax - 1) * cfg_.width * 2),
          (ymax - ymin + 1) * cfg_.width * 2);
    }
  }

  Config cfg_;
  std::unique_ptr<Dev> buffer_;
};

using ParallelRgb565Buffered = ParallelRgb565<FLUSH_MODE_BUFFERED>;

}  // namespace esp32s3_dma

}  // namespace roo_display

#endif  // #if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)
