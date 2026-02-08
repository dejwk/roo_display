#pragma once

// If you want to use this adapter, include TFT_eSPI.h before including this
// file. You must have configured TFT_eSPI first, by editing the User_Setup.h as
// needed.
//
// Example main.cpp:
//
// #include "Arduino.h"
// #include "TFT_eSPI.h"
//
// using namespace roo_display;
//
// TFT_eSPI_Adapter device(Orientation().rotateLeft());
// Display display(device);
//
// // (The rest is identical to regular roo_display usage, except you do NOT
// // initialize the SPI directly).

#ifdef TFT_ESPI_VERSION

#include "roo_display/color/color.h"
#include "roo_display/core/device.h"
#include "roo_display/driver/common/addr_window_device.h"
#include "roo_display/internal/color_io.h"

namespace roo_display {

class TFT_eSPI_Adapter : public DisplayDevice {
 public:
  TFT_eSPI_Adapter(uint16_t width, uint16_t height)
      : TFT_eSPI_Adapter(Orientation(), width, height) {}

  TFT_eSPI_Adapter(Orientation orientation = Orientation())
      : TFT_eSPI_Adapter(orientation, TFT_WIDTH, TFT_HEIGHT) {}

  TFT_eSPI_Adapter(Orientation orientation, uint16_t width, uint16_t height)
      : DisplayDevice(orientation, width, height),
        tft_(width, height),
        bgcolor_(0xFF7F7F7F),
        compactor_() {}

  ~TFT_eSPI_Adapter() override {}

  void init() override {
    end();
    tft_.init();
    begin();
    tft_.setRotation(orientation().getRotationCount());
  }

  void begin() override { tft_.startWrite(); }

  void end() override { tft_.endWrite(); }

  void setBgColorHint(Color bgcolor) override { bgcolor_ = bgcolor; }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    blending_mode_ = mode;
    tft_.setWindow(x0, y0, x1, y1);
  }

  void write(Color* color, uint32_t pixel_count) override {
    uint16_t buffer[64];
    tft_.setSwapBytes(true);
    while (pixel_count > 64) {
      color = processColorSequence(blending_mode_, color, buffer, 64);
      tft_.pushPixels(buffer, 64);
      pixel_count -= 64;
    }
    processColorSequence(blending_mode_, color, buffer, pixel_count);
    tft_.pushPixels(buffer, pixel_count);
    tft_.setSwapBytes(false);
  }

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      uint32_t pixel_count = (*x1 - *x0 + 1) * (*y1 - *y0 + 1);
      TFT_eSPI_Adapter::setAddress(*x0++, *y0++, *x1++, *y1++,
                                   BLENDING_MODE_SOURCE);
      Color mycolor = *color++;
      if (mode == BLENDING_MODE_SOURCE_OVER) {
        mycolor = AlphaBlend(bgcolor_, mycolor);
      }
      uint16_t raw_color = to_raw_color(mycolor);
      tft_.pushBlock(raw_color, pixel_count);
    }
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    if (mode == BLENDING_MODE_SOURCE_OVER) {
      color = AlphaBlend(bgcolor_, color);
    }
    uint16_t raw_color = to_raw_color(color);

    while (count-- > 0) {
      uint32_t pixel_count = (*x1 - *x0 + 1) * (*y1 - *y0 + 1);
      TFT_eSPI_Adapter::setAddress(*x0++, *y0++, *x1++, *y1++,
                                   BLENDING_MODE_SOURCE);
      tft_.pushBlock(raw_color, pixel_count);
    }
  }

  void writePixels(BlendingMode mode, Color* colors, int16_t* xs, int16_t* ys,
                   uint16_t pixel_count) override {
    compactor_.drawPixels(
        xs, ys, pixel_count,
        [this, mode, colors](int16_t offset, int16_t x, int16_t y,
                             Compactor::WriteDirection direction,
                             int16_t count) {
          switch (direction) {
            case Compactor::RIGHT: {
              TFT_eSPI_Adapter::setAddress(x, y, x + count - 1, y, mode);
              break;
            }
            case Compactor::DOWN: {
              TFT_eSPI_Adapter::setAddress(x, y, x, y + count - 1, mode);
              break;
            }
            case Compactor::LEFT: {
              TFT_eSPI_Adapter::setAddress(x - count + 1, y, x, y, mode);
              std::reverse(colors + offset, colors + offset + count);
              break;
            }
            case Compactor::UP: {
              TFT_eSPI_Adapter::setAddress(x, y - count + 1, x, y, mode);
              std::reverse(colors + offset, colors + offset + count);
              break;
            }
          }
          TFT_eSPI_Adapter::write(colors + offset, count);
        });
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* xs, int16_t* ys,
                  uint16_t pixel_count) override {
    if (mode == BLENDING_MODE_SOURCE_OVER) {
      color = AlphaBlend(bgcolor_, color);
    }
    uint16_t raw_color = to_raw_color(color);
    compactor_.drawPixels(
        xs, ys, pixel_count,
        [this, raw_color](int16_t offset, int16_t x, int16_t y,
                          Compactor::WriteDirection direction, int16_t count) {
          switch (direction) {
            case Compactor::RIGHT: {
              TFT_eSPI_Adapter::setAddress(x, y, x + count - 1, y,
                                           BLENDING_MODE_SOURCE);
              break;
            }
            case Compactor::DOWN: {
              TFT_eSPI_Adapter::setAddress(x, y, x, y + count - 1,
                                           BLENDING_MODE_SOURCE);
              break;
            }
            case Compactor::LEFT: {
              TFT_eSPI_Adapter::setAddress(x - count + 1, y, x, y,
                                           BLENDING_MODE_SOURCE);
              break;
            }
            case Compactor::UP: {
              TFT_eSPI_Adapter::setAddress(x, y - count + 1, x, y,
                                           BLENDING_MODE_SOURCE);
              break;
            }
          }
          tft_.pushBlock(raw_color, count);
        });
  }

  void interpretRect(const roo::byte* data, size_t row_width_bytes,
                     int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                     Color* output) override {
    ColorRectIo<Rgb565, roo_io::kBigEndian> io;
    io.interpret(data, row_width_bytes, x0, y0, x1, y1, output);
  }

  void orientationUpdated() override {
    tft_.setRotation(orientation().getRotationCount());
  }

  static inline uint16_t to_raw_color(Color color)
      __attribute__((always_inline)) {
    return Rgb565().fromArgbColor(color);
  }

 protected:
  TFT_eSPI tft_;

 private:
  Color* processColorSequence(BlendingMode mode, Color* src, uint16_t* dest,
                              uint32_t pixel_count) {
    switch (mode) {
      case BLENDING_MODE_SOURCE: {
        while (pixel_count-- > 0) {
          *dest++ = to_raw_color(*src++);
        }
        break;
      }
      case BLENDING_MODE_SOURCE_OVER: {
        while (pixel_count-- > 0) {
          *dest++ = to_raw_color(AlphaBlend(bgcolor_, *src++));
        }
      }
      default:
        break;
    }
    return src;
  }

  Color bgcolor_;
  BlendingMode blending_mode_;
  Compactor compactor_;
};

}  // namespace roo_display

#endif