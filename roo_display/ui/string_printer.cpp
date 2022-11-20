#include "roo_display/ui/string_printer.h"

#include <string>

namespace roo_display {

std::string StringPrintf(const char* format, ...) {
  va_list arg;
  va_start(arg, format);
  std::string result = StringVPrintf(format, arg);
  va_end(arg);
  return result;
}

std::string StringVPrintf(const char* format, va_list arg) {
  StringPrinter printer;
  char loc_buf[1024];
  int len = vsnprintf(loc_buf, 1024, format, arg);
  if (len > 0) {
    printer.write((uint8_t*)loc_buf, len);
  }
  return std::move(printer).get();
}

}