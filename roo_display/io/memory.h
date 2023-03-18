#pragma once

// For use with low-level, high-performance image drawing (fonts, progmem
// images).

#include <type_traits>

#include "pgmspace.h"
#include "roo_display/io/resource.h"
#include "roo_display/io/stream.h"

namespace roo_display {

template <typename PtrType>
using IsPtrWritable =
    std::enable_if<!std::is_const<decltype(*std::declval<PtrType>())>::value>;

namespace internal {

template <typename PtrType>
class MemoryPtrStream {
 public:
  MemoryPtrStream(PtrType ptr) : ptr_(ptr) {}

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

typedef MemoryPtrStream<uint8_t*> DramPtrStream;
typedef MemoryPtrStream<const uint8_t*> ConstDramPtrStream;
typedef MemoryPtrStream<const uint8_t PROGMEM*> ProgMemPtrStream;

}  // namespace internal

template <typename PtrType>
class MemoryPtr {
 public:
  MemoryPtr(PtrType ptr) : ptr_(ptr) {}

  internal::MemoryPtrStream<PtrType> createRawStream() const {
    return internal::MemoryPtrStream<PtrType>(ptr_);
  }

 private:
  PtrType ptr_;
};

using DramPtr = MemoryPtr<uint8_t*>;
using ConstDramPtr = MemoryPtr<const uint8_t*>;
using ProgMemPtr = MemoryPtr<const uint8_t PROGMEM*>;

// DEPRECATED; for backwards compatibility.
using PrgMemResource = ProgMemPtr;

namespace internal {

template <typename PtrType>
class MemoryStream : public ResourceStream {
 public:
  MemoryStream(PtrType begin, PtrType end)
      : begin_(begin), end_(end), current_(begin) {}

  int read(uint8_t* buf, int count) override {
    if (count > end_ - current_) {
      count = end_ - current_;
    }
    memcpy(buf, current_, count);
    current_ += count;
    return count;
  }

  bool skip(uint32_t count) override {
    if (count > end_ - current_) {
      current_ = end_;
    } else {
      current_ += count;
    }
    return true;
  }

  // Returns true on success.
  bool seek(uint32_t offset) override {
    if (current_ + offset > end_) {
      current_ = end_;
    } else {
      current_ += offset;
    }
    return true;
  }

  int size() override {
    return end_ - begin_;
  }

 private:
  PtrType begin_, end_, current_;
};

}  // namespace internal

template <typename PtrType>
class MemoryResource {
 public:
  MemoryResource(PtrType begin, PtrType end) : begin_(begin), end_(end) {}

  std::unique_ptr<internal::MemoryStream<PtrType>> open() const {
    return std::unique_ptr<internal::MemoryStream<PtrType>>(
        new internal::MemoryStream<PtrType>(begin_, end_));
  }

 private:
  PtrType begin_;
  PtrType end_;
};

using ConstDramResource = MemoryResource<const uint8_t*>;
using ProgMemResource = MemoryResource<const uint8_t * PROGMEM>;

}  // namespace roo_display