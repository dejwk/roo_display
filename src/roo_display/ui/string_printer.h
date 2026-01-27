#pragma once

#ifdef ARDUINO
#include <Print.h>
#endif

#include <stdarg.h>
#include <stdio.h>

#include <string>

namespace roo_display {

// prefer roo_io::StringPrintf in new code.
std::string StringPrintf(const char* format, ...);

// prefer roo_io::StringVPrintf in new code.
std::string StringVPrintf(const char* format, va_list arg);

#ifdef ARDUINO
// StringPrinter is a utility that allows formatted writing to std::string.
class StringPrinter : public Print {
 public:
  const std::string& get() const& { return s_; }
  const std::string get() && { return std::move(s_); }

  size_t write(uint8_t c) override {
    s_.append((const char*)(&c));
    return 1;
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    s_.append((const char*)buffer, size);
    return size;
  }

 private:
  std::string s_;
};
#endif

}  // namespace roo_display