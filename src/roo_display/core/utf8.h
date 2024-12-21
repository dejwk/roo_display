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

// Helper Utf8 iterator that allows to peek at up to 8 subsequent
// characters before consuming the next one. Useful for fonts supporting
// kerning and ligatures.
class Utf8LookAheadDecoder {
 public:
  Utf8LookAheadDecoder(StringView v)
      : Utf8LookAheadDecoder(v.data(), v.size()) {}

  Utf8LookAheadDecoder(const char *data, uint32_t size)
      : decoder_(data, size), buffer_offset_(0), buffer_size_(0) {
    // Fill in the lookahead buffer.
    while (decoder_.has_next() && lookahead_buffer_size() < 8) {
      push(decoder_.next());
    }
  }

  int lookahead_buffer_size() const { return buffer_size_; }

  bool has_next() const { return lookahead_buffer_size() > 0; }

  unicode_t peek_next(int index) const {
    assert(index < lookahead_buffer_size());
    return buffer_[(buffer_offset_ + index) % 8];
  }

  unicode_t next() {
    assert(has_next());
    unicode_t result = buffer_[buffer_offset_];
    ++buffer_offset_;
    buffer_offset_ %= 8;
    --buffer_size_;
    if (decoder_.has_next()) {
      push(decoder_.next());
    }

    return result;
  }

 private:
  void push(unicode_t c) {
    assert(lookahead_buffer_size() < 8);
    buffer_[(buffer_offset_ + buffer_size_) % 8] = c;
    ++buffer_size_;
  }

  Utf8Decoder decoder_;
  unicode_t buffer_[8];
  int buffer_offset_;
  int buffer_size_;
};

}  // namespace roo_display