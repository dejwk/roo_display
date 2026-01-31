#pragma once

namespace roo_display {

enum SpiDataMode { kSpiMode0 = 0, kSpiMode1 = 1, kSpiMode2 = 2, kSpiMode3 = 3 };

enum SpiBitOrder { kSpiLsbFirst = 0, kSpiMsbFirst = 1 };

template <uint32_t _clock, SpiBitOrder _bit_order, SpiDataMode _data_mode>
struct SpiSettings {
  static constexpr uint32_t clock = _clock;
  static constexpr SpiBitOrder bit_order = _bit_order;
  static constexpr SpiDataMode data_mode = _data_mode;
};

}  // namespace roo_display
