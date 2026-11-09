#pragma once

#include <cstdint>
#include <initializer_list>

#include "roo_display/hal/config.h"

#if defined(ESP_PLATFORM)
#include "esp_rom_sys.h"
#include "roo_display/driver/common/gpio_setter.h"
#include "roo_threads/thread.h"
#include "roo_time.h"
#endif

namespace roo_display::st7701 {

/// Names the ST7701S command bytes used to initialize RGB panels.
/// Command 2 register IDs are interpreted in the bank selected by CMD2BKXSEL.
enum Command : uint8_t {
  SWRESET = 0x01,
  SLPOUT = 0x11,
  INVON = 0x21,
  DISPON = 0x29,
  MADCTL = 0x36,
  COLMOD = 0x3A,

  CMD2BKXSEL = 0xFF,

  BK0_PVGAMCTRL = 0xB0,
  BK0_NVGAMCTRL = 0xB1,
  BK0_LNESET = 0xC0,
  BK0_PORCTRL = 0xC1,
  BK0_INVSET = 0xC2,
  BK0_COLCTRL = 0xCD,

  BK1_VRHS = 0xB0,
  BK1_VCOMS = 0xB1,
  BK1_VGHSS = 0xB2,
  BK1_TESTCMD = 0xB3,
  BK1_VGLS = 0xB5,
  BK1_PWCTRL1 = 0xB7,
  BK1_PWCTRL2 = 0xB8,
  BK1_SPD1 = 0xC1,
  BK1_SPD2 = 0xC2,

  // BK0 E1h is NRCTRL. The BK1 E0h-EDh panel GIP settings are not named
  // in the public datasheet.
  BK1_GIP_E0 = 0xE0,
  BK1_GIP_E1 = 0xE1,
  BK1_GIP_E2 = 0xE2,
  BK1_GIP_E3 = 0xE3,
  BK1_GIP_E4 = 0xE4,
  BK1_GIP_E5 = 0xE5,
  BK1_GIP_E6 = 0xE6,
  BK1_GIP_E7 = 0xE7,
  BK1_GIP_E8 = 0xE8,
  BK1_GIP_EB = 0xEB,
  BK1_GIP_EC = 0xEC,
  BK1_GIP_ED = 0xED,

  // The Makerfabs sequence uses this undocumented register in BK3.
  BK3_E5 = 0xE5,
};

/// Initializes the Makerfabs 480x480 ST7701S panel with its RGB setup values.
struct Init {
  /// Sends the panel commands through @p target and waits after reset and
  /// sleep-out.
  /// The target must provide writeCommand(command, parameters, delay_ms).
  template <typename Target>
  void init(Target& target) const {
    // The Makerfabs board has no dedicated display reset GPIO.
    target.writeCommand(SWRESET, {}, 120);
    target.writeCommand(CMD2BKXSEL, {0x77, 0x01, 0x00, 0x00, 0x10});
    target.writeCommand(BK0_LNESET, {0x3B, 0x00});
    target.writeCommand(BK0_PORCTRL, {0x0D, 0x02});
    target.writeCommand(BK0_INVSET, {0x31, 0x05});
    target.writeCommand(BK0_COLCTRL, {0x08});
    target.writeCommand(BK0_PVGAMCTRL,
                        {0x00, 0x11, 0x18, 0x0E, 0x11, 0x06, 0x07, 0x08, 0x07,
                         0x22, 0x04, 0x12, 0x0F, 0xAA, 0x31, 0x18});
    target.writeCommand(BK0_NVGAMCTRL,
                        {0x00, 0x11, 0x19, 0x0E, 0x12, 0x07, 0x08, 0x08, 0x08,
                         0x22, 0x04, 0x11, 0x11, 0xA9, 0x32, 0x18});

    target.writeCommand(CMD2BKXSEL, {0x77, 0x01, 0x00, 0x00, 0x11});
    target.writeCommand(BK1_VRHS, {0x60});
    target.writeCommand(BK1_VCOMS, {0x32});
    target.writeCommand(BK1_VGHSS, {0x07});
    target.writeCommand(BK1_TESTCMD, {0x80});
    target.writeCommand(BK1_VGLS, {0x49});
    target.writeCommand(BK1_PWCTRL1, {0x85});
    target.writeCommand(BK1_PWCTRL2, {0x21});
    target.writeCommand(BK1_SPD1, {0x78});
    target.writeCommand(BK1_SPD2, {0x78});
    target.writeCommand(BK1_GIP_E0, {0x00, 0x1B, 0x02});
    target.writeCommand(BK1_GIP_E1, {0x08, 0xA0, 0x00, 0x00, 0x07, 0xA0, 0x00,
                                     0x00, 0x00, 0x44, 0x44});
    target.writeCommand(BK1_GIP_E2, {0x11, 0x11, 0x44, 0x44, 0xED, 0xA0, 0x00,
                                     0x00, 0xEC, 0xA0, 0x00, 0x00});
    target.writeCommand(BK1_GIP_E3, {0x00, 0x00, 0x11, 0x11});
    target.writeCommand(BK1_GIP_E4, {0x44, 0x44});
    target.writeCommand(
        BK1_GIP_E5, {0x0A, 0xE9, 0xD8, 0xA0, 0x0C, 0xEB, 0xD8, 0xA0, 0x0E, 0xED,
                     0xD8, 0xA0, 0x10, 0xEF, 0xD8, 0xA0});
    target.writeCommand(BK1_GIP_E6, {0x00, 0x00, 0x11, 0x11});
    target.writeCommand(BK1_GIP_E7, {0x44, 0x44});
    target.writeCommand(
        BK1_GIP_E8, {0x09, 0xE8, 0xD8, 0xA0, 0x0B, 0xEA, 0xD8, 0xA0, 0x0D, 0xEC,
                     0xD8, 0xA0, 0x0F, 0xEE, 0xD8, 0xA0});
    target.writeCommand(BK1_GIP_EB, {0x02, 0x00, 0xE4, 0xE4, 0x88, 0x00, 0x40});
    target.writeCommand(BK1_GIP_EC, {0x3C, 0x00});
    target.writeCommand(
        BK1_GIP_ED, {0xAB, 0x89, 0x76, 0x54, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                     0xFF, 0x20, 0x45, 0x67, 0x98, 0xBA});

    target.writeCommand(CMD2BKXSEL, {0x77, 0x01, 0x00, 0x00, 0x13});
    target.writeCommand(BK3_E5, {0xE4});
    target.writeCommand(CMD2BKXSEL, {0x77, 0x01, 0x00, 0x00, 0x00});
    target.writeCommand(INVON);
    target.writeCommand(COLMOD, {0x60});
    target.writeCommand(SLPOUT, {}, 120);
    target.writeCommand(DISPON);
  }
};

#if defined(ESP_PLATFORM)
/// Sends ST7701S commands over a GPIO driven 9-bit, 3-wire SPI connection.
class Spi9Bit {
 public:
  /// Creates a transport using the supplied chip select, clock, and data pins.
  Spi9Bit(int8_t cs_pin, int8_t clock_pin, int8_t data_pin)
      : cs_(cs_pin), clock_(clock_pin), data_(data_pin) {}

  /// Configures the pins for output and leaves chip select inactive.
  void init() {
    cs_.init();
    clock_.init();
    data_.init();
    cs_.setHigh();
    clock_.setLow();
  }

  /// Sends @p command and @p bytes, then waits for @p delay_ms if requested.
  void writeCommand(Command command, std::initializer_list<uint8_t> bytes = {},
                    uint32_t delay_ms = 0) {
    cs_.setLow();
    writeByte(false, command);
    for (uint8_t value : bytes) {
      writeByte(true, value);
    }
    cs_.setHigh();
    if (delay_ms != 0) {
      roo::this_thread::sleep_for(roo_time::Millis(delay_ms));
    }
  }

 private:
  // Clocks the command/data flag followed by one byte, most significant bit
  // first.
  void writeByte(bool is_data, uint8_t value) {
    writeBit(is_data);
    for (int bit = 7; bit >= 0; --bit) {
      writeBit((value & (1U << bit)) != 0);
    }
    clock_.setLow();
  }

  // Applies the bit value before the rising clock edge.
  void writeBit(bool bit) {
    clock_.setLow();
    if (bit) {
      data_.setHigh();
    } else {
      data_.setLow();
    }
    esp_rom_delay_us(1);
    clock_.setHigh();
    esp_rom_delay_us(1);
  }

  GpioSetter cs_;
  GpioSetter clock_;
  GpioSetter data_;
};
#endif

}  // namespace roo_display::st7701
