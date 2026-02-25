#pragma once

#ifdef ARDUINO
#ifdef __AVR__
#else
#include <pgmspace.h>
#endif
#else  // !ARDUINO
#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif

#ifndef pgm_read_word
#define pgm_read_word(addr)           \
  ({                                  \
    typeof(addr) _addr = (addr);      \
    *(const unsigned short *)(_addr); \
  })
#endif

#ifndef pgm_read_dword
#define pgm_read_dword(addr)         \
  ({                                 \
    typeof(addr) _addr = (addr);     \
    *(const unsigned long *)(_addr); \
  })
#endif

#ifndef pgm_read_float
#define pgm_read_float(addr)     \
  ({                             \
    typeof(addr) _addr = (addr); \
    *(const float *)(_addr);     \
  })
#endif

#ifndef pgm_read_ptr
#define pgm_read_ptr(addr)       \
  ({                             \
    typeof(addr) _addr = (addr); \
    *(void *const *)(_addr);     \
  })
#endif
#endif