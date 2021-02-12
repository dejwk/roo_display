#pragma once

#include <type_traits>
#include "roo_display/internal/byte_order.h"

namespace roo_display {

// The Stream template contract abstracts away various sources of
// (primarily image) data, such as: DRAM, PROGMEM, SPIFFS, or SDFS.
// A compliant class looks like follows:

// class Stream {
//  public:
//   uint8_t Read();
//   void advance(int32_t count);
//   void seek(uint32_t offset);
// };

template <typename ByteStreamType>
using IsByteStreamWritable = std::enable_if<
    !std::is_const<decltype(std::declval<ByteStreamType>().write(0))>::value>;

template <typename Resource>
using StreamType = decltype(std::declval<Resource>().Open());

// Helper to read uint16_t from two consecutive bytes, assuming a specified
// byte order.
template <ByteOrder byte_order>
class StreamReader16;

// Helper to read uint32_t from three consecutive bytes, assuming a specified
// byte order.
template <ByteOrder byte_order>
class StreamReader24;

// Helper to read uint32_t from four consecutive bytes, assuming a specified
// byte order.
template <ByteOrder byte_order>
class StreamReader32;

template <>
class StreamReader16<BYTE_ORDER_BIG_ENDIAN> {
 public:
  template <typename Stream>
  uint16_t operator()(Stream* stream) const {
    uint16_t hi = stream->read();
    uint16_t lo = stream->read();
    return (hi << 8) | lo;
  }
};

template <>
class StreamReader16<BYTE_ORDER_LITTLE_ENDIAN> {
 public:
  template <typename Stream>
  uint16_t operator()(Stream* stream) const {
    uint16_t lo = stream->read();
    uint16_t hi = stream->read();
    return (hi << 8) | lo;
  }
};

template <>
class StreamReader24<BYTE_ORDER_BIG_ENDIAN> {
 public:
  template <typename Stream>
  uint32_t operator()(Stream* stream) const {
    uint32_t b2 = stream->read();
    uint32_t b1 = stream->read();
    uint32_t b0 = stream->read();
    return (b2 << 16) | (b1 << 8) | b0;
  }
};

template <>
class StreamReader24<BYTE_ORDER_LITTLE_ENDIAN> {
 public:
  template <typename Stream>
  uint32_t operator()(Stream* stream) const {
    uint32_t b0 = stream->read();
    uint32_t b1 = stream->read();
    uint32_t b2 = stream->read();
    return (b2 << 16) | (b1 << 8) | b0;
  }
};

template <>
class StreamReader32<BYTE_ORDER_BIG_ENDIAN> {
 public:
  template <typename Stream>
  uint32_t operator()(Stream* stream) const {
    uint32_t b3 = stream->read();
    uint32_t b2 = stream->read();
    uint32_t b1 = stream->read();
    uint32_t b0 = stream->read();
    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
  }
};

template <>
class StreamReader32<BYTE_ORDER_LITTLE_ENDIAN> {
 public:
  template <typename Stream>
  uint32_t operator()(Stream* stream) const {
    uint32_t b0 = stream->read();
    uint32_t b1 = stream->read();
    uint32_t b2 = stream->read();
    uint32_t b3 = stream->read();
    return (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
  }
};

// Convenience helper to read network-encoded (big-endian) uint16.
template <typename Stream>
uint16_t read_uint16_be(Stream* in) {
  StreamReader16<BYTE_ORDER_BIG_ENDIAN> read;
  return read(in);
}

// Convenience helper to read network-encoded (big-endian) uint24.
template <typename Stream>
uint32_t read_uint24_be(Stream* in) {
  StreamReader24<BYTE_ORDER_BIG_ENDIAN> read;
  return read(in);
}

// Convenience helper to read network-encoded (big-endian) uint32.
template <typename Stream>
uint32_t read_uint32_be(Stream* in) {
  StreamReader32<BYTE_ORDER_BIG_ENDIAN> read;
  return read(in);
}

}  // namespace roo_display
