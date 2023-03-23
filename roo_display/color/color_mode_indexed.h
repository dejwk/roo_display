#pragma once

#include <inttypes.h>

#include "roo_display/color/color.h"
#include "roo_display/color/color_set.h"
#include "roo_display/color/transparency_mode.h"
#include "roo_display/internal/hashtable.h"

namespace roo_display {

class Palette;

namespace internal {

struct PaletteElementKey {
  Color operator()(uint8_t entry) const { return palette[entry]; }
  const Color* palette;
};

class PaletteIndex {
 public:
  using Map = internal::Hashtable<uint8_t, Color, internal::ColorHash,
                                  internal::PaletteElementKey>;

  // For a 'static' index.
  PaletteIndex(const Color* palette, int size)
      : colors_(nullptr),
        index_map_(size, internal::ColorHash(),
                   internal::PaletteElementKey{.palette = palette}),
        max_size_(size),
        size_(size) {
    for (int i = 0; i < size; ++i) {
      index_map_.insert(i);
    }
  }

  // For a 'dynamic' index.
  PaletteIndex(int max_size)
      : colors_(new Color[max_size]),
        index_map_(max_size, internal::ColorHash(),
                   internal::PaletteElementKey{.palette = colors_.get()}),
        max_size_(max_size),
        size_(0) {}

  Map::Iterator find(Color color) const { return index_map_.find(color); }

  Map::Iterator end() const { return index_map_.end(); }

 private:
  friend class ::roo_display::Palette;

  const Color* palette() const { return colors_.get(); }

  int maybeAddColor(Color color) {
    if (colors_ == nullptr || size_ == max_size_) return 0;
    colors_[size_] = color;
    index_map_.insert(size_);
    return size_++;
  }

  // Color storage in case of a dynamic palette.
  std::unique_ptr<Color[]> colors_;

  Map index_map_;
  int max_size_;
  int size_;
};

}  // namespace internal

// Used with IndexedN color modes to store the color palette.
class Palette {
 public:
  // Creates a dummy palette with a single transparent color.
  Palette();

  // Creates an immutable, read-only palette that uses the specified colors.
  // The color array is not copied, and it must remain unchanged for as long as
  // this palette is in use. The palette cannot be used for inverse-lookup, i.e.
  // to resolve ARGB colors to indexes, needed e.g. by Offscreens. For that, use
  // `ReadWrite()` or `Dynamic()`.
  //
  // The transparency mode of the palette is auto-determined by evaluating the
  // colors.
  //
  static Palette ReadOnly(const Color* colors, int size);

  // Similar to the above, but uses the provided transparency mode. Saves some
  // CPU on determining the transparency mode.
  static Palette ReadOnly(const Color* colors, int size,
                          TransparencyMode transparency_mode);

  // Creates an immutable palette that uses the specified colors. The color
  // array is not copied, and it must remain unchanged for as long as this
  // palette is in use. The palette can be used for inverse-lookup, i.e. to
  // resolve ARGB colors to indexes. Hence, it can be used with the Offscreen.
  //
  // The transparency mode of the palette is auto-determined by evaluating the
  // colors.
  //
  static Palette ReadWrite(const Color* colors, int size);

  // Similar to the above, but uses the provided transparency mode. Saves some
  // CPU on determining the transparency mode.
  static Palette ReadWrite(const Color* colors, int size,
                           TransparencyMode transparency_mode);

  // Creates an empty 'dynamic' palette for drawing to offscreens. The colors
  // will be automatically added to the palette up to the specified size.
  // After the size is exhausted, new missing colors will be replaced by the
  // first color added to the palette (i.e. the color with index 0).
  static Palette Dynamic(int max_size);

  // Returns the pointer to the color table of this palette.
  const Color* colors() const { return colors_; }

  // Returns the size of this palette.
  int size() const { return size_; }

  // Returns the `idx`-th color from the palette.
  inline Color getColorAt(int idx) const { return colors_[idx]; }

  TransparencyMode transparency_mode() const { return transparency_mode_; }

  // Returns the index of a specified color. Should not be called (and will
  // assert-fail) if the palette has been created as read-only. If the color is
  // not found, the behavior depends further on how the palette was created: if
  // the palette is dynamic, and the current size is smaller than the maximum
  // size, the color is added at the next available position. Otherwise, if the
  // size is maxed out or the palette is not dynamic, returns zero.
  uint8_t getIndexOfColor(Color color);

 private:
  Palette(std::unique_ptr<internal::PaletteIndex> index, const Color* colors,
          int size, TransparencyMode transparency_mode)
      : index_(std::move(index)),
        colors_(colors),
        size_(size),
        transparency_mode_(transparency_mode) {}

  std::unique_ptr<internal::PaletteIndex> index_;
  const Color* colors_;
  int size_;
  TransparencyMode transparency_mode_;
};

namespace internal {

template <uint8_t bits>
class Indexed {
 public:
  static const int8_t bits_per_pixel = bits;

  Indexed(const Palette* palette) : palette_(palette) {
    assert(palette_->size() >= 0 && palette_->size() <= (1 << bits));
  }

  inline uint8_t fromArgbColor(Color color) {
    return ((Palette*)palette_)->getIndexOfColor(color);
  }

  constexpr TransparencyMode transparency() const {
    return palette_->transparency_mode();
  }

  inline constexpr Color toArgbColor(uint8_t in) const {
    return palette_->getColorAt(in);
  }

  inline uint8_t rawAlphaBlend(uint8_t bg, Color fg) {
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