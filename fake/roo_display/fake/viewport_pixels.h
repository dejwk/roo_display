#pragma once

#include <memory>
#include <type_traits>

#include "roo_testing/transducers/ui/viewport/viewport.h"

namespace roo_display::internal {

template <typename Pixels>
Pixels ViewportDrawRectPixels(void (roo_testing_transducers::Viewport::*)(
    int16_t, int16_t, int16_t, int16_t, Pixels));

// Keep the simulator buildable against the released raw-pointer interface
// while adopting the unique_ptr interface in newer roo_testing checkouts.
using ViewportPixels = decltype(ViewportDrawRectPixels(
    &roo_testing_transducers::Viewport::drawRect));

// Transfers the allocation using the installed viewport interface.
template <typename Pixels = ViewportPixels>
Pixels TransferViewportPixels(std::unique_ptr<uint32_t[]> pixels) {
  if constexpr (std::is_pointer_v<Pixels>) {
    return pixels.release();
  } else {
    return pixels;
  }
}

}  // namespace roo_display::internal
