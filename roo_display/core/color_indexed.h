#pragma once

#include "roo_display/core/color.h"
#include "roo_display/internal/hashtable.h"

namespace roo_display {

namespace internal {

struct PaletteElementKey {
  Color operator()(uint8_t entry) const { return palette_[entry]; }
  const Color* palette_;
};

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

}  // namespace internal

class PaletteIndex
    : public internal::Hashtable<uint8_t, Color, internal::ColorHash,
                                 internal::PaletteElementKey> {
 public:
  PaletteIndex(const Color* palette, int palette_size)
      : internal::Hashtable<uint8_t, Color, internal::ColorHash,
                            internal::PaletteElementKey>(
            palette_size, internal::ColorHash(),
            internal::PaletteElementKey{.palette_ = palette}) {
    for (int i = 0; i < palette_size; ++i) {
      insert(i);
    }
  }
};

// Used with IndexedN color modes to store the color palette.
class Palette {
 public:
  // Initializes the palette using the specified color array. Auto-determines
  // transparency mode of the palette.
  //
  // The colors array is not copied, and it must remain unchanged for as long as
  // this palette is in use.
  Palette(const Color* colors, int colors_count)
      : Palette(
            colors, colors_count,
            internal::DeterminePaletteTransparencyMode(colors, colors_count)) {}

  // Initializes the palette using the specified color array. Uses the specified
  // transparency mode.
  //
  // The colors array is not copied, and it must remain unchanged for as long as
  // this palette is in use.
  Palette(const Color* colors, int colors_count,
          TransparencyMode transparency_mode)
      : colors_(colors),
        max_palette_color_idx_(colors_count - 1),
        transparency_mode_(transparency_mode),
        index_(nullptr) {}

  // If you intend to create offscreens using this palette, make sure you call
  // this; otherwise, the palette will not be able to convert colors to indexes
  // and it will always write using the first color from the palette. We don't
  // do it by default, or even lazily, because we want to make sure that palette
  // objects take up minimum DRAM in the common case when they are used to just
  // draw images using color tables stored in PROGMEM.
  void buildIndex() {
    if (index_ == nullptr) {
      index_ = std::unique_ptr<PaletteIndex>(
          new PaletteIndex(colors_, (int)max_palette_color_idx_ + 1));
    }
  }

  const Color* colors() const { return colors_; }

  // Returns the `idx`-th color from the palette.
  inline Color getColorAt(int idx) const { return colors_[idx]; }

  int size() const { return (int)max_palette_color_idx_ + 1; }

  TransparencyMode transparency_mode() const { return transparency_mode_; }

  // Returns the index of a specified color, if exists in the palette.
  // If the palette does not contain the specified color, or if buildIndex() has
  // not been called, returns zero.
  inline constexpr uint8_t getIndexOfColor(Color color) const {
    if (index_ == nullptr) return 0;
    auto itr = index_->find(color);
    if (itr == index_->end()) return 0;
    return *itr;
  }

 private:
  const Color* colors_;
  uint8_t max_palette_color_idx_;
  TransparencyMode transparency_mode_;
  std::unique_ptr<PaletteIndex> index_;
};

namespace internal {

template <uint8_t bits>
class Indexed {
 public:
  static const int8_t bits_per_pixel = bits;

  Indexed(const Palette* palette) : palette_(palette) {
    assert(palette_->size() > 0 && palette_->size() <= (1 << bits));
  }

  inline constexpr uint8_t fromArgbColor(Color color) const {
    return palette_->getIndexOfColor(color);
  }

  constexpr TransparencyMode transparency() const {
    return palette_->transparency_mode();
  }

  inline constexpr Color toArgbColor(uint8_t in) const {
    return palette_->getColorAt(in);
  }

  inline uint8_t rawAlphaBlend(uint8_t bg, Color fg) const {
    return fromArgbColor(AlphaBlend(toArgbColor(bg), fg));
  }

  const Palette* palette() const { return palette_; }

 private:
  const Palette* palette_;
};

}  // namespace internal

using Indexed1 = internal::Indexed<1>;
using Indexed2 = internal::Indexed<2>;
using Indexed4 = internal::Indexed<4>;
using Indexed8 = internal::Indexed<8>;

};  // namespace roo_display