#pragma once

#include <Arduino.h>
#include <SPI.h>

#include "roo_display/core/orientation.h"
#include "roo_display/core/raster.h"
#include "roo_display/driver/common/buffered_addr_window_device.h"
#include "roo_display/transport/spi.h"

namespace roo_display {
namespace ssd1327 {

enum Command { CASET = 0x15, RASET = 0x75 };

static const int16_t kWidth = 128;
static const int16_t kHeight = 128;

typedef SpiSettings<16000000, MSBFIRST, SPI_MODE0> DefaultSpiSettings;

template <typename Transport>
class Ssd1327Target {
 public:
  typedef Grayscale4 ColorMode;

  Ssd1327Target() : transport_(), xy_swap_(false) {}

  void init() {
    delay(200);
    begin();
    uint8_t init[] = {0xAE, 0x15, 0x00, 0x7F, 0x75, 0x00, 0x7F, 0x81, 0x53,
                      0xA0, 0x42, 0xA1, 0x00, 0xA2, 0x00, 0xA4, 0xA8, 0x7F,
                      0xB1, 0xF1, 0xB3, 0xC1, 0xAB, 0x01, 0xB6, 0x01, 0xBE,
                      0x07, 0xBC, 0x08, 0xD5, 0x62, 0xFD, 0x12};
    writeCommand(init, sizeof(init));
    end();
    delay(200);
    begin();
    writeCommand(0xAF);
    end();
  }

  int16_t width() const { return kWidth; }
  int16_t height() const { return kHeight; }

  void begin() {
    transport_.beginWriteOnlyTransaction();
    transport_.begin();
  }

  void end() {
    transport_.end();
    transport_.endTransaction();
  }

  void flushRect(ConstDramRaster<Grayscale4>& buffer, int16_t x0, int16_t y0,
                 int16_t x1, int16_t y1) {
    if (xy_swap_) {
      y0 &= ~1;
      y1 |= 1;
      setYaddr(x0, x1);
      setXaddr(y0, y1);
      const uint8_t* ptr = buffer.buffer() + (x0 + y0 * kWidth) / 2;
      uint32_t offset;
      for (int16_t x = (x0 & ~1); x <= (x1 | 1);) {
        if (x++ >= x0) {
          offset = 0;
          for (int16_t y = y0; y <= y1; y += 2) {
            uint8_t datum = (ptr[offset] & 0xF0) + (ptr[offset + 64] >> 4);
            transport_.write(datum);
            offset += 128;
          }
        }
        if (x++ <= x1) {
          offset = 0;
          for (int16_t y = y0; y <= y1; y += 2) {
            uint8_t datum = (ptr[offset] << 4) + (ptr[offset + 64] & 0x0F);
            transport_.write(datum);
            offset += 128;
          }
        }
        ptr++;
      }
    } else {
      x0 &= ~1;
      x1 |= 1;
      setXaddr(x0, x1);
      setYaddr(y0, y1);
      const uint8_t* ptr = buffer.buffer() + (x0 + y0 * kWidth) / 2;
      for (int16_t y = y0; y <= y1; ++y) {
        uint32_t offset = 0;
        for (int16_t x = x0; x <= x1; x += 2) {
          uint8_t datum = ptr[offset];
          // TODO(dawidk): block-write is possible here, and perhaps it would be
          // faster. One gotcha is that SPI.writeBytes() doesn't guarantee the
          // source data won't be mutated, so technically we'd need to use an
          // extra buffer, perhaps with zero-buffer specializations for
          // platforms that don't mutate.
          transport_.write(datum);
          offset++;
        }
        ptr += 64;
      }
    }
  }

  void setOrientation(Orientation orientation) {
    xy_swap_ = orientation.isXYswapped();
    uint8_t remap[] = {0xA0, 0x40 |
                                 (orientation.isLeftToRight() ? 0x02 : 0x01) |
                                 (orientation.isTopToBottom() ? 0x00 : 0x10)};
    writeCommand(remap, 2);
  }

 private:
  void setXaddr(uint16_t x0, uint16_t x1) __attribute__((always_inline)) {
    uint8_t caset[] = {CASET, x0 / 2, x1 / 2};
    writeCommand(caset, 3);
  }

  void setYaddr(uint16_t y0, uint16_t y1) __attribute__((always_inline)) {
    uint8_t raset[] = {RASET, y0, y1};
    writeCommand(raset, 3);
  }

  void writeCommand(uint8_t c) __attribute__((always_inline)) {
    transport_.cmdBegin();
    transport_.write(c);
    transport_.cmdEnd();
  }

  void writeCommand(uint8_t* c, uint32_t size) __attribute__((always_inline)) {
    transport_.cmdBegin();
    transport_.writeBytes_async(c, size);
    transport_.sync();
    transport_.cmdEnd();
  }

  Transport transport_;
  uint8_t write_buffer_[64];
  bool xy_swap_;
};

}  // namespace ssd1327

template <typename Transport>
using Ssd1327 = BufferedAddrWindowDevice<ssd1327::Ssd1327Target<Transport>>;

template <int pinCS, int pinDC, int pinRST, typename Spi = DefaultSpi,
          typename SpiSettings = ssd1327::DefaultSpiSettings>
using Ssd1327spi =
    Ssd1327<SpiTransport<pinCS, pinDC, pinRST, SpiSettings, Spi, DefaultGpio>>;

}  // namespace roo_display
