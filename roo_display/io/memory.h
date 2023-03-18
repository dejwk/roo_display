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
  MemoryStream(PtrType address) : start_(address), current_(address) {}

  // The caller must ensure that the pointer won't overflow.
  uint8_t read() { return *current_++; }

  // The caller must ensure that the pointer won't overflow.
  void skip(int32_t count) { current_ += count; }

  template <typename = typename IsPtrWritable<PtrType>::type>
  void write(uint8_t datum) {
    *current_++ = datum;
  }

  const PtrType ptr() const { return current_; }

 private:
  PtrType start_;
  PtrType current_;
};

typedef MemoryStream<uint8_t*> DramStream;
typedef MemoryStream<const uint8_t*> ConstDramStream;
typedef MemoryStream<const uint8_t PROGMEM*> PrgMemStream;

}  // namespace internal

template <typename PtrType>
class MemoryResource {
 public:
  MemoryResource(PtrType ptr) : ptr_(ptr) {}

  internal::MemoryStream<PtrType> Open() const {
    return internal::MemoryStream<PtrType>(ptr_);
  }

 private:
  PtrType ptr_;
};

typedef MemoryResource<uint8_t*> DramResource;
typedef MemoryResource<const uint8_t*> ConstDramResource;
typedef MemoryResource<const uint8_t PROGMEM*> PrgMemResource;

}  // namespace roo_display