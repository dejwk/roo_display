#pragma once

#include <assert.h>
#include <inttypes.h>

namespace roo_display {

typedef uint16_t unicode_t;

// UTF8-encoded string reference.
class StringView {
 public:
  using value_type = uint8_t;
  using pointer = const uint8_t *;
  using const_pointer = const uint8_t *;
  using reference = const uint8_t &;
  using const_reference = const uint8_t &;
  using iterator = const uint8_t *;
  using const_iterator = const uint8_t *;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = const_reverse_iterator;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  static constexpr size_type npos = size_type(-1);

  constexpr StringView() noexcept : len_(0), str_(nullptr) {}

  constexpr StringView(const StringView &) noexcept = default;

  constexpr StringView(const uint8_t *str, size_type len) noexcept
      : len_(len), str_(str) {}

  StringView(const char *str) noexcept
      : len_(strlen(str)), str_(reinterpret_cast<const uint8_t *>(str)) {}

  StringView(const std::string &str) noexcept
      : len_(str.size()),
        str_(reinterpret_cast<const uint8_t *>(str.c_str())) {}

  StringView &operator=(const StringView &) noexcept = default;

  constexpr const_iterator begin() const noexcept { return str_; }

  constexpr const_iterator end() const noexcept { return str_ + len_; }

  constexpr const_iterator cbegin() const noexcept { return str_; }

  constexpr const_iterator cend() const noexcept { return str_ + len_; }

  const_reverse_iterator rbegin() const noexcept {
    return const_reverse_iterator(end());
  }

  const_reverse_iterator rend() const noexcept {
    return const_reverse_iterator(begin());
  }

  const_reverse_iterator crbegin() const noexcept {
    return const_reverse_iterator(end());
  }

  const_reverse_iterator crend() const noexcept {
    return const_reverse_iterator(begin());
  }

  constexpr size_type size() const noexcept { return len_; }

  constexpr bool empty() const noexcept { return len_ == 0; }

  constexpr const uint8_t &operator[](size_type pos) const noexcept {
    return str_[pos];
  }

  constexpr const uint8_t &at(size_type pos) const { return str_[pos]; }

  constexpr const uint8_t &front() const noexcept { return str_[0]; }

  constexpr const uint8_t &back() const noexcept { return str_[len_ - 1]; }

  constexpr const uint8_t *data() const noexcept { return str_; }

  void remove_prefix(size_type n) noexcept {
    assert(len_ >= n);
    str_ += n;
    len_ -= n;
  }

  void remove_suffix(size_type n) noexcept {
    assert(len_ >= n);
    len_ -= n;
  }

  void swap(StringView &sv) noexcept {
    auto tmp = *this;
    *this = sv;
    sv = tmp;
  }

  StringView substr(size_type pos, size_type n = npos) const {
    assert(pos <= len_);
    const size_type rlen = std::min(n, len_ - pos);
    return StringView(str_ + pos, rlen);
  }

  int compare(StringView str) const noexcept {
    const size_type rlen = std::min(len_, str.len_);
    int ret = strncmp(reinterpret_cast<const char *>(str_),
                      reinterpret_cast<const char *>(str.str_), rlen);
    if (ret == 0) {
      ret = (len_ < str.len_) ? -1 : (len_ > str.len_) ? 1 : 0;
    }
    return ret;
  }

 private:
  size_t len_;
  const uint8_t *str_;
};

inline bool operator==(StringView x, StringView y) noexcept {
  return x.size() == y.size() && x.compare(y) == 0;
}

inline bool operator!=(StringView x, StringView y) noexcept {
  return !(x == y);
}

inline bool operator<(StringView x, StringView y) noexcept {
  return x.compare(y) < 0;
}

inline bool operator>(StringView x, StringView y) noexcept {
  return x.compare(y) > 0;
}

inline bool operator<=(StringView x, StringView y) noexcept {
  return x.compare(y) <= 0;
}

inline bool operator>=(StringView x, StringView y) noexcept {
  return x.compare(y) >= 0;
}

inline std::ostream &operator<<(std::ostream &os, StringView v) {
  for (size_t i = 0; i < v.size(); ++i) os << v[i];
  return os;
}

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
      : data_(data), remaining_(size) {}

  Utf8Decoder(const char *data, uint32_t size)
      : data_((const uint8_t *)data), remaining_(size) {}

  bool has_next() const { return remaining_ > 0; }
  const uint8_t *data() const { return data_; }
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
  const uint8_t *data_;
  uint32_t remaining_;
};

// Helper Utf8 iterator that allows to peek at up to 8 subsequent
// characters before consuming the next one. Useful for fonts supporting
// kerning and ligatures.
class Utf8LookAheadDecoder {
 public:
  Utf8LookAheadDecoder(StringView v)
      : Utf8LookAheadDecoder(v.data(), v.size()) {}

  Utf8LookAheadDecoder(const uint8_t *data, uint32_t size)
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