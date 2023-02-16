#pragma once

#include <Print.h>
#include <stdarg.h>
#include <stdio.h>

#include <string>

namespace roo_display {

std::string StringPrintf(const char* format, ...);
std::string StringVPrintf(const char* format, va_list arg);

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

  // Deprecated; use StringPrintf.
  static std::string sprintf(const char* format, ...) {
    va_list arg;
    va_start(arg, format);
    std::string result = StringVPrintf(format, arg);
    va_end(arg);
    return result;
  }

 private:
  std::string s_;
};

}  // namespace roo_display