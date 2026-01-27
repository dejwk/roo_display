// #include <Arduino.h>

#if defined(ESP_PLATFORM)
#include "sdkconfig.h"

#if CONFIG_IDF_TARGET_ESP32S3

#include "esp_err.h"
#include "esp_idf_version.h"
#include "esp_lcd_panel_ops.h"
#include "roo_backport.h"
#include "roo_backport/byte.h"
#include "roo_display/driver/esp32s3_dma_parallel_rgb565.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 0)
#define LEGACY_RGBPANEL
#endif

#ifdef LEGACY_RGBPANEL
#include "roo_display/driver/common/rgb_panel.h"
#else
#include "esp_lcd_panel_rgb.h"
#endif

namespace roo_display {

namespace esp32s3_dma {

roo::byte *AllocateBuffer(const Config &config) {
  esp_lcd_rgb_panel_config_t *cfg =
      (esp_lcd_rgb_panel_config_t *)heap_caps_calloc(
          1, sizeof(esp_lcd_rgb_panel_config_t),
          MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  cfg->clk_src = LCD_CLK_SRC_PLL160M;

#ifdef LEGACY_RGBPANEL
#ifdef CONFIG_SPIRAM_MODE_QUAD
  int32_t default_speed = 6500000L;
#else
  int32_t default_speed = 13000000L;
#endif
#else
  // #ifdef CONFIG_SPIRAM_MODE_QUAD
  //   int32_t default_speed = 10000000L;
  // #else
  int32_t default_speed = 18000000L;
// #endif
#endif

  cfg->timings.pclk_hz =
      (config.prefer_speed == -1) ? default_speed : config.prefer_speed;
  cfg->timings.h_res = config.width;
  cfg->timings.v_res = config.height;
  // The following parameters should refer to LCD spec.
  cfg->timings.hsync_pulse_width = config.hsync_pulse_width;
  cfg->timings.hsync_back_porch = config.hsync_back_porch;
  cfg->timings.hsync_front_porch = config.hsync_front_porch;
  cfg->timings.vsync_pulse_width = config.vsync_pulse_width;
  cfg->timings.vsync_back_porch = config.vsync_back_porch;
  cfg->timings.vsync_front_porch = config.vsync_front_porch;
  cfg->timings.flags.hsync_idle_low = (config.hsync_polarity == 0) ? 1 : 0;
  cfg->timings.flags.vsync_idle_low = (config.vsync_polarity == 0) ? 1 : 0;
  cfg->timings.flags.de_idle_high = 0;
  cfg->timings.flags.pclk_active_neg = config.pclk_active_neg;
  cfg->timings.flags.pclk_idle_high = 0;

  cfg->data_width = 16;  // RGB565 in parallel mode.
#ifdef LEGACY_RGBPANEL
  // Deprecated.
  cfg->sram_trans_align = 8;
  cfg->psram_trans_align = 64;
#endif
  cfg->hsync_gpio_num = config.hsync;
  cfg->vsync_gpio_num = config.vsync;
  cfg->de_gpio_num = config.de;
  cfg->pclk_gpio_num = config.pclk;

#ifndef LEGACY_RGBPANEL
  cfg->bounce_buffer_size_px = 10 * config.width;
#endif

  if (config.bswap) {
    cfg->data_gpio_nums[0] = config.g3;
    cfg->data_gpio_nums[1] = config.g4;
    cfg->data_gpio_nums[2] = config.g5;
    cfg->data_gpio_nums[3] = config.r0;
    cfg->data_gpio_nums[4] = config.r1;
    cfg->data_gpio_nums[5] = config.r2;
    cfg->data_gpio_nums[6] = config.r3;
    cfg->data_gpio_nums[7] = config.r4;
    cfg->data_gpio_nums[8] = config.b0;
    cfg->data_gpio_nums[9] = config.b1;
    cfg->data_gpio_nums[10] = config.b2;
    cfg->data_gpio_nums[11] = config.b3;
    cfg->data_gpio_nums[12] = config.b4;
    cfg->data_gpio_nums[13] = config.g0;
    cfg->data_gpio_nums[14] = config.g1;
    cfg->data_gpio_nums[15] = config.g2;
  } else {
    cfg->data_gpio_nums[0] = config.b0;
    cfg->data_gpio_nums[1] = config.b1;
    cfg->data_gpio_nums[2] = config.b2;
    cfg->data_gpio_nums[3] = config.b3;
    cfg->data_gpio_nums[4] = config.b4;
    cfg->data_gpio_nums[5] = config.g0;
    cfg->data_gpio_nums[6] = config.g1;
    cfg->data_gpio_nums[7] = config.g2;
    cfg->data_gpio_nums[8] = config.g3;
    cfg->data_gpio_nums[9] = config.g4;
    cfg->data_gpio_nums[10] = config.g5;
    cfg->data_gpio_nums[11] = config.r0;
    cfg->data_gpio_nums[12] = config.r1;
    cfg->data_gpio_nums[13] = config.r2;
    cfg->data_gpio_nums[14] = config.r3;
    cfg->data_gpio_nums[15] = config.r4;
  }

  cfg->disp_gpio_num = -1;

  cfg->flags.disp_active_low = 0;

#if defined(LEGACY_RGBPANEL)
  cfg->flags.relax_on_idle = 0;
#else
  cfg->num_fbs = 1;
#endif

  cfg->flags.fb_in_psram = 1;  // allocate frame buffer in PSRAM

  esp_lcd_panel_handle_t handle;
  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(cfg, &handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(handle));

  roo::byte *buf;
  esp_lcd_rgb_panel_get_frame_buffer(handle, 1, (void **)&buf);
  return buf;
}

template <>
void ParallelRgb565<FLUSH_MODE_AGGRESSIVE>::init() {
  roo::byte *buffer = AllocateBuffer(cfg_);
  buffer_.reset(new Dev(cfg_.width, cfg_.height, buffer,
                        ::roo_display::internal::Rgb565Dma()));
  buffer_->setOrientation(orientation());
}

template <>
void ParallelRgb565<FLUSH_MODE_AGGRESSIVE>::end() {}

template <>
void ParallelRgb565<FLUSH_MODE_AGGRESSIVE>::write(Color *color,
                                                  uint32_t pixel_count) {
  buffer_->write(color, pixel_count);
}

template <>
void ParallelRgb565<FLUSH_MODE_AGGRESSIVE>::writePixels(BlendingMode mode,
                                                        Color *color,
                                                        int16_t *x, int16_t *y,
                                                        uint16_t pixel_count) {
  buffer_->writePixels(mode, color, x, y, pixel_count);
}

template <>
void ParallelRgb565<FLUSH_MODE_AGGRESSIVE>::fillPixels(BlendingMode mode,
                                                       Color color, int16_t *x,
                                                       int16_t *y,
                                                       uint16_t pixel_count) {
  buffer_->fillPixels(mode, color, x, y, pixel_count);
}

template <>
void ParallelRgb565<FLUSH_MODE_AGGRESSIVE>::writeRects(BlendingMode mode,
                                                       Color *color,
                                                       int16_t *x0, int16_t *y0,
                                                       int16_t *x1, int16_t *y1,
                                                       uint16_t count) {
  buffer_->writeRects(mode, color, x0, y0, x1, y1, count);
}

template <>
void ParallelRgb565<FLUSH_MODE_AGGRESSIVE>::fillRects(BlendingMode mode,
                                                      Color color, int16_t *x0,
                                                      int16_t *y0, int16_t *x1,
                                                      int16_t *y1,
                                                      uint16_t count) {
  buffer_->fillRects(mode, color, x0, y0, x1, y1, count);
}

namespace {

struct FlushRange {
  uint32_t offset;
  uint32_t length;
};

inline FlushRange ResolveFlushRangeForRects(const Config &cfg,
                                            Orientation orientation,
                                            int16_t *x0, int16_t *y0,
                                            int16_t *x1, int16_t *y1,
                                            uint16_t count) {
  if (orientation.isXYswapped()) {
    y0 = x0;
    y1 = x1;
  }
  int16_t ymin = *y0++;
  int16_t ymax = *y1++;
  while (--count > 0) {
    ymin = std::min(ymin, *y0++);
    ymax = std::max(ymax, *y1++);
  }
  return FlushRange{
      .offset = orientation.isTopToBottom()
                    ? (uint32_t)(ymin * cfg.width * 2)
                    : (uint32_t)((cfg.height - ymax - 1) * cfg.width * 2),
      .length = (uint32_t)((ymax - ymin + 1) * cfg.width * 2),
  };
}

}  // namespace

template <>
void ParallelRgb565<FLUSH_MODE_BUFFERED>::init() {
  roo::byte *buffer = AllocateBuffer(cfg_);
  buffer_.reset(new Dev(cfg_.width, cfg_.height, buffer, Rgb565()));
  buffer_->setOrientation(orientation());
}

template <>
void ParallelRgb565<FLUSH_MODE_BUFFERED>::end() {}

template <>
void ParallelRgb565<FLUSH_MODE_BUFFERED>::write(Color *color,
                                                uint32_t pixel_count) {
  // int16_t x0 = buffer_->window_x();
  // int16_t y0 = buffer_->window_y();
  buffer_->write(color, pixel_count);
  // int16_t x1 = buffer_->window_x();
  // int16_t y1 = buffer_->window_y();
  // if (orientation().isXYswapped()) {
  //   y0 = x0;
  //   y1 = x1;
  // }
  // FlushRange range{.offset = orientation().isTopToBottom()
  //                                ? y0 * cfg_.width * 2
  //                                : (cfg_.height - y1 - 1) * cfg_.width * 2,
  //                  .length = (y1 - y0 + 1) * cfg_.width * 2};
  // Cache_WriteBack_Addr((uint32_t)buffer_->buffer() + range.offset,
  //                      range.length);
  flush();
  // Cache_WriteBack_Addr((uint32_t)(buffer_->buffer()),
  //                      cfg_.width * cfg_.height * 2);
}

template <>
void ParallelRgb565<FLUSH_MODE_BUFFERED>::writePixels(BlendingMode mode,
                                                      Color *color, int16_t *x,
                                                      int16_t *y,
                                                      uint16_t pixel_count) {
  // FlushRange range =
  //     ResolveFlushRangeForRects(cfg_, orientation(), x, y, x, y,
  //     pixel_count);
  buffer_->writePixels(mode, color, x, y, pixel_count);
  // Cache_WriteBack_Addr((uint32_t)buffer_->buffer() + range.offset,
  //                      range.length);
  flush();
  // Cache_WriteBack_Addr((uint32_t)(buffer_->buffer()),
  //                      cfg_.width * cfg_.height * 2);
}

template <>
void ParallelRgb565<FLUSH_MODE_BUFFERED>::fillPixels(BlendingMode mode,
                                                     Color color, int16_t *x,
                                                     int16_t *y,
                                                     uint16_t pixel_count) {
  // FlushRange range =
  //     ResolveFlushRangeForRects(cfg_, orientation(), x, y, x, y,
  //     pixel_count);
  buffer_->fillPixels(mode, color, x, y, pixel_count);
  // Cache_WriteBack_Addr((uint32_t)buffer_->buffer() + range.offset,
  //                      range.length);
  flush();
  // Cache_WriteBack_Addr((uint32_t)(buffer_->buffer()),
  //                      cfg_.width * cfg_.height * 2);
}

template <>
void ParallelRgb565<FLUSH_MODE_BUFFERED>::writeRects(BlendingMode mode,
                                                     Color *color, int16_t *x0,
                                                     int16_t *y0, int16_t *x1,
                                                     int16_t *y1,
                                                     uint16_t count) {
  // FlushRange range =
  //     ResolveFlushRangeForRects(cfg_, orientation(), x0, y0, x1, y1,
  //     count);
  buffer_->writeRects(mode, color, x0, y0, x1, y1, count);
  // Cache_WriteBack_Addr((uint32_t)buffer_->buffer() + range.offset,
  //                      range.length);
  flush();
  // Cache_WriteBack_Addr((uint32_t)(buffer_->buffer()),
  //                      cfg_.width * cfg_.height * 2);
}

template <>
void ParallelRgb565<FLUSH_MODE_BUFFERED>::fillRects(BlendingMode mode,
                                                    Color color, int16_t *x0,
                                                    int16_t *y0, int16_t *x1,
                                                    int16_t *y1,
                                                    uint16_t count) {
  // FlushRange range =
  //     ResolveFlushRangeForRects(cfg_, orientation(), x0, y0, x1, y1,
  //     count);
  buffer_->fillRects(mode, color, x0, y0, x1, y1, count);
  // Cache_WriteBack_Addr((uint32_t)buffer_->buffer() + range.offset,
  //                      range.length);
  flush();
  // Cache_WriteBack_Addr((uint32_t)(buffer_->buffer()),
  //                      cfg_.width * cfg_.height * 2);
}

template <>
void ParallelRgb565<FLUSH_MODE_LAZY>::init() {
  roo::byte *buffer = AllocateBuffer(cfg_);
  buffer_.reset(new Dev(cfg_.width, cfg_.height, buffer, Rgb565()));
  buffer_->setOrientation(orientation());
}

template <>
void ParallelRgb565<FLUSH_MODE_LAZY>::end() {
  if (buffer_ != nullptr) {
    flush();
  }
}

template <>
void ParallelRgb565<FLUSH_MODE_LAZY>::write(Color *color,
                                            uint32_t pixel_count) {
  buffer_->write(color, pixel_count);
}

template <>
void ParallelRgb565<FLUSH_MODE_LAZY>::writePixels(BlendingMode mode,
                                                  Color *color, int16_t *x,
                                                  int16_t *y,
                                                  uint16_t pixel_count) {
  buffer_->writePixels(mode, color, x, y, pixel_count);
}

template <>
void ParallelRgb565<FLUSH_MODE_LAZY>::fillPixels(BlendingMode mode, Color color,
                                                 int16_t *x, int16_t *y,
                                                 uint16_t pixel_count) {
  buffer_->fillPixels(mode, color, x, y, pixel_count);
}

template <>
void ParallelRgb565<FLUSH_MODE_LAZY>::writeRects(BlendingMode mode,
                                                 Color *color, int16_t *x0,
                                                 int16_t *y0, int16_t *x1,
                                                 int16_t *y1, uint16_t count) {
  buffer_->writeRects(mode, color, x0, y0, x1, y1, count);
}

template <>
void ParallelRgb565<FLUSH_MODE_LAZY>::fillRects(BlendingMode mode, Color color,
                                                int16_t *x0, int16_t *y0,
                                                int16_t *x1, int16_t *y1,
                                                uint16_t count) {
  buffer_->fillRects(mode, color, x0, y0, x1, y1, count);
}

// #if FLUSH_MODE == FLUSH_MODE_HARDCODED

// void ParallelRgb565::setAddress(uint16_t x0, uint16_t y0, uint16_t x1,
//                                 uint16_t y1, BlendingMode mode) {
//   window_.setAddress(x0, y0, x1, y1, config.width, config.height,
//   orientation());
// }

// void ParallelRgb565::write(Color *color, uint32_t pixel_count) {
//   int16_t xmin = buffer_->window_x();
//   int16_t ymin = buffer_->window_y();
//   uint16_t *buf = (uint16_t *)buffer_->buffer();
//   while (pixel_count-- > 0) {
//     buf[window_.offset()] = Rgb565().fromArgbColor(*color++);
//     window_.advance();
//   }
//   int16_t xmax = buffer_->window_x();
//   int16_t ymax = buffer_->window_y();

//   flush(&xmin, &ymin, &xmax, &ymax, 1);
// }

// void ParallelRgb565::writePixels(BlendingMode mode, Color *color, int16_t
// *x,
//                                  int16_t *y, uint16_t pixel_count) {
//   uint16_t *buf = (uint16_t *)buffer_->buffer();
//   int16_t w = config.width;
//   while (pixel_count-- > 0) {
//     uint16_t *addr = &buf[*y++ * w + *x++];
//     *addr = Rgb565().fromArgbColor(*color++);
//   }
//   // Empirically faster than flushing every pixel.
//   flush(x, y, x, y, 1);
// }

// void ParallelRgb565::fillPixels(BlendingMode mode, Color color, int16_t *x,
//                                 int16_t *y, uint16_t pixel_count) {
//   Rgb565 cmode;
//   uint16_t raw_color = cmode.fromArgbColor(color);
//   uint16_t *buf = (uint16_t *)buffer_->buffer();
//   int16_t w = config.width;
//   while (pixel_count-- > 0) {
//     uint16_t *addr = &buf[*y++ * w + *x++];
//     *addr = raw_color;
//   }
//   // Empirically faster than flushing every pixel.
//   flush(x, y, x, y, 1);
// }

// void ParallelRgb565::writeRects(BlendingMode mode, Color *color, int16_t
// *x0,
//                                 int16_t *y0, int16_t *x1, int16_t *y1,
//                                 uint16_t count) {
//   Rgb565 cmode;
//   int16_t* x0_orig = x0;
//   int16_t* y0_orig = y0;
//   int16_t* x1_orig = x1;
//   int16_t* y1_orig = y1;
//   uint16_t count_orig = count;
//   uint16_t *buf = (uint16_t *)buffer_->buffer();
//   int16_t width = config.width;
//   while (count-- > 0) {
//     uint16_t raw_color = cmode.fromArgbColor(*color++);
//     int16_t my_w = (*x1 - *x0 + 1);
//     int16_t my_h = (*y1 - *y0 + 1);
//     uint32_t line_offset = *y0 * width;
//     uint32_t flush_pixels = my_h * width;
//     if (my_w == width) {
//       pattern_fill<2>((uint8_t *)&buf[line_offset], my_w * my_h,
//                       (const uint8_t *)&raw_color);
//     } else {
//       uint16_t *out = &buf[*x0 + line_offset];
//       while (my_h-- > 0) {
//         pattern_fill<2>((uint8_t *)out, my_w, (const uint8_t *)&raw_color);
//         out += width;
//       }
//     }
//     // Cache_WriteBack_Addr((uint32_t)&buf[line_offset], flush_pixels * 2);
//     x0++;
//     y0++;
//     x1++;
//     y1++;
//   }
//   // Empirically faster than if we do it for every rect.
//   flush(x0_orig, y0_orig, x1_orig, y1_orig, count_orig);
// }

// void ParallelRgb565::fillRects(BlendingMode mode, Color color, int16_t *x0,
//                                int16_t *y0, int16_t *x1, int16_t *y1,
//                                uint16_t count) {
//   Rgb565 cmode;
//   int16_t* x0_orig = x0;
//   int16_t* y0_orig = y0;
//   int16_t* x1_orig = x1;
//   int16_t* y1_orig = y1;
//   uint16_t count_orig = count;
//   uint16_t raw_color = cmode.fromArgbColor(color);
//   uint16_t *buf = (uint16_t *)buffer_->buffer();
//   int16_t width = config.width;
//   while (count-- > 0) {
//     int16_t my_w = (*x1 - *x0 + 1);
//     int16_t my_h = (*y1 - *y0 + 1);
//     uint32_t line_offset = *y0 * width;
//     uint32_t flush_pixels = my_h * width;
//     if (my_w == width) {
//       pattern_fill<2>((uint8_t *)&buf[line_offset], my_w * my_h,
//                       (const uint8_t *)&raw_color);
//     } else {
//       uint16_t *out = &buf[*x0 + line_offset];
//       while (my_h-- > 0) {
//         pattern_fill<2>((uint8_t *)out, my_w, (const uint8_t *)&raw_color);
//         out += width;
//       }
//     }
//     // Cache_WriteBack_Addr((uint32_t)&buf[line_offset], flush_pixels * 2);
//     x0++;
//     y0++;
//     x1++;
//     y1++;
//   }
//   // Empirically faster than if we do it for every rect.
//   flush(x0_orig, y0_orig, x1_orig, y1_orig, count_orig);
// }

}  // namespace esp32s3_dma

}  // namespace roo_display

#endif  // CONFIG_IDF_TARGET_ESP32S3
#endif  // ESP_PLATFORM
