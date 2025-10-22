#pragma once

#include <Arduino.h>

#if defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)

#include <inttypes.h>

#include "esp_lcd_panel_io.h"
#include "roo_display/hal/esp32s3/gpio.h"
#include "roo_display/internal/byte_order.h"
#include "soc/lcd_cam_reg.h"
#include "soc/lcd_cam_struct.h"

#include "roo_io/data/byte_order.h"

namespace roo_display {
namespace esp32s3 {

class ParallelLcd8Bit {
 public:
  struct DataBus {
    int8_t pinD0;
    int8_t pinD1;
    int8_t pinD2;
    int8_t pinD3;
    int8_t pinD4;
    int8_t pinD5;
    int8_t pinD6;
    int8_t pinD7;
  };

  ParallelLcd8Bit(int8_t pinCs, int8_t pinDc, int pinRst, int8_t pinWr,
                  int8_t pinRd, DataBus data)
      : ParallelLcd8Bit(pinCs, pinDc, pinRst, pinWr, pinRd, std::move(data),
                        24000000) {}

  ParallelLcd8Bit(int8_t pinCs, int8_t pinDc, int pinRst, int8_t pinWr,
                  int8_t pinRd, DataBus data, int32_t speed)
      : pinCs_(pinCs),
        pinDc_(pinDc),
        pinRst_(pinRst),
        pinWr_(pinWr),
        pinRd_(pinRd),
        data_(std::move(data)),
        speed_(speed) {}

  void init() {
    if (pinCs_ >= 0) {
      pinMode(pinCs_, OUTPUT);
      digitalWrite(pinCs_, HIGH);
    }

    pinMode(pinDc_, OUTPUT);
    digitalWrite(pinDc_, HIGH);

    if (pinRst_ >= 0) {
      pinMode(pinRst_, OUTPUT);
      digitalWrite(pinRst_, HIGH);
    }

    pinMode(pinWr_, OUTPUT);
    digitalWrite(pinWr_, HIGH);

    if (pinRd_ >= 0) {
      pinMode(pinRd_, OUTPUT);
      digitalWrite(pinRd_, HIGH);
    }

    esp_lcd_i80_bus_handle_t i80_bus = nullptr;

    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = pinDc_,
        .wr_gpio_num = pinWr_,
        .clk_src = LCD_CLK_SRC_PLL160M,
        .data_gpio_nums = {data_.pinD0, data_.pinD1, data_.pinD2, data_.pinD3,
                           data_.pinD4, data_.pinD5, data_.pinD6, data_.pinD7},
        .bus_width = 8,
        .max_transfer_bytes = 2};
    esp_lcd_new_i80_bus(&bus_config, &i80_bus);

    uint32_t diff = INT32_MAX;
    uint32_t div_n = 256;
    uint32_t div_a = 63;
    uint32_t div_b = 62;
    uint32_t clkcnt = 64;
    uint32_t start_cnt = std::min<uint32_t>(64u, (F_CPU / (speed_ * 2) + 1));
    uint32_t end_cnt = std::max<uint32_t>(2u, F_CPU / 256u / speed_);
    if (start_cnt <= 2) {
      end_cnt = 1;
    }
    for (uint32_t cnt = start_cnt; diff && cnt >= end_cnt; --cnt) {
      float fdiv = (float)F_CPU / cnt / speed_;
      uint32_t n = std::max<uint32_t>(2u, (uint32_t)fdiv);
      fdiv -= n;

      for (uint32_t a = 63; diff && a > 0; --a) {
        uint32_t b = roundf(fdiv * a);
        if (a == b && n == 256) {
          break;
        }
        uint32_t freq = F_CPU / ((n * cnt) + (float)(b * cnt) / (float)a);
        uint32_t d = abs(speed_ - (int)freq);
        if (diff <= d) {
          continue;
        }
        diff = d;
        clkcnt = cnt;
        div_n = n;
        div_b = b;
        div_a = a;
        if (b == 0 || a == b) {
          break;
        }
      }
    }
    if (div_a == div_b) {
      div_b = 0;
      div_n += 1;
    }

    lcd_cam_lcd_clock_reg_t lcd_clock;
    lcd_clock.lcd_clkcnt_n = std::max(1ul, clkcnt - 1);
    lcd_clock.lcd_clk_equ_sysclk = (clkcnt == 1);
    lcd_clock.lcd_ck_idle_edge = true;
    lcd_clock.lcd_ck_out_edge = false;
    lcd_clock.lcd_clkm_div_num = div_n;
    lcd_clock.lcd_clkm_div_b = div_b;
    lcd_clock.lcd_clkm_div_a = div_a;
    lcd_clock.lcd_clk_sel =
        2;  // clock_select: 1=XTAL CLOCK / 2=240MHz / 3=160MHz
    lcd_clock.clk_en = true;

    LCD_CAM.lcd_clock.val = lcd_clock.val;
  }

  void beginTransaction() {}
  void beginWriteOnlyTransaction() {}
  void endTransaction() {}

  void begin() {
    digitalWrite(pinCs_, LOW);
    LCD_CAM.lcd_misc.val = LCD_CAM_LCD_CD_IDLE_EDGE;
    LCD_CAM.lcd_user.val = 0;
    LCD_CAM.lcd_user.val = LCD_CAM_LCD_CMD | LCD_CAM_LCD_UPDATE;
  }

  void sync() {}

  void end() {
    while (LCD_CAM.lcd_user.val & LCD_CAM_LCD_START) {
    }
    digitalWrite(pinCs_, HIGH);
  }

  void cmdBegin() {
    LCD_CAM.lcd_misc.val = LCD_CAM_LCD_CD_IDLE_EDGE | LCD_CAM_LCD_CD_CMD_SET;
  }

  void cmdEnd() { LCD_CAM.lcd_misc.val = LCD_CAM_LCD_CD_IDLE_EDGE; }

  void writeBytes(uint8_t* data, uint32_t len) {
    while (len-- > 0) write(*data++);
  }

  void writeBytes_async(uint8_t* data, uint32_t len) { writeBytes(data, len); }

  void write(uint8_t data) {
    LCD_CAM.lcd_cmd_val.lcd_cmd_value = data;
    while (LCD_CAM.lcd_user.val & LCD_CAM_LCD_START) {
    }
    LCD_CAM.lcd_user.val =
        LCD_CAM_LCD_CMD | LCD_CAM_LCD_UPDATE | LCD_CAM_LCD_START;
  }

  void write16(uint16_t data) {
    write(data >> 8);
    write(data & 0xFF);
  }

  void write16x2(uint16_t a, uint16_t b) {
    write(a >> 8);
    write(a & 0xFF);
    write(b >> 8);
    write(b & 0xFF);
  }

  void write16x2_async(uint16_t a, uint16_t b) { write16x2(a, b); }

  // Writes 2-byte word that has been pre-converted to BE if needed.
  void write16be(uint16_t data) { writeBytes((uint8_t*)&data, 2); }

  void write32(uint32_t data) {
    write((data >> 24) & 0xFF);
    write((data >> 16) & 0xFF);
    write((data >> 8) & 0xFF);
    write(data & 0xFF);
  }

  // Writes 4-byte word that has been pre-converted to BE if needed.
  void write32be(uint32_t data) { writeBytes((uint8_t*)&data, 4); }

  void fill16(uint16_t data, uint32_t len) {
    fill16be(roo_io::htobe(data), len);
  }

  void fill16be(uint16_t data, uint32_t len) {
    while (len-- > 0) {
      write16be(data);
    }
  }

  void fill16be_async(uint16_t data, uint32_t len) { fill16be(data, len); }

 private:
  int8_t pinCs_;
  int8_t pinDc_;
  int8_t pinRst_;
  int8_t pinWr_;
  int8_t pinRd_;
  DataBus data_;
  int32_t speed_;
};

}  // namespace esp32s3
}  // namespace roo_display

#endif  // defined(ESP32) && (CONFIG_IDF_TARGET_ESP32S3)