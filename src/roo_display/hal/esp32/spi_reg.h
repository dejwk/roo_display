#pragma once

#include "roo_backport.h"
#include "roo_backport/byte.h"
#include "roo_display/hal/spi_settings.h"
#include "roo_io/data/byte_order.h"
#include "soc/spi_reg.h"

#pragma PUSH_MACRO("ASSERT_IF_DPORT_REG")

// Used by WRITE_PERI_REG and READ_PERI_REG, and causes trouble on ESP32 with
// IRAM_ATTR, leading to 'dangerous relocation: l32r: literal placed after use'
// errors.
#undef ASSERT_IF_DPORT_REG
#define ASSERT_IF_DPORT_REG(a, b)

#if CONFIG_IDF_TARGET_ESP32
#define ROO_DISPLAY_ESP32_SPI_DEFAULT_PORT 3
#else
#define ROO_DISPLAY_ESP32_SPI_DEFAULT_PORT 2
#ifndef SPI_MOSI_DLEN_REG
#define SPI_MOSI_DLEN_REG(x) SPI_MS_DLEN_REG(x)
#endif
#ifndef SPI_MISO_DLEN_REG
#define SPI_MISO_DLEN_REG(x) SPI_MS_DLEN_REG(x)
#endif
#endif

#if !defined(CONFIG_IDF_TARGET_ESP32) && !defined(CONFIG_IDF_TARGET_ESP32S2)
#define ROO_DISPLAY_SPI_CMD_UPDATE_REQUIRED 1
#else
#define ROO_DISPLAY_SPI_CMD_UPDATE_REQUIRED 0
#endif

#ifndef SPI_DMA_TX_ENA
#ifdef SPI_DMA_TX_EN
#define SPI_DMA_TX_ENA SPI_DMA_TX_EN
#endif
#endif

#ifndef SPI_TRANS_DONE_INT_ENA
#ifdef SPI_OUT_EOF_INT_ENA
#define SPI_TRANS_DONE_INT_ENA SPI_OUT_EOF_INT_ENA
#elif defined(SPI_OUT_DONE_INT_ENA)
#define SPI_TRANS_DONE_INT_ENA SPI_OUT_DONE_INT_ENA
#endif
#endif

#ifndef SPI_TRANS_DONE_INT_CLR
#ifdef SPI_OUT_EOF_INT_CLR
#define SPI_TRANS_DONE_INT_CLR SPI_OUT_EOF_INT_CLR
#elif defined(SPI_OUT_DONE_INT_CLR)
#define SPI_TRANS_DONE_INT_CLR SPI_OUT_DONE_INT_CLR
#endif
#endif

#ifndef SPI_TRANS_DONE_INT_ST
#ifdef SPI_OUT_EOF_INT_ST
#define SPI_TRANS_DONE_INT_ST SPI_OUT_EOF_INT_ST
#elif defined(SPI_OUT_DONE_INT_ST)
#define SPI_TRANS_DONE_INT_ST SPI_OUT_DONE_INT_ST
#endif
#endif

#ifndef ROO_TESTING

namespace roo_display {
namespace esp32 {

// Enters the read-write mode.
inline void SpiSetReadWriteMode(uint8_t spi_port)
    __attribute__((always_inline));

// Enters the write-only mode.
inline void SpiSetWriteOnlyMode(uint8_t spi_port)
    __attribute__((always_inline));

// Initiates a polling (non-ISR) SPI operation.
inline void SpiTxStart(uint8_t spi_port) __attribute__((always_inline));

// Waits for the current SPI operation, if any, to complete.
inline void SpiTxWait(uint8_t spi_port) __attribute__((always_inline));

// Returns true while SPI user transaction is in progress.
inline bool SpiTxBusy(uint8_t spi_port) __attribute__((always_inline));

// Enables SPI DMA TX.
inline void SpiDmaTxEnable(uint8_t spi_port) __attribute__((always_inline));

// Disables SPI DMA TX.
inline void SpiDmaTxDisable(uint8_t spi_port) __attribute__((always_inline));

// Returns true when SPI DMA TX path is currently enabled.
inline bool SpiDmaTxEnabled(uint8_t spi_port) __attribute__((always_inline));

// Returns true when SPI DMA TX engine is active.
inline bool SpiDmaTxActive(uint8_t spi_port) __attribute__((always_inline));

// Resets SPI DMA AHB master and FIFO state.
inline void SpiDmaFifoReset(uint8_t spi_port) __attribute__((always_inline));

// Returns true when the SPI DMA transfer-done interrupt is pending.
inline bool SpiTransferDoneIntPending(uint8_t spi_port)
    __attribute__((always_inline));

// Clears the SPI DMA transfer-done interrupt.
inline void SpiTransferDoneIntClear(uint8_t spi_port)
    __attribute__((always_inline));

// Enables the SPI DMA transfer-done interrupt.
inline void SpiTransferDoneIntEnable(uint8_t spi_port)
    __attribute__((always_inline));

// Disables the SPI DMA transfer-done interrupt.
inline void SpiTransferDoneIntDisable(uint8_t spi_port)
    __attribute__((always_inline));

// Sets the number of bytes in the output buffer for the next SPI operation.
inline void SpiSetOutBufferSize(uint8_t spi_port, int len)
    __attribute__((always_inline));

// Sets the number of bytes in the output and input buffers for the next SPI
// operation.
inline void SpiSetTxBufferSize(uint8_t spi_port, int len)
    __attribute__((always_inline));

// Writes 4 bytes to the output registers from the provided uint32 value
// (assumed byte-swapped to big-endian).
inline void SpiWrite4(uint8_t spi_port, uint32_t d32)
    __attribute__((always_inline));

// Writes 4 bytes to the output registers, at the specified d32 offset, from the
// provided uint32 value (assumed byte-swapped to big-endian).
inline void SpiWrite4AtOffset(uint8_t spi_port, uint32_t d32, int offset)
    __attribute__((always_inline));

// Reads 4 bytes from the output registers.
inline uint32_t SpiRead4(uint8_t spi_port) __attribute__((always_inline));

// Writes 64 bytes to the output registers from the 32-bit aligned input buffer.
inline void SpiWrite64Aligned(uint8_t spi_port, const roo::byte* data)
    __attribute__((always_inline));

// Writes up to 64 bytes to the output registers, from the 32-bit aligned
// buffer.
inline void SpiWriteUpTo64Aligned(uint8_t spi_port, const roo::byte* data,
                                  int len) __attribute__((always_inline));

// Fills 64 bytes of the output registers with the specified 32-bit value.
inline void SpiFill64(uint8_t spi_port, uint32_t d32)
    __attribute__((always_inline));

// Clears SPI output data registers W0..W15.
inline void SpiClearDataRegisters(uint8_t spi_port)
    __attribute__((always_inline));

// Fills output registers with 1-64 bytes, provided in the 32-bit aligned
// buffer.
inline void SpiFillUpTo64(uint8_t spi_port, uint32_t d32, int len)
    __attribute__((always_inline));

// Fills output registers with up to 60 bytes, repeating the specified 12-byte
// pattern.
inline void SpiFill60(uint8_t spi_port, uint32_t d0, uint32_t d1, uint32_t d2)
    __attribute__((always_inline));

// Fills output registers with 60 bytes, 5x repeating the specified 12-byte
// pattern.
inline void SpiFillUpTo60(uint8_t spi_port, uint32_t d0, uint32_t d1,
                          uint32_t d2, int len) __attribute__((always_inline));

// Implementation below.

inline void SpiTxWait(uint8_t spi_port) {
  while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR) {
  }
}

inline bool SpiTxBusy(uint8_t spi_port) {
  return (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_USR) != 0;
}

inline bool SpiTransferDoneIntPending(uint8_t spi_port) {
#if CONFIG_IDF_TARGET_ESP32
  // Classic ESP32 reports master transfer completion in SPI_SLAVE as
  // SPI_TRANS_DONE.
  return (READ_PERI_REG(SPI_SLAVE_REG(spi_port)) & SPI_TRANS_DONE) != 0;
#else
  return (READ_PERI_REG(SPI_DMA_INT_ST_REG(spi_port)) &
          SPI_TRANS_DONE_INT_ST) != 0;
#endif
}

inline void SpiTransferDoneIntClear(uint8_t spi_port) {
#if CONFIG_IDF_TARGET_ESP32
  CLEAR_PERI_REG_MASK(SPI_SLAVE_REG(spi_port), SPI_TRANS_DONE);
#else
  SET_PERI_REG_MASK(SPI_DMA_INT_CLR_REG(spi_port), SPI_TRANS_DONE_INT_CLR);
#endif
}

inline void SpiTransferDoneIntEnable(uint8_t spi_port) {
#if CONFIG_IDF_TARGET_ESP32
  SET_PERI_REG_MASK(SPI_SLAVE_REG(spi_port), (SPI_TRANS_DONE << 5));
#else
  SET_PERI_REG_MASK(SPI_DMA_INT_ENA_REG(spi_port), SPI_TRANS_DONE_INT_ENA);
#endif
}

inline void SpiTransferDoneIntDisable(uint8_t spi_port) {
#if CONFIG_IDF_TARGET_ESP32
  CLEAR_PERI_REG_MASK(SPI_SLAVE_REG(spi_port), (SPI_TRANS_DONE << 5));
#else
  CLEAR_PERI_REG_MASK(SPI_DMA_INT_ENA_REG(spi_port), SPI_TRANS_DONE_INT_ENA);
#endif
}

inline void SpiDmaTxEnable(uint8_t spi_port) {
  SET_PERI_REG_MASK(SPI_DMA_CONF_REG(spi_port), SPI_DMA_TX_ENA);
}

inline void SpiDmaTxDisable(uint8_t spi_port) {
  CLEAR_PERI_REG_MASK(SPI_DMA_CONF_REG(spi_port), SPI_DMA_TX_ENA);
}

inline bool SpiDmaTxEnabled(uint8_t spi_port) {
  return (READ_PERI_REG(SPI_DMA_CONF_REG(spi_port)) & SPI_DMA_TX_ENA) != 0;
}

inline void SpiTxStart(uint8_t spi_port) {
#if ROO_DISPLAY_SPI_CMD_UPDATE_REQUIRED
  SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_UPDATE);
  while (READ_PERI_REG(SPI_CMD_REG(spi_port)) & SPI_UPDATE) {
  }
#endif

  // SET_PERI_REG_MASK(SPI_CMD_REG(spi_port), SPI_USR);
  // The 'correct' way to set the SPI_USR would be to set just the single bit,
  // as in the commented-out code above. But the remaining bits of this register
  // are unused (marked 'reserved'; see
  // https://www.espressif.com/sites/default/files/documentation/esp32_technical_reference_manual_en.pdf
  // page 131). Unconditionally setting the entire register brings significant
  // performance improvements.
  WRITE_PERI_REG(SPI_CMD_REG(spi_port), SPI_USR);
}

inline void SpiSetOutBufferSize(uint8_t spi_port, int len) {
  WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), (len << 3) - 1);
}

inline void SpiSetTxBufferSize(uint8_t spi_port, int len) {
  WRITE_PERI_REG(SPI_MOSI_DLEN_REG(spi_port), (len << 3) - 1);
#if CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32
  WRITE_PERI_REG(SPI_MISO_DLEN_REG(spi_port), (len << 3) - 1);
#endif
}

inline void SpiSetReadWriteMode(uint8_t spi_port) {
  WRITE_PERI_REG(SPI_USER_REG(spi_port),
                 SPI_USR_MOSI | SPI_USR_MISO | SPI_DOUTDIN);
}

inline void SpiSetWriteOnlyMode(uint8_t spi_port) {
  WRITE_PERI_REG(SPI_USER_REG(spi_port), SPI_USR_MOSI);
}

inline void SpiWrite64Aligned(uint8_t spi_port, const roo::byte* data) {
  const uint32_t* d32 = reinterpret_cast<const uint32_t*>(data);
  WRITE_PERI_REG(SPI_W0_REG(spi_port), d32[0]);
  WRITE_PERI_REG(SPI_W1_REG(spi_port), d32[1]);
  WRITE_PERI_REG(SPI_W2_REG(spi_port), d32[2]);
  WRITE_PERI_REG(SPI_W3_REG(spi_port), d32[3]);
  WRITE_PERI_REG(SPI_W4_REG(spi_port), d32[4]);
  WRITE_PERI_REG(SPI_W5_REG(spi_port), d32[5]);
  WRITE_PERI_REG(SPI_W6_REG(spi_port), d32[6]);
  WRITE_PERI_REG(SPI_W7_REG(spi_port), d32[7]);
  WRITE_PERI_REG(SPI_W8_REG(spi_port), d32[8]);
  WRITE_PERI_REG(SPI_W9_REG(spi_port), d32[9]);
  WRITE_PERI_REG(SPI_W10_REG(spi_port), d32[10]);
  WRITE_PERI_REG(SPI_W11_REG(spi_port), d32[11]);
  WRITE_PERI_REG(SPI_W12_REG(spi_port), d32[12]);
  WRITE_PERI_REG(SPI_W13_REG(spi_port), d32[13]);
  WRITE_PERI_REG(SPI_W14_REG(spi_port), d32[14]);
  WRITE_PERI_REG(SPI_W15_REG(spi_port), d32[15]);
}

inline void SpiWrite4(uint8_t spi_port, uint32_t d32) {
  WRITE_PERI_REG(SPI_W0_REG(spi_port), d32);
}

inline void SpiWrite4AtOffset(uint8_t spi_port, uint32_t d32, int offset) {
  WRITE_PERI_REG(SPI_W0_REG(spi_port) + (offset << 2), d32);
}

inline uint32_t SpiRead4(uint8_t spi_port) {
  return READ_PERI_REG(SPI_W0_REG(spi_port));
}

inline void SpiWriteUpTo64Aligned(uint8_t spi_port, const roo::byte* data,
                                  int len) {
  const uint32_t* d32 = reinterpret_cast<const uint32_t*>(data);
  do {
    WRITE_PERI_REG(SPI_W0_REG(spi_port), d32[0]);
    if (len <= 4) break;
    WRITE_PERI_REG(SPI_W1_REG(spi_port), d32[1]);
    if (len <= 8) break;
    WRITE_PERI_REG(SPI_W2_REG(spi_port), d32[2]);
    if (len <= 12) break;
    WRITE_PERI_REG(SPI_W3_REG(spi_port), d32[3]);
    if (len <= 16) break;
    WRITE_PERI_REG(SPI_W4_REG(spi_port), d32[4]);
    if (len <= 20) break;
    WRITE_PERI_REG(SPI_W5_REG(spi_port), d32[5]);
    if (len <= 24) break;
    WRITE_PERI_REG(SPI_W6_REG(spi_port), d32[6]);
    if (len <= 28) break;
    WRITE_PERI_REG(SPI_W7_REG(spi_port), d32[7]);
    if (len <= 32) break;
    WRITE_PERI_REG(SPI_W8_REG(spi_port), d32[8]);
    if (len <= 36) break;
    WRITE_PERI_REG(SPI_W9_REG(spi_port), d32[9]);
    if (len <= 40) break;
    WRITE_PERI_REG(SPI_W10_REG(spi_port), d32[10]);
    if (len <= 44) break;
    WRITE_PERI_REG(SPI_W11_REG(spi_port), d32[11]);
    if (len <= 48) break;
    WRITE_PERI_REG(SPI_W12_REG(spi_port), d32[12]);
    if (len <= 52) break;
    WRITE_PERI_REG(SPI_W13_REG(spi_port), d32[13]);
    if (len <= 56) break;
    WRITE_PERI_REG(SPI_W14_REG(spi_port), d32[14]);
    if (len <= 60) break;
    WRITE_PERI_REG(SPI_W15_REG(spi_port), d32[15]);
  } while (false);
}

inline void SpiFill64(uint8_t spi_port, uint32_t d32) {
  WRITE_PERI_REG(SPI_W0_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W1_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W2_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W3_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W4_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W5_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W6_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W7_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W8_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W9_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W10_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W11_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W12_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W13_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W14_REG(spi_port), d32);
  WRITE_PERI_REG(SPI_W15_REG(spi_port), d32);
}

inline void SpiFillUpTo64(uint8_t spi_port, uint32_t d32, int len) {
  do {
    WRITE_PERI_REG(SPI_W0_REG(spi_port), d32);
    if (len <= 4) break;
    WRITE_PERI_REG(SPI_W1_REG(spi_port), d32);
    if (len <= 8) break;
    WRITE_PERI_REG(SPI_W2_REG(spi_port), d32);
    if (len <= 12) break;
    WRITE_PERI_REG(SPI_W3_REG(spi_port), d32);
    if (len <= 16) break;
    WRITE_PERI_REG(SPI_W4_REG(spi_port), d32);
    if (len <= 20) break;
    WRITE_PERI_REG(SPI_W5_REG(spi_port), d32);
    if (len <= 24) break;
    WRITE_PERI_REG(SPI_W6_REG(spi_port), d32);
    if (len <= 28) break;
    WRITE_PERI_REG(SPI_W7_REG(spi_port), d32);
    if (len <= 32) break;
    WRITE_PERI_REG(SPI_W8_REG(spi_port), d32);
    if (len <= 36) break;
    WRITE_PERI_REG(SPI_W9_REG(spi_port), d32);
    if (len <= 40) break;
    WRITE_PERI_REG(SPI_W10_REG(spi_port), d32);
    if (len <= 44) break;
    WRITE_PERI_REG(SPI_W11_REG(spi_port), d32);
    if (len <= 48) break;
    WRITE_PERI_REG(SPI_W12_REG(spi_port), d32);
    if (len <= 52) break;
    WRITE_PERI_REG(SPI_W13_REG(spi_port), d32);
    if (len <= 56) break;
    WRITE_PERI_REG(SPI_W14_REG(spi_port), d32);
    if (len <= 60) break;
    WRITE_PERI_REG(SPI_W15_REG(spi_port), d32);
  } while (false);
}

inline void SpiFill60(uint8_t spi_port, uint32_t d0, uint32_t d1, uint32_t d2) {
  WRITE_PERI_REG(SPI_W0_REG(spi_port), d0);
  WRITE_PERI_REG(SPI_W1_REG(spi_port), d1);
  WRITE_PERI_REG(SPI_W2_REG(spi_port), d2);
  WRITE_PERI_REG(SPI_W3_REG(spi_port), d0);
  WRITE_PERI_REG(SPI_W4_REG(spi_port), d1);
  WRITE_PERI_REG(SPI_W5_REG(spi_port), d2);
  WRITE_PERI_REG(SPI_W6_REG(spi_port), d0);
  WRITE_PERI_REG(SPI_W7_REG(spi_port), d1);
  WRITE_PERI_REG(SPI_W8_REG(spi_port), d2);
  WRITE_PERI_REG(SPI_W9_REG(spi_port), d0);
  WRITE_PERI_REG(SPI_W10_REG(spi_port), d1);
  WRITE_PERI_REG(SPI_W11_REG(spi_port), d2);
  WRITE_PERI_REG(SPI_W12_REG(spi_port), d0);
  WRITE_PERI_REG(SPI_W13_REG(spi_port), d1);
  WRITE_PERI_REG(SPI_W14_REG(spi_port), d2);
}

inline void SpiFillUpTo60(uint8_t spi_port, uint32_t d0, uint32_t d1,
                          uint32_t d2, int len) {
  do {
    WRITE_PERI_REG(SPI_W0_REG(spi_port), d0);
    if (len <= 4) break;
    WRITE_PERI_REG(SPI_W1_REG(spi_port), d1);
    if (len <= 8) break;
    WRITE_PERI_REG(SPI_W2_REG(spi_port), d2);
    if (len <= 12) break;
    WRITE_PERI_REG(SPI_W3_REG(spi_port), d0);
    if (len <= 16) break;
    WRITE_PERI_REG(SPI_W4_REG(spi_port), d1);
    if (len <= 20) break;
    WRITE_PERI_REG(SPI_W5_REG(spi_port), d2);
    if (len <= 24) break;
    WRITE_PERI_REG(SPI_W6_REG(spi_port), d0);
    if (len <= 28) break;
    WRITE_PERI_REG(SPI_W7_REG(spi_port), d1);
    if (len <= 32) break;
    WRITE_PERI_REG(SPI_W8_REG(spi_port), d2);
    if (len <= 36) break;
    WRITE_PERI_REG(SPI_W9_REG(spi_port), d0);
    if (len <= 40) break;
    WRITE_PERI_REG(SPI_W10_REG(spi_port), d1);
    if (len <= 44) break;
    WRITE_PERI_REG(SPI_W11_REG(spi_port), d2);
    if (len <= 48) break;
    WRITE_PERI_REG(SPI_W12_REG(spi_port), d0);
    if (len <= 52) break;
    WRITE_PERI_REG(SPI_W13_REG(spi_port), d1);
    if (len <= 56) break;
    WRITE_PERI_REG(SPI_W14_REG(spi_port), d2);
  } while (false);
}

}  // namespace esp32
}  // namespace roo_display

#pragma POP_MACRO("ASSERT_IF_DPORT_REG")

#endif  // ROO_TESTING
