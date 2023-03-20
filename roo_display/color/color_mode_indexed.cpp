#include "roo_display/color/color_mode_indexed.h"

namespace roo_display {

namespace {
Color transparent = color::Transparent;

TransparencyMode DeterminePaletteTransparencyMode(const Color* palette,
                                                  int palette_size) {
  TransparencyMode mode = TRANSPARENCY_NONE;
  for (int i = 0; i < palette_size; ++i) {
    uint8_t a = palette[i].a();
    if (a < 255) {
      if (a == 0) {
        mode = TRANSPARENCY_BINARY;
      } else {
        return TRANSPARENCY_GRADUAL;
      }
    }
  }
  return mode;
}

}  // namespace

Palette::Palette() : Palette(&transparent, 1) {}

Palette::Palette(const Color* colors, int colors_count)
    : Palette(
          colors, colors_count,
          DeterminePaletteTransparencyMode(colors, colors_count)) {}

}  // namespace roo_display
