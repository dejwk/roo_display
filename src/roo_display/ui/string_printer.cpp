#include "roo_display/ui/string_printer.h"

#include "roo_io/text/string_printf.h"

namespace roo_display {

std::string StringPrintf(const char* format, ...) {
  va_list arg;
  va_start(arg, format);
  std::string result = StringVPrintf(format, arg);
  va_end(arg);
  return result;
}

std::string StringVPrintf(const char* format, va_list arg) {
  return roo_io::StringVPrintf(format, arg);
}

}