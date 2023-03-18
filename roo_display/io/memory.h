#pragma once

// For use with low-level, high-performance image drawing (fonts, progmem
// images).

#include <type_traits>

#include "pgmspace.h"
#include "roo_display/io/stream.h"

namespace roo_display {

template <typename PtrType>
using IsPtrWritable =
    std::enable_if<!std::is_const<decltype(*std::declval<PtrType>())>::value>;

namespace internal {

template <typename PtrType>
class MemoryStream {
 public:
  MemoryStream(PtrType ptr) : ptr_(ptr) {}

  // The caller must ensure that the pointer won't overflow.
  uint8_t read() { return *ptr_++; }

  // The caller must ensure that the pointer won't overflow.
  void skip(int32_t count) { ptr_ += count; }

  template <typename = typename IsPtrWritable<PtrType>::type>
  void write(uint8_t datum) {
    *ptr_++ = datum;
  }

  const PtrType ptr() const { return ptr_; }

 private:
  PtrType ptr_;
};

typedef MemoryStream<uint8_t*> DramStream;
typedef MemoryStream<const uint8_t*> ConstDramStream;
typedef MemoryStream<const uint8_t PROGMEM*> PrgMemStream;

}  // namespace internal

template <typename PtrType>
class MemoryPtr {
 public:
  MemoryPtr(PtrType ptr) : ptr_(ptr) {}

  internal::MemoryStream<PtrType> Open() const {
    return internal::MemoryStream<PtrType>(ptr_);
  }

 private:
  PtrType ptr_;
};

using DramPtr = MemoryPtr<uint8_t*>;
using ConstDramPtr = MemoryPtr<const uint8_t*>;
using ProgMemPtr = MemoryPtr<const uint8_t PROGMEM*>;

// DEPRECATED; for backwards compatibility.
using PrgMemResource = ProgMemPtr;

}  // namespace roo_display