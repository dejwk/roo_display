#pragma once

#ifdef ARDUINO
#ifdef __AVR__
#else
#include <pgmspace.h>
#endif
#else  // !ARDUINO
#define PROGMEM
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#define pgm_read_word(addr)           \
  ({                                  \
    typeof(addr) _addr = (addr);      \
    *(const unsigned short *)(_addr); \
  })
#define pgm_read_dword(addr)         \
  ({                                 \
    typeof(addr) _addr = (addr);     \
    *(const unsigned long *)(_addr); \
  })
#define pgm_read_float(addr)     \
  ({                             \
    typeof(addr) _addr = (addr); \
    *(const float *)(_addr);     \
  })
#define pgm_read_ptr(addr)       \
  ({                             \
    typeof(addr) _addr = (addr); \
    *(void *const *)(_addr);     \
  })
#endif