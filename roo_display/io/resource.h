#pragma once

#include <inttypes.h>

namespace roo_display {

// In order to implement a custom resource, write a class that has the following template contract:
//
// class MyResource {
//  public:
//   // ...
//   std::unique_ptr<internal::ResourceStream> open() const;
// };

// Virtualizes access to files, memory, or other sources. Represents an 'open'
// resource with some 'file pointer'.
class ResourceStream {
 public:
  virtual ~ResourceStream() = default;

  // Reads up to `count` bytes. Returns the number of bytes read.
  virtual int read(uint8_t* buf, int count) = 0;

  // Returns true on success.
  virtual bool skip(uint32_t count) = 0;

  // Returns true on success.
  virtual bool seek(uint32_t offset) = 0;

  // Returns the size of the resource in bytes.
  virtual int size() = 0;
};

}  // namespace roo_display
