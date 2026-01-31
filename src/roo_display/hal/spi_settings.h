#pragma once

namespace roo_display {

template <uint32_t _clock, uint8_t _bit_order, uint8_t _data_mode>
struct SpiSettings {
  static constexpr uint32_t clock = _clock;
  static constexpr uint8_t bit_order = _bit_order;
  static constexpr uint8_t data_mode = _data_mode;
};

}  // namespace roo_display
