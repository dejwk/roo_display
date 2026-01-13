# Waveshare ESP32-S3-Touch-LCD-4.3 Test Sketch

Basic test application for the Waveshare ESP32-S3-Touch-LCD-4.3 display using the roo_display library.

## Hardware Requirements

- Waveshare ESP32-S3-Touch-LCD-4.3
  - 800x480 RGB parallel display
  - GT911 touch controller
  - CH422G I/O expander
  - ESP32-S3 with PSRAM

## Arduino IDE Configuration

**Board Selection:**
- Board: `Waveshare ESP32 S3 Touch LCD 4.3B`

**Required Settings:**
- USB CDC On Boot: `Disabled`
- PSRAM: `OPI PSRAM`

## Dependencies

- [roo_display](https://github.com/dejwk/roo_display) library
- Arduino ESP32 core (tested with 3.2.0)

## Hardware Abstraction

The display requires a hardware configuration header file to function. Include it in your sketch:
```cpp
#include "roo_display/products/waveshare/waveshare-wsp32-S3-touch-lcd-43/waveshare_esp32s3_touch_lcd_43.h"
```

See the example sketch in the `examples` folder for complete implementation details.
