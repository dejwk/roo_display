#pragma once

#if defined(ARDUINO)
#include <Arduino.h>
#include <SPI.h>
#else
#include "driver/spi_master.h"
#include "esp_err.h"
#endif

#include <cstring>

#include "esp_private/spi_common_internal.h"
#if defined(ESP_PLATFORM)
#include "esp_memory_utils.h"
#endif
#include "roo_backport.h"
#include "roo_backport/byte.h"
#include "roo_display/hal/esp32/spi_reg.h"
#include "roo_display/hal/spi_settings.h"
#include "roo_io/data/byte_order.h"
#include "roo_logging.h"
#include "soc/spi_reg.h"

#ifndef ROO_TESTING

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

class AsyncOperation {
 public:
  AsyncOperation(int spi_port)
      : spi_port_(spi_port),
        done_(true),
        stop_(false),
        mux_(portMUX_INITIALIZER_UNLOCKED),
        waiter_task_(nullptr) {}

  __attribute__((always_inline)) void initFill(size_t len) {
    done_ = false;
    type_ = kFill;
    fill_.remaining = len;
    stop_ = false;
  }

  __attribute__((always_inline)) void initBlit(const roo::byte* data,
                                               size_t row_stride_bytes,
                                               size_t row_bytes,
                                               size_t row_count) {
    done_ = false;
    type_ = kBlit;
    blit_.next_row = data;
    blit_.row_stride_bytes = row_stride_bytes;
    blit_.row_bytes = row_bytes;
    blit_.remaining_rows = row_count;
    blit_.row_offset = 0;
    blit_.chunk_size = 0;
    prepareNextBlitChunk();
    stop_ = false;
  }

  __attribute__((always_inline)) IRAM_ATTR void handleInterrupt() {
    if (!SpiDmaTransferDoneIntPending(spi_port_)) {
      // Shared IRQ line: ignore unrelated interrupt sources.
      return;
    }
    SpiDmaTransferDoneIntClear(spi_port_);
    if (checkCompleteISR()) {
      return;
    }
    // state->remaining -= 64;
    // if (state->remaining >= 64) {
    SpiTxStart(spi_port_);
    // } else {
    //   SpiSetOutBufferSize(state->spi_port, state->remaining);
    //   SpiTxStart(state->spi_port);
    // }
  }

  void awaitCompletion() {
    TaskHandle_t me = xTaskGetCurrentTaskHandle();
    while (true) {
      portENTER_CRITICAL(&mux_);
      stop_ = true;
      if (done_) {
        waiter_task_ = nullptr;
        portEXIT_CRITICAL(&mux_);
        finishRemaining();
        return;
      }
      CHECK(waiter_task_ == nullptr || waiter_task_ == me);
      waiter_task_ = me;
      portEXIT_CRITICAL(&mux_);
      ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
  }

 private:
  enum Type { kFill, kBlit };

  struct Fill {
    size_t remaining;
  };

  struct Blit {
    const roo::byte* next_row;
    size_t row_stride_bytes;
    size_t row_bytes;
    size_t remaining_rows;
    size_t row_offset;
    size_t chunk_size;
    alignas(4) roo::byte chunk[64];
  };

  bool hasMoreBlitData() const {
    return blit_.remaining_rows > 0 && blit_.next_row != nullptr;
  }

  // Builds and loads up to 64 bytes from possibly strided source rows.
  bool prepareNextBlitChunk() {
    if (!hasMoreBlitData()) {
      blit_.chunk_size = 0;
      return false;
    }

    size_t out = 0;
    while (out < 64 && blit_.remaining_rows > 0) {
      size_t in_row = blit_.row_bytes - blit_.row_offset;
      size_t take = std::min<size_t>(64 - out, in_row);
      std::memcpy(&blit_.chunk[out], blit_.next_row + blit_.row_offset, take);
      out += take;
      blit_.row_offset += take;
      if (blit_.row_offset == blit_.row_bytes) {
        blit_.row_offset = 0;
        blit_.next_row += blit_.row_stride_bytes;
        --blit_.remaining_rows;
      }
    }

    blit_.chunk_size = out;
    if (out == 0) return false;
    SpiSetOutBufferSize(spi_port_, static_cast<int>(out));
    SpiWriteUpTo64Aligned(spi_port_, blit_.chunk, static_cast<int>(out));
    return true;
  }

  IRAM_ATTR bool markDoneAndNotifyWaiterISR() {
    SpiDmaTransferDoneIntDisable(spi_port_);
    done_ = true;
    TaskHandle_t waiter = waiter_task_;
    waiter_task_ = nullptr;
    portEXIT_CRITICAL_ISR(&mux_);
    if (waiter != nullptr) {
      BaseType_t high_wakeup = pdFALSE;
      vTaskNotifyGiveFromISR(waiter, &high_wakeup);
      portYIELD_FROM_ISR(high_wakeup);
    }
    return true;
  }

  IRAM_ATTR bool checkCompleteISR() {
    if (type_ == kFill) {
      portENTER_CRITICAL_ISR(&mux_);
      if (!stop_ && fill_.remaining > 64) {
        fill_.remaining -= 64;
        portEXIT_CRITICAL_ISR(&mux_);
        return false;
      }
      if (!stop_) fill_.remaining = 0;
      return markDoneAndNotifyWaiterISR();
    }

    if (type_ == kBlit) {
      portENTER_CRITICAL_ISR(&mux_);
      // Do not touch source row memory in ISR context because it may live in
      // PSRAM/cache-backed memory. Finish remaining chunks from task context.
      return markDoneAndNotifyWaiterISR();
    }
    return true;
  }

  void finishFill() {
    size_t count = fill_.remaining;
    while (count > 0) {
      SpiTxStart(spi_port_);
      count -= 64;
      SpiTxWait(spi_port_);
    }
  }

  void finishBlit() {
    while (prepareNextBlitChunk()) {
      SpiTxStart(spi_port_);
      SpiTxWait(spi_port_);
    }
  }

  void finishRemaining() {
    if (type_ == kFill) {
      finishFill();
    } else if (type_ == kBlit) {
      finishBlit();
    }
  }

  int spi_port_;
  Type type_;
  union {
    Fill fill_;
    Blit blit_;
  };
  bool done_;
  bool stop_;
  portMUX_TYPE mux_;
  TaskHandle_t waiter_task_;
};

void IRAM_ATTR TransferCompleteISR(void* arg) {
  AsyncOperation* op = static_cast<AsyncOperation*>(arg);
  op->handleInterrupt();
}

// Note: hardcoding the spi_port, making register accesses compile-time
// constants, does bring tangible performance improvements; ~2% in 'Lines' test
// in the Adafruit benchmark.
template <uint8_t spi_port, typename SpiSettings>
class Esp32SpiDevice {
 public:
  // friend void IRAM_ATTR TransferCompleteISR(void* arg);
  Esp32SpiDevice(Esp32Spi<spi_port>& spi)
      : spi_(spi.spi_), async_op_(spi_port) {}

#if defined(ARDUINO)
  void init() {
    esp_err_t intr_err =
        esp_intr_alloc(spicommon_irqsource_for_host(
                           static_cast<spi_host_device_t>(spi_port - 1)),
                       /* ESP_INTR_FLAG_SHARED | */ ESP_INTR_FLAG_IRAM,
                       TransferCompleteISR, &async_op_, &intr_handle_);
  }

  void beginReadWriteTransaction() {
    spi_.beginTransaction(SPISettings(
        SpiSettings::clock, SpiSettings::bit_order, SpiSettings::data_mode));
  }

  void beginWriteOnlyTransaction() {
    spi_.beginTransaction(SPISettings(
        SpiSettings::clock, SpiSettings::bit_order, SpiSettings::data_mode));
    SpiSetWriteOnlyMode(spi_port);
  }

  void endTransaction() {
    SpiSetReadWriteMode(spi_port);
    spi_.endTransaction();
  }
#else
  void init() {
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
  }

  void beginReadWriteTransaction() {
    spi_device_acquire_bus(device_, portMAX_DELAY);
  }

  void beginWriteOnlyTransaction() {
    spi_device_acquire_bus(device_, portMAX_DELAY);
    SpiSetWriteOnlyMode(spi_port);
  }

  void endTransaction() {
    SpiSetReadWriteMode(spi_port);
    spi_device_release_bus(device_);
  }
#endif

  void flush() __attribute__((always_inline)) {
    if (!need_sync_) return;
    need_sync_ = false;
    if (!async_op_pending_) {
      SpiTxWait(spi_port);
      return;
    }
    async_op_.awaitCompletion();
    async_op_pending_ = false;
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

  void writeBytes(const roo::byte* data, uint32_t len) {
    flush();
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
    // const uint32_t* d32 = reinterpret_cast<const uint32_t*>(data);
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

  void fill16(const roo::byte* data, uint32_t repetitions) {
    // Note: ESP32 is little-endian, so we're byte-swapping to
    // get the bytes sorted correctly in the output register.
    uint16_t hi = static_cast<uint16_t>(data[1]);
    uint16_t lo = static_cast<uint16_t>(data[0]);
    uint16_t d16 = (hi << 8) | lo;
    uint32_t d32 = (d16 << 16) | d16;
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

    async_op_pending_ = true;
    need_sync_ = true;
    async_op_.initFill(len);
    SpiDmaTransferDoneIntClear(spi_port);
    SpiDmaTransferDoneIntEnable(spi_port);
    SpiSetOutBufferSize(spi_port, 64);
    SpiTxStart(spi_port);
    // while (true) {
    //   SpiTxStart(spi_port);
    //   len -= 64;
    //   if (len == 0) {
    //     need_sync_ = true;
    //     return;
    //   }
    //   SpiTxWait(spi_port);
    // }
  }

  void fill24(const roo::byte* data, uint32_t repetitions) {
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

  void asyncBlit(const roo::byte* data, size_t row_stride_bytes,
                 size_t row_bytes, size_t row_count) {
    if (data == nullptr || row_bytes == 0 || row_count == 0) {
      return;
    }

    ++async_blit_stats_.requests;
    bool source_internal = isInternalBlitSource(data, row_stride_bytes,
                                                row_bytes, row_count);
    if (source_internal) {
      ++async_blit_stats_.internal_source;
    } else {
      ++async_blit_stats_.non_internal_source;
    }

    // Keep transfer synchronous until the conservative async path is proven
    // stable on-device. We still collect eligibility telemetry.
    bool async_eligible = source_internal && (row_stride_bytes == row_bytes);
    if (async_eligible) {
      ++async_blit_stats_.eligible_internal_contiguous;
    }

    ++async_blit_stats_.sync_fallbacks;
    if (!source_internal) {
      ++async_blit_stats_.fallback_non_internal;
    } else if (row_stride_bytes != row_bytes) {
      ++async_blit_stats_.fallback_non_contiguous;
    }

    if (row_stride_bytes == row_bytes) {
      writeBytes(data, static_cast<uint32_t>(row_bytes * row_count));
      flush();
      return;
    }

    const roo::byte* row = data;
    for (size_t i = 0; i < row_count; ++i) {
      writeBytes(row, static_cast<uint32_t>(row_bytes));
      row += row_stride_bytes;
    }
    flush();
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
  struct AsyncBlitStats {
    uint32_t requests = 0;
    uint32_t internal_source = 0;
    uint32_t non_internal_source = 0;
    uint32_t eligible_internal_contiguous = 0;
    uint32_t fallback_non_internal = 0;
    uint32_t fallback_non_contiguous = 0;
    uint32_t sync_fallbacks = 0;
  };

  bool isInternalBlitSource(const roo::byte* data, size_t row_stride_bytes,
                            size_t row_bytes, size_t row_count) const {
#if defined(ESP_PLATFORM)
    if (row_stride_bytes < row_bytes) return false;
    const roo::byte* row = data;
    for (size_t i = 0; i < row_count; ++i) {
      if (!esp_ptr_internal(row)) return false;
      if (!esp_ptr_internal(row + row_bytes - 1)) return false;
      row += row_stride_bytes;
    }
    return true;
#else
    (void)data;
    (void)row_stride_bytes;
    (void)row_bytes;
    (void)row_count;
    return false;
#endif
  }

#if defined(ARDUINO)
  decltype(SPI)& spi_;
#else
  spi_host_device_t spi_;
  spi_device_handle_t device_;
#endif
  AsyncBlitStats async_blit_stats_;
  bool need_sync_ = false;
  bool async_op_pending_ = false;
  AsyncOperation async_op_;
  intr_handle_t intr_handle_;
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
