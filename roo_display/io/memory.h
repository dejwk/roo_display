#pragma once

#include <type_traits>

#include "pgmspace.h"
#include "roo_display/io/stream.h"

namespace roo_display {

template <typename PtrType>
using IsPtrWritable =
    std::enable_if<!std::is_const<decltype(*std::declval<PtrType>())>::value>;

template <typename PtrType>
class MemoryStream {
 public:
  MemoryStream(PtrType address) : start_(address), current_(address) {}

  uint8_t read() { return *current_++; }

  int read(uint8_t* buf, int count) {
    memcpy(buf, current_, count);
    current_ += count;
    return count;
  }

  void advance(int32_t count) { current_ += count; }

  void seek(uint32_t offset) { current_ = start_ + offset; }

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

template <typename PtrType>
class MemoryResource {
 public:
  MemoryResource(PtrType ptr) : ptr_(ptr) {}

  MemoryStream<PtrType> Open() const {
    return MemoryStream<PtrType>(ptr_);
  }

 private:
  PtrType ptr_;
};

typedef MemoryResource<uint8_t*> DramResource;
typedef MemoryResource<const uint8_t*> ConstDramResource;
typedef MemoryResource<const uint8_t PROGMEM*> PrgMemResource;

}  // namespace roo_display