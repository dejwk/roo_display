#pragma once

#if defined(ARDUINO)
#include <Arduino.h>
#include <SPI.h>
#else
#include "driver/spi_master.h"
#include "esp_err.h"
#endif

#ifndef ROO_TESTING

#include <cstring>

#include "roo_backport.h"
#include "roo_backport/byte.h"
#include "roo_display/hal/esp32/memory.h"
#include "roo_display/hal/esp32/spi_config.h"
#include "roo_display/hal/esp32/spi_dma.h"
#include "roo_display/hal/esp32/spi_dma_pipeline.h"
#include "roo_display/hal/esp32/spi_non_dma_pipeline.h"
#include "roo_display/hal/esp32/spi_reg.h"
#include "roo_display/hal/spi_settings.h"
#include "roo_io/data/byte_order.h"
#include "roo_logging.h"
#include "soc/spi_reg.h"

namespace roo_display {
namespace esp32 {

template <uint8_t spi_port, typename SpiSettings>
class Esp32SpiDevice;

template <uint8_t spi_port>
class Esp32Spi {
 public:
  template <typename SpiSettings>
  using Device = Esp32SpiDevice<spi_port, SpiSettings>;

#if defined(ARDUINO)
  Esp32Spi() : spi_(SPI) {
    static_assert(
        spi_port == ROO_DISPLAY_ESP32_SPI_DEFAULT_PORT,
        "When using a SPI interface different than the default, you must "
        "provide a SPIClass object also in the constructor.");
  }

  Esp32Spi(decltype(SPI)& spi) : spi_(spi) {}

  void init() { spi_.begin(); }

  void init(uint8_t sck, uint8_t miso, uint8_t mosi) {
    spi_.begin(sck, miso, mosi);
  }

#else
  // The host enum is one-off from the spi_port.
  Esp32Spi() : spi_((spi_host_device_t)(spi_port - 1)) {}

  void init() {
    spi_bus_config_t config = {
        .mosi_io_num = -1,
        .miso_io_num = -1,
        .sclk_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(spi_, &config, SPI_DMA_CH_AUTO));
  }

  void init(uint8_t sck, uint8_t miso, uint8_t mosi) {
    spi_bus_config_t config = {
        .mosi_io_num = mosi,
        .miso_io_num = miso,
        .sclk_io_num = sck,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(spi_, &config, SPI_DMA_CH_AUTO));
  }

#endif

 private:
  template <uint8_t, typename SpiSettings>
  friend class Esp32SpiDevice;

#if defined(ARDUINO)
  decltype(SPI)& spi_;
#else
  spi_host_device_t spi_;
#endif
};

// Note: hardcoding the spi_port, making register accesses compile-time
// constants, does bring tangible performance improvements; ~2% in 'Lines' test
// in the Adafruit benchmark.
template <uint8_t spi_port, typename SpiSettings>
class Esp32SpiDevice {
 public:
  Esp32SpiDevice(Esp32Spi<spi_port>& spi) : spi_(spi.spi_) {}

  Esp32SpiDevice(const Esp32SpiDevice&) = delete;
  Esp32SpiDevice& operator=(const Esp32SpiDevice&) = delete;

#if defined(ARDUINO)
  Esp32SpiDevice(Esp32SpiDevice&& other) noexcept : spi_(other.spi_) {}
#else
  Esp32SpiDevice(Esp32SpiDevice&& other) noexcept
      : spi_(other.spi_),
        device_(other.device_) {}
#endif

  Esp32SpiDevice& operator=(Esp32SpiDevice&&) = delete;

  void init() {
    spi_async_mode_ = GetSpiAsyncModeFromFlag();
#if !defined(ARDUINO)
    spi_device_interface_config_t config_ = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = SpiSettings::data_mode,
        .clock_source = SPI_CLK_SRC_DEFAULT,
        .duty_cycle_pos = 128,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = SpiSettings::clock,
        .input_delay_ns = 0,
        .spics_io_num = -1,
        .flags = {SpiSettings::bit_order == kSpiLsbFirst
                      ? SPI_DEVICE_BIT_LSBFIRST
                      : 0},
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(spi_, &config_, &device_));
#endif
    if (spi_async_mode_ == kSpiAsyncModeDmaPipeline) {
      dma_pipeline_.init();
    }
  }

  void beginReadWriteTransaction() {
#if defined(ARDUINO)
    spi_.beginTransaction(SPISettings(
        SpiSettings::clock, SpiSettings::bit_order, SpiSettings::data_mode));
#else
    spi_device_acquire_bus(device_, portMAX_DELAY);
#endif
  }

  void beginWriteOnlyTransaction() {
#if defined(ARDUINO)
    spi_.beginTransaction(SPISettings(
        SpiSettings::clock, SpiSettings::bit_order, SpiSettings::data_mode));
#else
    spi_device_acquire_bus(device_, portMAX_DELAY);
#endif
    SpiSetWriteOnlyMode(spi_port);
    if (spi_async_mode_ == kSpiAsyncModeDmaPipeline) {
      CHECK(dma_pipeline_.beginWriteOnlyTransaction());
    } else if (spi_async_mode_ != kSpiAsyncModeSync) {
      CHECK(non_dma_pipeline_.beginWriteOnlyTransaction());
    }
  }

  void endTransaction() {
    flush();
    SpiSetReadWriteMode(spi_port);
    non_dma_pipeline_.endTransaction();
    dma_pipeline_.endTransaction();
#if defined(ARDUINO)
    spi_.endTransaction();
#else
    spi_device_release_bus(device_);
#endif
  }

  void flush() __attribute__((always_inline)) {
    if (!need_sync_) return;
    need_sync_ = false;
    if (!async_op_pending_) {
      SpiTxWait(spi_port);
      return;
    }
    async_op_pending_ = false;

    if (spi_async_mode_ == kSpiAsyncModeDmaPipeline) {
      dma_pipeline_.flush();
      return;
    }
    non_dma_pipeline_.flush(spi_async_mode_ != kSpiAsyncModeIsrStrict);
  }

  void write(uint8_t data) __attribute__((always_inline)) {
    flush();
    SpiSetOutBufferSize(spi_port, 1);
    SpiWrite4(spi_port, static_cast<uint32_t>(data));
    SpiTxStart(spi_port);
    need_sync_ = true;
  }

  void write16(uint16_t data) __attribute__((always_inline)) {
    flush();
    SpiSetOutBufferSize(spi_port, 2);
    SpiWrite4(spi_port, roo_io::htobe(data));
    SpiTxStart(spi_port);
    need_sync_ = true;
  }

  void write16x2(uint16_t a, uint16_t b) __attribute((always_inline)) {
    flush();
    SpiSetOutBufferSize(spi_port, 4);
    SpiWrite4(spi_port, roo_io::htobe(a) | (roo_io::htobe(b) << 16));
    SpiTxStart(spi_port);
    need_sync_ = true;
  }

  // We expect len > 0.
  void writeBytes(const roo::byte* data, uint32_t len) {
    if (spi_async_mode_ == kSpiAsyncModeDmaPipeline) {
      dma_pipeline_.appendBytes(data, len);
      need_sync_ = true;
      async_op_pending_ = dma_pipeline_.hasPendingAsync();
      return;
    }
    flush();
    writeBytesSyncFlushed(data, len);
  }

  // Like fill16, but must be called when flushed, and hints that it will be
  // followed by flush.
  void fill16once(const roo::byte* data, uint32_t repetitions) {
    uint32_t len = repetitions * 2;
    if (len <= 64) {
      // Note: ESP32 is little-endian, so we're byte-swapping to
      // get the bytes sorted correctly in the output register.
      uint16_t hi = static_cast<uint16_t>(data[1]);
      uint16_t lo = static_cast<uint16_t>(data[0]);
      uint16_t d16 = (hi << 8) | lo;
      uint32_t d32 = (d16 << 16) | d16;
      SpiSetOutBufferSize(spi_port, len);
      SpiFillUpTo64(spi_port, d32, len);
      SpiTxStart(spi_port);
      need_sync_ = true;
      return;
    }
    if (spi_async_mode_ == kSpiAsyncModeDmaPipeline && len >= 512) {
      dma_pipeline_.fill16once(data, repetitions);
      need_sync_ = true;
      async_op_pending_ = dma_pipeline_.hasPendingAsync();
      return;
    }
    // Note: ESP32 is little-endian, so we're byte-swapping to
    // get the bytes sorted correctly in the output register.
    uint16_t hi = static_cast<uint16_t>(data[1]);
    uint16_t lo = static_cast<uint16_t>(data[0]);
    uint16_t d16 = (hi << 8) | lo;
    uint32_t d32 = (d16 << 16) | d16;
    SpiFill64(spi_port, d32);
    uint32_t rem = len & 63;
    if (rem != 0) {
      SpiSetOutBufferSize(spi_port, rem);
      SpiTxStart(spi_port);
      len -= rem;
      SpiTxWait(spi_port);
    }
    SpiSetOutBufferSize(spi_port, 64);

    if (spi_async_mode_ != kSpiAsyncModeSync) {
      non_dma_pipeline_.beginAsyncFill(len);
      async_op_pending_ = true;
      need_sync_ = true;
      return;
    }

    // Sync fallback.
    while (true) {
      SpiTxStart(spi_port);
      len -= 64;
      if (len == 0) {
        need_sync_ = true;
        return;
      }
      SpiTxWait(spi_port);
    }
  }

  void fill16(const roo::byte* data, uint32_t repetitions) {
    // Note: ESP32 is little-endian, so we're byte-swapping to
    // get the bytes sorted correctly in the output register.
    uint16_t hi = static_cast<uint16_t>(data[1]);
    uint16_t lo = static_cast<uint16_t>(data[0]);
    uint16_t d16 = (hi << 8) | lo;
    uint32_t d32 = (d16 << 16) | d16;
    if (spi_async_mode_ == kSpiAsyncModeDmaPipeline) {
      dma_pipeline_.appendRepeated16(data, repetitions);
      dma_pipeline_.submitPartialIfLineIdle();
      need_sync_ = true;
      async_op_pending_ = dma_pipeline_.hasPendingAsync();
      return;
    }
    uint32_t len = repetitions * 2;
    if (len < 64) {
      flush();
      SpiSetOutBufferSize(spi_port, len);
      SpiFillUpTo64(spi_port, d32, len);
      SpiTxStart(spi_port);
      need_sync_ = true;
      return;
    }

    uint32_t rem = len & 63;
    flush();
    SpiFill64(spi_port, d32);
    if (rem != 0) {
      SpiSetOutBufferSize(spi_port, rem);
      SpiTxStart(spi_port);
      len -= rem;
      SpiTxWait(spi_port);
    }
    SpiSetOutBufferSize(spi_port, 64);

    if (spi_async_mode_ != kSpiAsyncModeSync) {
      non_dma_pipeline_.beginAsyncFill(len);
      async_op_pending_ = true;
      need_sync_ = true;
      return;
    }

    // Sync fallback.
    while (true) {
      SpiTxStart(spi_port);
      len -= 64;
      if (len == 0) {
        need_sync_ = true;
        return;
      }
      SpiTxWait(spi_port);
    }
  }

  void fill24(const roo::byte* data, uint32_t repetitions) {
    if (spi_async_mode_ == kSpiAsyncModeDmaPipeline) {
      dma_pipeline_.appendRepeated24(data, repetitions);
      dma_pipeline_.submitPartialIfLineIdle();
      need_sync_ = true;
      async_op_pending_ = dma_pipeline_.hasPendingAsync();
      return;
    }
    uint32_t r = static_cast<uint8_t>(data[0]);
    uint32_t g = static_cast<uint8_t>(data[1]);
    uint32_t b = static_cast<uint8_t>(data[2]);
    // Concatenate 4 pixels into three 32 bit blocks.
    uint32_t d2 = b | r << 8 | g << 16 | b << 24;
    uint32_t d1 = d2 << 8 | g;
    uint32_t d0 = d1 << 8 | r;
    uint32_t len = repetitions * 3;
    if (len < 60) {
      flush();
      SpiSetOutBufferSize(spi_port, len);
      SpiFillUpTo60(spi_port, d0, d1, d2, len);
      SpiTxStart(spi_port);
      need_sync_ = true;
      return;
    }

    uint32_t rem = len % 60;
    flush();
    SpiFill60(spi_port, d0, d1, d2);
    if (rem != 0) {
      SpiSetOutBufferSize(spi_port, rem);
      SpiTxStart(spi_port);
      len -= rem;
      SpiTxWait(spi_port);
    }
    SpiSetOutBufferSize(spi_port, 60);

    if (spi_async_mode_ != kSpiAsyncModeSync) {
      non_dma_pipeline_.beginAsyncFill(len, 60);
      async_op_pending_ = true;
      need_sync_ = true;
      return;
    }

    while (true) {
      SpiTxStart(spi_port);
      len -= 60;
      if (len == 0) {
        need_sync_ = true;
        return;
      }
      SpiTxWait(spi_port);
    }
  }

  // The source buffer must remain valid and unchanged until completion
  // (flush()/endTransaction()/next synchronous operation), same contract as
  // DisplayDevice::drawDirectRectAsync.
  void asyncBlit(const roo::byte* data, size_t row_stride_bytes,
                 size_t row_bytes, size_t row_count) {
    if (row_stride_bytes == row_bytes) {
      row_bytes = row_bytes * row_count;
      row_stride_bytes = row_bytes;
      row_count = 1;
    }

    if (spi_async_mode_ == kSpiAsyncModeDmaPipeline) {
      if (!need_sync_ && row_bytes * row_count <= 64) {
        syncBlitFallback(data, row_stride_bytes, row_bytes, row_count);
        return;
      }
      if (row_stride_bytes == row_bytes) {
        dma_pipeline_.appendBytes(data, row_bytes * row_count);
        dma_pipeline_.submitPartialIfLineIdle();
        need_sync_ = true;
        async_op_pending_ = dma_pipeline_.hasPendingAsync();
        return;
      } else {
        const roo::byte* row = data;
        for (size_t i = 0; i < row_count; ++i) {
          dma_pipeline_.appendBytes(row, row_bytes);
          row += row_stride_bytes;
        }
        dma_pipeline_.submitPartialIfLineIdle();
        need_sync_ = true;
        async_op_pending_ = dma_pipeline_.hasPendingAsync();
      }
      return;
    }

    if (spi_async_mode_ == kSpiAsyncModeSync) {
      syncBlitFallback(data, row_stride_bytes, row_bytes, row_count);
      return;
    }

    flush();
    if (!non_dma_pipeline_.tryBeginAsyncBlit(data, row_stride_bytes, row_bytes,
                                             row_count)) {
      syncBlitFallback(data, row_stride_bytes, row_bytes, row_count);
      return;
    }
    async_op_pending_ = true;
    need_sync_ = true;
    return;
  }

  roo::byte transfer(roo::byte data) __attribute__((always_inline)) {
    SpiSetTxBufferSize(spi_port, 1);
    SpiWrite4(spi_port, static_cast<uint32_t>(data));
    SpiTxStart(spi_port);
    SpiTxWait(spi_port);
    return static_cast<roo::byte>(SpiRead4(spi_port) & 0xFF);
  }

  uint16_t transfer16(uint16_t data) __attribute__((always_inline)) {
    // Apply byte-swapping for MSBFIRST (wr_bit_order = 0)
    data = roo_io::htobe(data);
    SpiSetTxBufferSize(spi_port, 2);
    SpiWrite4(spi_port, data);
    SpiTxStart(spi_port);
    SpiTxWait(spi_port);
    uint16_t result = SpiRead4(spi_port) & 0xFFFF;
    // Apply byte-swapping for MSBFIRST (rd_bit_order = 0)
    return roo_io::betoh(result);
  }

 private:
  void writeBytesSyncFlushed(const roo::byte* data, uint32_t len) {
    uintptr_t misalign = reinterpret_cast<uintptr_t>(data) & 0x3u;
    if (misalign != 0) {
      uint32_t prefix = 4 - static_cast<uint32_t>(misalign);
      if (prefix > len) prefix = len;
      uint32_t word = 0;
      __builtin_memcpy(&word, data, prefix);
      SpiSetOutBufferSize(spi_port, prefix);
      SpiWrite4(spi_port, word);
      SpiTxStart(spi_port);
      len -= prefix;
      if (len == 0) {
        need_sync_ = true;
        return;
      }
      data += prefix;
      SpiTxWait(spi_port);
    }
    if (len >= 64) {
      SpiSetOutBufferSize(spi_port, 64);
      while (true) {
        SpiWrite64Aligned(spi_port, data);
        SpiTxStart(spi_port);
        len -= 64;
        if (len == 0) {
          need_sync_ = true;
          return;
        }
        data += 64;
        SpiTxWait(spi_port);
        if (len < 64) break;
      }
    }
    SpiSetOutBufferSize(spi_port, len);
    SpiWriteUpTo64Aligned(spi_port, data, len);
    SpiTxStart(spi_port);
    need_sync_ = true;
  }

  void syncBlitFallback(const roo::byte* data, size_t row_stride_bytes,
                        size_t row_bytes, size_t row_count) {
    const roo::byte* row = data;
    for (size_t i = 0; i < row_count; ++i) {
      flush();
      writeBytesSyncFlushed(row, static_cast<uint32_t>(row_bytes));
      row += row_stride_bytes;
    }
  }

#if defined(ARDUINO)
  decltype(SPI)& spi_;
#else
  spi_host_device_t spi_;
  spi_device_handle_t device_;
#endif
  bool need_sync_ = false;
  bool async_op_pending_ = false;
  NonDmaPipeline<spi_port> non_dma_pipeline_;
  // Caching this here does improve performance of tight loops slightly (e.g.
  // filled triangles benchmark: ~6% impact without it).
  SpiAsyncMode spi_async_mode_ = kSpiAsyncModeSync;
  DmaPipeline<spi_port> dma_pipeline_;
};

// Original ESP32: SPI0 (none), FSPI -> SPI1, HSPI -> SPI2, VSPI -> SPI3.
// ESP32S2/S3/P4: FSPI -> SPI2, HSPI -> SPI3.
// Others: FSPI -> SPI2.
//
// Based on
// https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/esp32-hal-spi.h
// and
// https://github.com/espressif/arduino-esp32/blob/master/cores/esp32/esp32-hal-spi.c.
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3 || \
    CONFIG_IDF_TARGET_ESP32P4
using Fspi = Esp32Spi<2>;
using Hspi = Esp32Spi<3>;
#elif CONFIG_IDF_TARGET_ESP32
using Fspi = Esp32Spi<1>;
using Hspi = Esp32Spi<2>;
using Vspi = Esp32Spi<3>;
#else  // ESP32C2, C3, C5, C6, C61, H2
using Fspi = Esp32Spi<2>;
#endif

}  // namespace esp32
}  // namespace roo_display

#undef ROO_DISPLAY_SPI_ASYNC_ISR_ATTR

#else
// The SPI hardware is not emulated by roo_testing; we need to use a
// higher-level driver abstraction.

#include "roo_display/hal/arduino/spi.h"

namespace roo_display {
namespace esp32 {

#if CONFIG_IDF_TARGET_ESP32
using Vspi = ArduinoSpi;
using Hspi = ArduinoSpi;
#endif
using Fspi = ArduinoSpi;

}  // namespace esp32
}  // namespace roo_display

#endif
