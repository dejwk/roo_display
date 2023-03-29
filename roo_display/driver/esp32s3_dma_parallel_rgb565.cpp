#include "roo_display/driver/esp32s3_dma_parallel_rgb565.h"

#if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)

#include "hal/dma_types.h"
#include "hal/lcd_hal.h"
#include "esp_intr_alloc.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_heap_caps.h"
#include "esp_pm.h"
#include "esp_private/gdma.h"

#include "rom/cache.h"

namespace roo_display {

namespace esp32s3_dma {

// extract from esp-idf esp_lcd_rgb_panel.c
struct esp_rgb_panel_t
{
  esp_lcd_panel_t base;                                        // Base class of generic lcd panel
  int panel_id;                                                // LCD panel ID
  lcd_hal_context_t hal;                                       // Hal layer object
  size_t data_width;                                           // Number of data lines (e.g. for RGB565, the data width is 16)
  size_t sram_trans_align;                                     // Alignment for framebuffer that allocated in SRAM
  size_t psram_trans_align;                                    // Alignment for framebuffer that allocated in PSRAM
  int disp_gpio_num;                                           // Display control GPIO, which is used to perform action like "disp_off"
  intr_handle_t intr;                                          // LCD peripheral interrupt handle
  esp_pm_lock_handle_t pm_lock;                                // Power management lock
  size_t num_dma_nodes;                                        // Number of DMA descriptors that used to carry the frame buffer
  uint8_t *fb;                                                 // Frame buffer
  size_t fb_size;                                              // Size of frame buffer
  int data_gpio_nums[SOC_LCD_RGB_DATA_WIDTH];                  // GPIOs used for data lines, we keep these GPIOs for action like "invert_color"
  size_t resolution_hz;                                        // Peripheral clock resolution
  esp_lcd_rgb_timing_t timings;                                // RGB timing parameters (e.g. pclk, sync pulse, porch width)
  gdma_channel_handle_t dma_chan;                              // DMA channel handle
  esp_lcd_rgb_panel_frame_trans_done_cb_t on_frame_trans_done; // Callback, invoked after frame trans done
  void *user_ctx;                                              // Reserved user's data of callback functions
  int x_gap;                                                   // Extra gap in x coordinate, it's used when calculate the flush window
  int y_gap;                                                   // Extra gap in y coordinate, it's used when calculate the flush window
  struct
  {
    unsigned int disp_en_level : 1; // The level which can turn on the screen by `disp_gpio_num`
    unsigned int stream_mode : 1;   // If set, the LCD transfers data continuously, otherwise, it stops refreshing the LCD when transaction done
    unsigned int fb_in_psram : 1;   // Whether the frame buffer is in PSRAM
  } flags;
  dma_descriptor_t dma_nodes[]; // DMA descriptor pool of size `num_dma_nodes`
};

void ParallelRgb565::init() {
  esp_lcd_rgb_panel_config_t* cfg =
      (esp_lcd_rgb_panel_config_t*)heap_caps_calloc(
          1, sizeof(esp_lcd_rgb_panel_config_t),
          MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);

  cfg->clk_src = LCD_CLK_SRC_PLL160M;

#ifdef CONFIG_SPIRAM_MODE_QUAD
    int32_t default_speed = 6000000L;
#else
    int32_t default_speed = 12000000L;
#endif

  cfg->timings.pclk_hz =
      (cfg_.prefer_speed == -1) ? default_speed : cfg_.prefer_speed;
  cfg->timings.h_res = cfg_.width;
  cfg->timings.v_res = cfg_.height;
  // The following parameters should refer to LCD spec.
  cfg->timings.hsync_pulse_width = cfg_.hsync_pulse_width;
  cfg->timings.hsync_back_porch = cfg_.hsync_back_porch;
  cfg->timings.hsync_front_porch = cfg_.hsync_front_porch;
  cfg->timings.vsync_pulse_width = cfg_.vsync_pulse_width;
  cfg->timings.vsync_back_porch = cfg_.vsync_back_porch;
  cfg->timings.vsync_front_porch = cfg_.vsync_front_porch;
  cfg->timings.flags.hsync_idle_low = (cfg_.hsync_polarity == 0) ? 1 : 0;
  cfg->timings.flags.vsync_idle_low = (cfg_.vsync_polarity == 0) ? 1 : 0;
  cfg->timings.flags.de_idle_high = 0;
  cfg->timings.flags.pclk_active_neg = cfg_.pclk_active_neg;
  cfg->timings.flags.pclk_idle_high = 0;

  cfg->data_width = 16;  // RGB565 in parallel mode.
  cfg->sram_trans_align = 8;
  cfg->psram_trans_align = 64;
  cfg->hsync_gpio_num = cfg_.hsync;
  cfg->vsync_gpio_num = cfg_.vsync;
  cfg->de_gpio_num = cfg_.de;
  cfg->pclk_gpio_num = cfg_.pclk;

  if (cfg_.bswap) {
    cfg->data_gpio_nums[0] = cfg_.g3;
    cfg->data_gpio_nums[1] = cfg_.g4;
    cfg->data_gpio_nums[2] = cfg_.g5;
    cfg->data_gpio_nums[3] = cfg_.r0;
    cfg->data_gpio_nums[4] = cfg_.r1;
    cfg->data_gpio_nums[5] = cfg_.r2;
    cfg->data_gpio_nums[6] = cfg_.r3;
    cfg->data_gpio_nums[7] = cfg_.r4;
    cfg->data_gpio_nums[8] = cfg_.b0;
    cfg->data_gpio_nums[9] = cfg_.b1;
    cfg->data_gpio_nums[10] = cfg_.b2;
    cfg->data_gpio_nums[11] = cfg_.b3;
    cfg->data_gpio_nums[12] = cfg_.b4;
    cfg->data_gpio_nums[13] = cfg_.g0;
    cfg->data_gpio_nums[14] = cfg_.g1;
    cfg->data_gpio_nums[15] = cfg_.g2;
  } else {
    cfg->data_gpio_nums[0] = cfg_.b0;
    cfg->data_gpio_nums[1] = cfg_.b1;
    cfg->data_gpio_nums[2] = cfg_.b2;
    cfg->data_gpio_nums[3] = cfg_.b3;
    cfg->data_gpio_nums[4] = cfg_.b4;
    cfg->data_gpio_nums[5] = cfg_.g0;
    cfg->data_gpio_nums[6] = cfg_.g1;
    cfg->data_gpio_nums[7] = cfg_.g2;
    cfg->data_gpio_nums[8] = cfg_.g3;
    cfg->data_gpio_nums[9] = cfg_.g4;
    cfg->data_gpio_nums[10] = cfg_.g5;
    cfg->data_gpio_nums[11] = cfg_.r0;
    cfg->data_gpio_nums[12] = cfg_.r1;
    cfg->data_gpio_nums[13] = cfg_.r2;
    cfg->data_gpio_nums[14] = cfg_.r3;
    cfg->data_gpio_nums[15] = cfg_.r4;
  }

  cfg->disp_gpio_num = -1;

  cfg->flags.disp_active_low = 0;
  cfg->flags.relax_on_idle = 0;
  cfg->flags.fb_in_psram = 1;  // allocate frame buffer in PSRAM

  esp_lcd_panel_handle_t handle;
  ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(cfg, &handle));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(handle));
  ESP_ERROR_CHECK(esp_lcd_panel_init(handle));

  esp_rgb_panel_t* rgb_panel = __containerof(handle, esp_rgb_panel_t, base);
  buffer_.reset(new Dev(cfg_.width, cfg_.height, (uint8_t*)rgb_panel->fb, Rgb565()));
}

void ParallelRgb565::end() { Cache_WriteBack_All(); }

}  // namespace esp32s3_dma

}  // namespace roo_display

#endif  // #if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)
