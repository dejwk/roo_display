#pragma once

#include "roo_backport/byte.h"
#include "roo_io/memory/memory_iterable.h"

namespace roo_display {

class ProgMemPtr
    : public roo_io::UnsafeGenericMemoryIterable<const roo::byte * PROGMEM> {
 public:
  using BaseT =
      roo_io::UnsafeGenericMemoryIterable<const roo::byte * PROGMEM>;

  using BaseT::BaseT;

  // Legacy constructor to work with old images using uint8_t without cast.
  ProgMemPtr(const void PROGMEM* ptr) : BaseT((const roo::byte*)ptr) {}
};

class ConstDramPtr
    : public roo_io::UnsafeGenericMemoryIterable<const roo::byte*> {
 public:
  using BaseT = roo_io::UnsafeGenericMemoryIterable<const roo::byte*>;

  using BaseT::BaseT;

  // Legacy constructor to work with old code using uint8_t without cast.
  ConstDramPtr(const void* ptr) : BaseT((const roo::byte*)ptr) {}
};

}  // namespace roo_display
