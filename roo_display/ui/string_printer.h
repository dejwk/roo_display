#pragma once

#include <Print.h>
#include <stdarg.h>
#include <stdio.h>
#include <string>

namespace roo_display {

// StringPrinter is a utility that allows formatted writing to std::string.
class StringPrinter : public Print {
 public:
  const std::string& get() const & { return s_; }
  const std::string get() && { return std::move(s_); }

  size_t write(uint8_t c) override {
    s_.append((const char*)c);
    return 1;
  }

  size_t write(const uint8_t* buffer, size_t size) override {
    s_.append((const char*)buffer, size);
    return size;
  }

  static std::string sprintf(const char * format, ...)  {
    StringPrinter printer;
    char loc_buf[1024];
    va_list arg;
    va_start(arg, format);
    int len = vsnprintf(loc_buf, 1024, format, arg);
    if (len > 0) {
      printer.write((uint8_t*)loc_buf, len);
    }
    va_end(arg);
    return std::move(printer).get();
  }

 private:
  std::string s_;
};

}  // namespace roo_display