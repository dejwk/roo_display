#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "roo_backport/byte.h"
#include "roo_display/hal/esp32/spi_dma.h"
#include "roo_display/hal/esp32/spi_reg.h"
#include "roo_io/memory/fill.h"
#include "roo_logging.h"

#ifndef ROO_TESTING

namespace roo_display {
namespace esp32 {

inline __attribute__((always_inline)) void rotatePattern2(
    const roo::byte* pattern, size_t phase, roo::byte* rotated) {
  if ((phase & 1) == 0) {
    rotated[0] = pattern[0];
    rotated[1] = pattern[1];
  } else {
    rotated[0] = pattern[1];
    rotated[1] = pattern[0];
  }
}

inline __attribute__((always_inline)) void rotatePattern3(
    const roo::byte* pattern, size_t phase, roo::byte* rotated) {
  switch (phase % 3) {
    case 0:
      rotated[0] = pattern[0];
      rotated[1] = pattern[1];
      rotated[2] = pattern[2];
      break;
    case 1:
      rotated[0] = pattern[1];
      rotated[1] = pattern[2];
      rotated[2] = pattern[0];
      break;
    default:
      rotated[0] = pattern[2];
      rotated[1] = pattern[0];
      rotated[2] = pattern[1];
      break;
  }
}

inline __attribute__((always_inline)) size_t FillPattern2Bytes(
    roo::byte* dst, size_t len, const roo::byte* pattern, size_t phase) {
  if (len == 0) return phase;
  alignas(4) roo::byte rotated[2];
  rotatePattern2(pattern, phase, rotated);
  size_t full = len / 2;
  if (full > 0) {
    roo_io::PatternFill<2>(dst, full, rotated);
    dst += full * 2;
  }
  if ((len & 1) != 0) {
    *dst = rotated[0];
  }
  return (phase + len) & 1;
}

inline __attribute__((always_inline)) size_t FillPattern3Bytes(
    roo::byte* dst, size_t len, const roo::byte* pattern, size_t phase) {
  if (len == 0) return phase;
  alignas(4) roo::byte rotated[3];
  rotatePattern3(pattern, phase, rotated);
  size_t full = len / 3;
  if (full > 0) {
    roo_io::PatternFill<3>(dst, full, rotated);
    dst += full * 3;
  }
  size_t rem = len - full * 3;
  if (rem != 0) {
    __builtin_memcpy(dst, rotated, rem);
  }
  return (phase + len) % 3;
}

template <uint8_t spi_port>
class DmaPipeline {
 public:
  void init() { dma_controller_ = GetDmaControllerForHost(spi_port); }

  __attribute__((always_inline)) bool hasPendingAsync() const {
    return dma_work_buffer_used_ != 0 || dma_submitted_;
  }

  bool beginWriteOnlyTransaction() { return ensureDmaReady(); }

  void endTransaction() {
    if (dma_irq_bound_ && dma_controller_ != nullptr) {
      dma_controller_->unbindInterrupt();
      dma_irq_bound_ = false;
    }
  }

  void flush() {
    if (dma_work_buffer_used_ >= 512) {
      const size_t aligned = dma_work_buffer_used_ & ~static_cast<size_t>(3);
      const size_t remainder = dma_work_buffer_used_ - aligned;
      alignas(4) roo::byte tail[3];
      if (remainder > 0) {
        __builtin_memcpy(tail, dma_work_buffer_.data + aligned, remainder);
      }
      dma_work_buffer_used_ = aligned;
      submitWorkingDmaBuffer();
      dma_controller_->awaitCompleted();
      dma_submitted_ = false;
      if (remainder > 0) {
        syncWriteUpTo64BytesAlignedNoFlush(tail, remainder);
        SpiTxWait(spi_port);
      }
    } else if (dma_work_buffer_used_ > 0) {
      // Too small for opportunistic DMA publish in flush; transmit sync.
      const size_t len = dma_work_buffer_used_;
      dma_work_buffer_used_ = 0;
      if (dma_submitted_) {
        dma_controller_->awaitCompleted();
        dma_submitted_ = false;
      }
      SpiTxWait(spi_port);
      writeBytesSyncFlushed(dma_work_buffer_.data, len);
      SpiTxWait(spi_port);
    } else if (dma_submitted_) {
      dma_controller_->awaitCompleted();
      dma_submitted_ = false;
    }
  }

  void fill16once(const roo::byte* data, uint32_t repetitions) {
    uint16_t hi = static_cast<uint16_t>(data[1]);
    uint16_t lo = static_cast<uint16_t>(data[0]);
    uint16_t d16 = (hi << 8) | lo;
    uint32_t d32 = (d16 << 16) | d16;

    uint32_t len = repetitions * 2;

    // Immediately start the first chunk, to minimize latency.
    // Modulo 4 is used to ensure the rest is 4-byte aligned for DMA.
    // 21 is chosen empirically to tune the concurrency (DMA pipeline prepares
    // the next chunk while the first is transmitting) and minimize total time.
    size_t first = ((len - 1) % 4) + 21;
    SpiSetOutBufferSize(spi_port, first);
    SpiFillUpTo64(spi_port, d32, first);
    SpiTxStart(spi_port);
    len -= first;

    size_t reps = std::min<size_t>(len / 4, kDmaBufferCapacity / 4);
    if (dma_work_buffer_.data == nullptr) {
      dma_work_buffer_ = dma_controller_->acquireBuffer();
    }
    uint32_t* dest = (uint32_t*)dma_work_buffer_.data;
    while (reps > 8) {
      dest[0] = d32;
      dest[1] = d32;
      dest[2] = d32;
      dest[3] = d32;
      dest[4] = d32;
      dest[5] = d32;
      dest[6] = d32;
      dest[7] = d32;
      dest += 8;
      reps -= 8;
    }
    while (reps-- > 0) {
      *dest++ = d32;
    }

    bool ok = dma_controller_->submit(DmaController::Operation{
        .out_data = dma_work_buffer_.data,
        .out_size = kDmaBufferCapacity,
        .remaining = len,
    });
    CHECK(ok);
    dma_submitted_ = true;
    dma_work_buffer_ = {nullptr};
    dma_work_buffer_used_ = 0;
  }

  void appendBytes(const roo::byte* data, size_t len) {
    // CHECK_NOTNULL(dma_controller_);
    if (dma_work_buffer_used_ == 0 && !dma_submitted_ && !SpiTxBusy(spi_port)) {
      const bool src_aligned = (reinterpret_cast<uintptr_t>(data) & 0x3u) == 0;
      // If producer is slow enough and line goes idle, publish directly.
      if (len <= 64) {
        if (src_aligned) {
          syncWriteUpTo64BytesAlignedNoFlush(data, len);
        } else {
          writeBytesSyncFlushed(data, len);
        }
        return;
      }
      if (src_aligned) {
        syncWriteUpTo64BytesAlignedNoFlush(data, 64);
      } else {
        writeBytesSyncFlushed(data, 64);
      }
      data += 64;
      len -= 64;
    }

    const size_t capacity = dma_controller_->bufferCapacity();
    while (len != 0) {
      if (dma_work_buffer_.data == nullptr) {
        dma_work_buffer_ = dma_controller_->acquireBuffer();
        CHECK_NOTNULL(dma_work_buffer_.data);
        dma_work_buffer_used_ = 0;
      }
      size_t available = capacity - dma_work_buffer_used_;
      size_t chunk = (len < available) ? len : available;
      __builtin_memcpy(dma_work_buffer_.data + dma_work_buffer_used_, data,
                       chunk);
      dma_work_buffer_used_ += chunk;
      data += chunk;
      len -= chunk;
      if (dma_work_buffer_used_ == capacity) {
        submitWorkingDmaBuffer();
      } else {
        submitPartialIfLineIdle();
      }
    }
  }

  void appendRepeated16(const roo::byte* pattern, size_t repetitions) {
    CHECK_NOTNULL(dma_controller_);
    const size_t capacity = dma_controller_->bufferCapacity();
    size_t remaining_bytes = repetitions * 2;

    if (dma_work_buffer_.data == nullptr) {
      dma_work_buffer_ = dma_controller_->acquireBuffer();
      dma_work_buffer_used_ = 0;
    }

    size_t available = capacity - dma_work_buffer_used_;
    if (remaining_bytes <= available) {
      roo::byte* dst = dma_work_buffer_.data + dma_work_buffer_used_;
      roo_io::PatternFill<2>(dst, repetitions, pattern);
      dma_work_buffer_used_ += remaining_bytes;
      return;
    }

    size_t phase = 0;
    if (dma_work_buffer_.data != nullptr && dma_work_buffer_used_ > 0) {
      size_t available = capacity - dma_work_buffer_used_;
      phase = FillPattern2Bytes(dma_work_buffer_.data + dma_work_buffer_used_,
                                available, pattern, phase);
      dma_work_buffer_used_ = capacity;
      remaining_bytes -= available;
      submitWorkingDmaBuffer();
    }
    if (dma_work_buffer_.data == nullptr) {
      dma_work_buffer_ = dma_controller_->acquireBuffer();
    }

    const size_t chunk2 = capacity - (capacity % 4);
    if (chunk2 >= 64 && remaining_bytes >= 2 * chunk2) {
      phase = FillPattern2Bytes(dma_work_buffer_.data, chunk2, pattern, phase);
      dma_work_buffer_used_ = chunk2;
      size_t op_repetitions = remaining_bytes / chunk2;
      submitWorkingDmaBuffer(op_repetitions);
      remaining_bytes -= op_repetitions * chunk2;
    }

    if (remaining_bytes > 0) {
      if (dma_work_buffer_.data == nullptr) {
        dma_work_buffer_ = dma_controller_->acquireBuffer();
      }
      FillPattern2Bytes(dma_work_buffer_.data, remaining_bytes, pattern, phase);
      dma_work_buffer_used_ = remaining_bytes;
    }
  }

  void appendRepeated24(const roo::byte* pattern, size_t repetitions) {
    CHECK_NOTNULL(dma_controller_);
    const size_t capacity = dma_controller_->bufferCapacity();
    size_t remaining_bytes = repetitions * 3;

    if (dma_work_buffer_.data == nullptr) {
      dma_work_buffer_ = dma_controller_->acquireBuffer();
      dma_work_buffer_used_ = 0;
    }

    size_t available = capacity - dma_work_buffer_used_;
    if (remaining_bytes <= available) {
      roo::byte* dst = dma_work_buffer_.data + dma_work_buffer_used_;
      roo_io::PatternFill<3>(dst, repetitions, pattern);
      dma_work_buffer_used_ += remaining_bytes;
      return;
    }

    size_t phase = 0;
    if (dma_work_buffer_.data != nullptr && dma_work_buffer_used_ > 0) {
      size_t available = capacity - dma_work_buffer_used_;
      phase = FillPattern3Bytes(dma_work_buffer_.data + dma_work_buffer_used_,
                                available, pattern, phase);
      dma_work_buffer_used_ = capacity;
      remaining_bytes -= available;
      submitWorkingDmaBuffer();
    }
    if (dma_work_buffer_.data == nullptr) {
      dma_work_buffer_ = dma_controller_->acquireBuffer();
    }

    const size_t chunk3 = capacity - (capacity % 12);
    if (chunk3 >= 64 && remaining_bytes >= 2 * chunk3) {
      phase = FillPattern3Bytes(dma_work_buffer_.data, chunk3, pattern, phase);
      dma_work_buffer_used_ = chunk3;
      size_t op_repetitions = remaining_bytes / chunk3;
      submitWorkingDmaBuffer(op_repetitions);
      remaining_bytes -= op_repetitions * chunk3;
    }

    if (remaining_bytes > 0) {
      if (dma_work_buffer_.data == nullptr) {
        dma_work_buffer_ = dma_controller_->acquireBuffer();
      }
      FillPattern3Bytes(dma_work_buffer_.data, remaining_bytes, pattern, phase);
      dma_work_buffer_used_ = remaining_bytes;
    }
  }

  void submitPartialIfLineIdle() {
    CHECK_NOTNULL(dma_controller_);
    if (dma_work_buffer_.data == nullptr || dma_work_buffer_used_ == 0) return;

    bool line_idle = !dma_submitted_ && !SpiTxBusy(spi_port);
    if (!line_idle) return;

    if (dma_work_buffer_used_ < 64) {
      syncWriteUpTo64BytesAlignedNoFlush(dma_work_buffer_.data,
                                         dma_work_buffer_used_);
      dma_work_buffer_used_ = 0;
      return;
    }

    if (dma_work_buffer_used_ <= 512) {
      // Keep SPI hot when producer is too slow for efficient DMA batching.
      SpiSetOutBufferSize(spi_port, 64);
      SpiWrite64Aligned(spi_port, dma_work_buffer_.data);
      SpiTxStart(spi_port);
      dma_work_buffer_used_ -= 64;
      __builtin_memcpy(dma_work_buffer_.data, dma_work_buffer_.data + 64,
                       dma_work_buffer_used_);
      return;
    }

    const size_t aligned = dma_work_buffer_used_ & ~static_cast<size_t>(3);
    const size_t remainder = dma_work_buffer_used_ - aligned;
    alignas(4) roo::byte tail[3];
    if (remainder > 0) {
      __builtin_memcpy(tail, dma_work_buffer_.data + aligned, remainder);
    }

    dma_work_buffer_used_ = aligned;
    submitWorkingDmaBuffer();

    if (remainder > 0) {
      dma_work_buffer_ = dma_controller_->acquireBuffer();
      CHECK_NOTNULL(dma_work_buffer_.data);
      __builtin_memcpy(dma_work_buffer_.data, tail, remainder);
      dma_work_buffer_used_ = remainder;
    }
  }

 private:
  bool ensureDmaReady() {
    if (dma_controller_ == nullptr) return false;
    if (dma_irq_bound_) return true;
    if (!dma_controller_->bindInterrupt()) return false;
    dma_irq_bound_ = true;
    return true;
  }

  void submitWorkingDmaBuffer(size_t repetitions = 1) {
    bool ok = dma_controller_->submit(
        DmaController::Operation{dma_work_buffer_.data, dma_work_buffer_used_,
                                 repetitions * dma_work_buffer_used_});
    CHECK(ok);
    dma_submitted_ = true;
    dma_work_buffer_ = {nullptr};
    dma_work_buffer_used_ = 0;
  }

  __attribute__((always_inline)) void syncWriteUpTo64BytesAlignedNoFlush(
      const roo::byte* data, size_t len) {
    SpiSetOutBufferSize(spi_port, len);
    SpiWriteUpTo64Aligned(spi_port, data, len);
    SpiTxStart(spi_port);
  }

  void writeBytesSyncFlushed(const roo::byte* data, size_t len) {
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
  }

  DmaController* dma_controller_ = nullptr;
  bool dma_irq_bound_ = false;
  bool dma_submitted_ = false;
  DmaBufferPool::Buffer dma_work_buffer_ = {nullptr};
  size_t dma_work_buffer_used_ = 0;
};

}  // namespace esp32
}  // namespace roo_display

#endif  // ROO_TESTING
