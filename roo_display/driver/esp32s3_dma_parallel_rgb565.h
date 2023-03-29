#pragma once

#include <Arduino.h>

#if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)

#include <memory>

#include "roo_display/core/device.h"
#include "roo_display/core/offscreen.h"

namespace roo_display {

namespace esp32s3_dma {

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

  void write(Color *color, uint32_t pixel_count) override {
    buffer_->write(color, pixel_count);
  }

  void writePixels(PaintMode mode, Color *color, int16_t *x, int16_t *y,
                   uint16_t pixel_count) override {
    buffer_->writePixels(mode, color, x, y, pixel_count);
  }

  void fillPixels(PaintMode mode, Color color, int16_t *x, int16_t *y,
                  uint16_t pixel_count) override {
    buffer_->fillPixels(mode, color, x, y, pixel_count);
  }

  void writeRects(PaintMode mode, Color *color, int16_t *x0, int16_t *y0,
                  int16_t *x1, int16_t *y1, uint16_t count) override {
    buffer_->writeRects(mode, color, x0, y0, x1, y1, count);
  }

  void fillRects(PaintMode mode, Color color, int16_t *x0, int16_t *y0,
                 int16_t *x1, int16_t *y1, uint16_t count) override {
    buffer_->fillRects(mode, color, x0, y0, x1, y1, count);
  }

  void orientationUpdated() override { buffer_->orientationUpdated(); }

 private:
  using Dev = OffscreenDevice<Rgb565, COLOR_PIXEL_ORDER_MSB_FIRST,
                              BYTE_ORDER_LITTLE_ENDIAN>;

  Config cfg_;
  std::unique_ptr<Dev> buffer_;
};

}  // namespace esp32s3_dma

}  // namespace roo_display

#endif  // #if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)
