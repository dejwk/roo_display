# Waveshare ESP32-S3-Touch-LCD-4.3B

- Example folder contains a basic test sketch

## Hardware overview

### Core System
- **MCU:** ESP32-S3-WROOM-1-N16R8
- **Processor:** Xtensa® 32-bit LX7 dual-core processor, up to 240MHz
- **Memory:** 16MB Flash, 8MB PSRAM, 512KB SRAM, 384KB ROM
- **Wireless:** 2.4GHz Wi-Fi (802.11 b/g/n) and Bluetooth 5 (LE)

### Display & Touch
- **Screen:** 4.3-inch LCD with 800×480 resolution (65K colors)
- **Touch Type:** Capacitive 5-point touch via I2C interface

### Connectivity & Industrial Interfaces
- **Communication:** RS485 and CAN bus support
- **Expansion:** I2C interface for external sensors
- **Isolated IO:** Onboard digital isolated IO (passive/active input, optocoupler-isolated output up to 450mA)
- **Storage:** TF Card Slot for external data logging or assets

### Power & Management
- **Input Voltage:** Supports wide-range 7~36V DC power supply
- **Battery:** Integrated lithium battery management (charging/discharging)
- **RTC:** Real-time clock chip with battery backup

## Arduino IDE Configuration

**Board Selection:**
- Board: `Waveshare ESP32 S3 Touch LCD 4.3B`

**Required Settings:**
- USB CDC On Boot: `Enabled`
- PSRAM: Enabled `OPI PSRAM`

## Dependencies

- [roo_display](https://github.com/dejwk/roo_display) library
- Arduino ESP32 core (tested with 3.2.0)

## Hardware Abstraction

The display requires a hardware configuration header file to function. Include it in your sketch:
```cpp
#include "roo_display/products/waveshare/waveshare-esp32-s3-touch-lcd-43/waveshare_esp32s3_touch_lcd_43.h"
```

See the example sketch in the `examples` folder for complete implementation details.
