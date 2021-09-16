#pragma once

// Utility methods for fast-filling memory with specified bit patterns. Used to
// fill large areas of offscreen buffers in various color modes. The methods
// optimize performance by reducing the frequency of branching and writing large
// data blocks when possible.

#include <inttypes.h>

#include <cstring>

namespace roo_display {

template <int bytes>
inline void pattern_write(uint8_t* buf, const uint8_t* val);

template <>
inline void pattern_write<1>(uint8_t* buf, const uint8_t* val) {
  *buf = *val;
}

template <>
inline void pattern_write<2>(uint8_t* buf, const uint8_t* val) {
  *((uint16_t*)buf) = *((const uint16_t*)val);
}

template <>
inline void pattern_write<3>(uint8_t* buf, const uint8_t* val) {
  buf[0] = val[0];
  buf[1] = val[1];
  buf[2] = val[2];
}

template <>
inline void pattern_write<4>(uint8_t* buf, const uint8_t* val) {
  *((uint32_t*)buf) = *((const uint32_t*)val);
}

template <int bytes>
inline void pattern_fill(uint8_t* buf, uint32_t count, const uint8_t* val);

template <>
inline void pattern_fill<1>(uint8_t* buf, uint32_t count, const uint8_t* val) {
  memset(buf, *val, count);
}

namespace internal {

inline void pattern_fill_32_aligned(uint32_t* buf, uint32_t count,
                                    uint32_t val) {
  while (count >= 8) {
    buf[0] = val;
    buf[1] = val;
    buf[2] = val;
    buf[3] = val;
    buf[4] = val;
    buf[5] = val;
    buf[6] = val;
    buf[7] = val;
    buf += 8;
    count -= 8;
  }
  if (count == 0) return;
  *buf++ = val;
  --count;
  if (count == 0) return;
  *buf++ = val;
  --count;
  if (count == 0) return;
  *buf++ = val;
  --count;
  if (count == 0) return;
  *buf++ = val;
  --count;
  if (count == 0) return;
  *buf++ = val;
  --count;
  if (count == 0) return;
  *buf++ = val;
  --count;
  if (count == 0) return;
  *buf = val;
  return;
}

inline void pattern_fill_16_aligned(uint16_t* buf, uint32_t count,
                                    uint16_t val) {
  if (((intptr_t)buf & 3) != 0) {
    *buf++ = val;
    --count;
  }
  uint32_t val32 = (uint32_t)val << 16 | val;
  pattern_fill_32_aligned((uint32_t*)buf, count / 2, val32);
  if ((count % 2) != 0) {
    buf[count - 1] = val;
  }
}

}  // namespace internal

template <>
inline void pattern_fill<2>(uint8_t* buf, uint32_t count, const uint8_t* val) {
  if (count < 8) {
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    --count;
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    --count;
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    --count;
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    --count;
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    --count;
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    --count;
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    --count;
    return;
  }
  if (val[0] == val[1]) {
    memset(buf, val[0], count * 2);
    return;
  }
  if ((((intptr_t)buf) & 1) != 0) {
    // Mis-aligned.
    *buf++ = val[0];
    uint16_t aligned;
    ((uint8_t*)&aligned)[0] = val[1];
    ((uint8_t*)&aligned)[1] = val[0];
    internal::pattern_fill_16_aligned((uint16_t*)buf, count - 1, aligned);
    buf[(count - 1) * 2] = val[1];
    return;
  } else {
    internal::pattern_fill_16_aligned((uint16_t*)buf, count, *(uint16_t*)val);
  }
}

template <>
inline void pattern_fill<3>(uint8_t* buf, uint32_t count, const uint8_t* val) {
  // Get to the point where we're aligned on 4 bytes.
  if (count < 8) {
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    *buf++ = val[2];
    --count;
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    *buf++ = val[2];
    --count;
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    *buf++ = val[2];
    --count;
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    *buf++ = val[2];
    --count;
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    *buf++ = val[2];
    --count;
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    *buf++ = val[2];
    --count;
    if (count == 0) return;
    *buf++ = val[0];
    *buf++ = val[1];
    *buf++ = val[2];
    --count;
    return;
  }

  if (((intptr_t)buf & 3) != 0) {
    *buf++ = val[0];
    *buf++ = val[1];
    *buf++ = val[2];
    --count;
    if (((intptr_t)buf & 3) != 0) {
      *buf++ = val[0];
      *buf++ = val[1];
      *buf++ = val[2];
      --count;
      if (((intptr_t)buf & 3) != 0) {
        *buf++ = val[0];
        *buf++ = val[1];
        *buf++ = val[2];
        --count;
      }
    }
  }

  uint8_t block[] = {val[0], val[1], val[2], val[0], val[1], val[2],
                     val[0], val[1], val[2], val[0], val[1], val[2]};
  const uint32_t* block32 = (const uint32_t*)block;
  uint32_t* buf32 = (uint32_t*)buf;
  while (count > 8) {
    *buf32++ = block32[0];
    *buf32++ = block32[1];
    *buf32++ = block32[2];
    *buf32++ = block32[0];
    *buf32++ = block32[1];
    *buf32++ = block32[2];
    count -= 8;
  }
  if (count > 4) {
    *buf32++ = block32[0];
    *buf32++ = block32[1];
    *buf32++ = block32[2];
    count -= 4;
  }
  buf = (uint8_t*)buf32;
  while (count-- > 0) {
    *buf++ = val[0];
    *buf++ = val[1];
    *buf++ = val[2];
  }
}

template <>
inline void pattern_fill<4>(uint8_t* buf, uint32_t count, const uint8_t* val) {
  if ((val[0] == val[1]) && (val[0] == val[2]) && (val[0] == val[3])) {
    // Common cases: black, white.
    memset(buf, val[0], count * 4);
    return;
  }
  if (count == 0) return;
  int offset = ((intptr_t)buf) & 3;
  if (offset != 0) {
    // Mis-aligned.
    memcpy(buf, val, 4 - offset);
    uint32_t aligned;
    ((uint8_t*)&aligned)[0] = val[4 - offset];
    ((uint8_t*)&aligned)[1] = val[(5 - offset) & 3];
    ((uint8_t*)&aligned)[2] = val[(6 - offset) & 3];
    ((uint8_t*)&aligned)[3] = val[(7 - offset) & 3];
    internal::pattern_fill_32_aligned((uint32_t*)(buf + 4 - offset), count - 1,
                                      aligned);
    memcpy(buf + count * 4 - offset, val + 4 - offset, offset);
  } else {
    internal::pattern_fill_32_aligned((uint32_t*)buf, count,
                                      *(const uint32_t*)val);
  }
}

// Fills 'count' consecutive bits of memory (in MSB order), starting at the
// given bit offset of the given buffer.
inline void bit_fill(uint8_t* buf, uint32_t offset, int16_t count, bool value) {
  buf += (offset / 8);
  offset %= 8;
  if (value) {
    if (offset > 0) {
      if (offset + count < 8) {
        *buf |= (((1 << count) - 1) << offset);
        return;
      }
      *buf++ |= (0xFF << offset);
      count -= (8 - offset);
      offset = 0;
    }
    memset(buf, 0xFF, count / 8);
    buf += (count / 8);
    count %= 8;
    if (count == 0) return;
    *buf |= ((1 << count) - 1);
  } else {
    if (offset > 0) {
      if (offset + count < 8) {
        *buf &= ~(((1 << count) - 1) << offset);
        return;
      }
      *buf++ &= ~(0xFF << offset);
      count -= (8 - offset);
      offset = 0;
    }
    memset(buf, 0x00, count / 8);
    buf += (count / 8);
    count %= 8;
    if (count == 0) return;
    *buf &= ~((1 << count) - 1);
  }
}

}  // namespace roo_display