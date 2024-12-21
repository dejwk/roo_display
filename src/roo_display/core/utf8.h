#pragma once

#include <assert.h>
#include <inttypes.h>

#include <cstring>
#include <ostream>

#include "roo_io/base/string_view.h"

namespace roo_display {

typedef uint16_t unicode_t;

// UTF8-encoded string reference.

// DEPRECATED: use roo_io::string_view directly. This alias may be removed in
// future versions of the library.
using StringView = roo_io::string_view;

// Writes the UTF-8 representation of the rune to buf. The `buf` must have
// sufficient size (4 is always safe). Returns the number of bytes actually
// written.
inline int EncodeRuneAsUtf8(uint32_t rune, uint8_t *buf) {
  if (rune <= 0x7F) {
    buf[0] = rune;
    return 1;
  }
  if (rune <= 0x7FF) {
    buf[1] = (rune & 0x3F) | 0x80;
    rune >>= 6;
    buf[0] = rune | 0xC0;
    return 2;
  }
  if (rune <= 0xFFFF) {
    buf[2] = (rune & 0x3F) | 0x80;
    rune >>= 6;
    buf[1] = (rune & 0x3F) | 0x80;
    rune >>= 6;
    buf[0] = rune | 0xE0;
    return 3;
  }
  buf[3] = (rune & 0x3F) | 0x80;
  rune >>= 6;
  buf[2] = (rune & 0x3F) | 0x80;
  rune >>= 6;
  buf[1] = (rune & 0x3F) | 0x80;
  rune >>= 6;
  buf[0] = rune | 0xF0;
  return 4;
}

class Utf8Decoder {
 public:
  Utf8Decoder(StringView v) : Utf8Decoder(v.data(), v.size()) {}

  Utf8Decoder(const uint8_t *data, uint32_t size)
      : Utf8Decoder((const char *)data, size) {}

  Utf8Decoder(const char *data, uint32_t size)
      : data_(data), remaining_(size) {}

  bool has_next() const { return remaining_ > 0; }
  const char *data() const { return data_; }
  uint32_t remaining() const { return remaining_; }

  unicode_t next() {
    --remaining_;
    uint8_t first = *data_++;
    // 7 bit Unicode
    if ((first & 0x80) == 0x00) {
      return first;
    }

    // 11 bit Unicode
    if (((first & 0xE0) == 0xC0) && (remaining_ >= 1)) {
      --remaining_;
      uint8_t second = *data_++;
      return ((first & 0x1F) << 6) | (second & 0x3F);
    }

    // 16 bit Unicode
    if (((first & 0xF0) == 0xE0) && (remaining_ >= 2)) {
      remaining_ -= 2;
      uint8_t second = *data_++;
      uint8_t third = *data_++;
      return ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | ((third & 0x3F));
    }

    // 21 bit Unicode not supported so fall-back to extended ASCII
    if ((first & 0xF8) == 0xF0) {
    }
    // fall-back to extended ASCII
    return first;
  }

 private:
  const char *data_;
  uint32_t remaining_;
};

}  // namespace roo_display