#pragma once

#include "roo_io/base/byte.h"
#include "roo_io/memory/memory_iterable.h"

namespace roo_display {

class ProgMemPtr
    : public roo_io::UnsafeGenericMemoryIterable<const roo_io::byte * PROGMEM> {
 public:
  using BaseT =
      roo_io::UnsafeGenericMemoryIterable<const roo_io::byte * PROGMEM>;

  using BaseT::BaseT;

  // Legacy constructor to work with old images using uint8_t without cast.
  ProgMemPtr(const void PROGMEM* ptr) : BaseT((const roo_io::byte*)ptr) {}
};

class ConstDramPtr
    : public roo_io::UnsafeGenericMemoryIterable<const roo_io::byte*> {
 public:
  using BaseT = roo_io::UnsafeGenericMemoryIterable<const roo_io::byte*>;

  using BaseT::BaseT;

  // Legacy constructor to work with old code using uint8_t without cast.
  ConstDramPtr(const void* ptr) : BaseT((const roo_io::byte*)ptr) {}
};

}  // namespace roo_display
