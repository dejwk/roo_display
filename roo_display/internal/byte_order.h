#pragma once

// Support for endian-ness conversions between platform-specific and
// platform-independent representations that can be used as template
// parameters.

// The 'integer' template parameters are meant to take values defined in
// <endian.h>, e.g. BYTE_ORDER, LITTLE_ENDIAN, BIG_ENDIAN.
//
// Note: a decent compiler will completely remove 'no-op' conversions,
// and inline swaps using native builtins.

#include <inttypes.h>
#include "endian.h"

namespace roo_display {

enum ByteOrder {
  BYTE_ORDER_BIG_ENDIAN = BIG_ENDIAN,
  BYTE_ORDER_LITTLE_ENDIAN = LITTLE_ENDIAN,
  BYTE_ORDER_NATIVE = BYTE_ORDER
};

namespace byte_order {

// Operator for swapping the byte order.
template <typename type>
struct Swapper;

template <>
struct Swapper<uint8_t> {
  //  template <int endian_from, int endian_to>
  uint8_t operator()(uint8_t in) const { return in; }
};

template <>
struct Swapper<uint16_t> {
  uint16_t operator()(uint16_t in) const {
    return BYTE_ORDER == LITTLE_ENDIAN ? htobe16(in) : htole16(in);
  }
};

template <>
struct Swapper<uint32_t> {
  uint32_t operator()(uint32_t in) const {
    return BYTE_ORDER == LITTLE_ENDIAN ? htobe32(in) : htole32(in);
  }
};

// Operator for converting the byte order from src to dst.
template <typename storage_type, ByteOrder src, ByteOrder dst>
struct Converter {
  storage_type operator()(storage_type in) const {
    Swapper<storage_type> swap;
    return swap(in);
  }
};

// No swapping if src == dst
template <typename storage_type, ByteOrder same>
struct Converter<storage_type, same, same> {
  storage_type operator()(storage_type in) const { return in; }
};

// Utility to convert an integer type from the src to dst byte order.
// If src == dst, compiles to no-op; otherwise, it compiles to
// a built-in byte swap routine.
template <typename storage_type, ByteOrder src, ByteOrder dst>
storage_type Convert(storage_type in) {
  Converter<storage_type, src, dst> convert;
  return convert(in);
}

// A convenience function to convert an integer type from
// host-native byte order to the specified byte order.
template <typename storage_type, ByteOrder dst>
storage_type hto(storage_type in) {
  return Convert<storage_type, BYTE_ORDER_NATIVE, dst>(in);
}

// A convenience function to convert an integer type from
// a specified byte order to the host-native byte order.
template <typename storage_type, ByteOrder src>
storage_type toh(storage_type in) {
  return Convert<storage_type, src, BYTE_ORDER_NATIVE>(in);
}

// A convenience function to convert an integer type from
// host-native byte order to big endian.
template <typename storage_type>
storage_type htobe(storage_type in) {
  return Convert<storage_type, BYTE_ORDER_NATIVE, BYTE_ORDER_BIG_ENDIAN>(in);
}

// A convenience function to convert an integer type from
// host-native byte order to little endian.
template <typename storage_type>
storage_type htole(storage_type in) {
  return Convert<storage_type, BYTE_ORDER_NATIVE, BYTE_ORDER_LITTLE_ENDIAN>(in);
}

// A convenience function to convert an integer type from
// big endian to the host-native byte order.
template <typename storage_type>
storage_type betoh(storage_type in) {
  return Convert<storage_type, BYTE_ORDER_BIG_ENDIAN, BYTE_ORDER_NATIVE>(in);
}

// A convenience function to convert an integer type from
// little endian to the host-native byte order.
template <typename storage_type>
storage_type letoh(storage_type in) {
  return Convert<storage_type, BYTE_ORDER_LITTLE_ENDIAN, BYTE_ORDER_NATIVE>(in);
}

}  // namespace byte_order
}  // namespace roo_display
