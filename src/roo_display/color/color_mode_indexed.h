#pragma once

#include <inttypes.h>

#include "roo_collections.h"
#include "roo_collections/flat_small_hashtable.h"
#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_display/color/color_set.h"

namespace roo_display {

class Palette;

namespace internal {

struct PaletteElementKey {
  Color operator()(uint8_t entry) const { return palette[entry]; }
  const Color* palette;
};

class PaletteIndex {
 public:
  using Map =
      roo_collections::FlatSmallHashtable<uint8_t, Color, internal::ColorHash,
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

  Map::ConstIterator find(Color color) const { return index_map_.find(color); }

  Map::ConstIterator end() const { return index_map_.end(); }

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

/// Palette storage for `IndexedN` color modes.
class Palette {
 public:
  /// Create a dummy palette with a single transparent color.
  Palette();

  /// Create a read-only palette backed by the given colors.
  ///
  /// The color array is not copied and must remain valid. Read-only palettes
  /// cannot be used for inverse lookup (ARGB -> index); use `ReadWrite()` or
  /// `Dynamic()` for that. Transparency is auto-detected.
  ///
  /// @param colors Palette colors (array must outlive the palette).
  /// @param size Number of colors in the palette.
  /// @return Read-only palette instance.
  static Palette ReadOnly(const Color* colors, int size);

  /// Read-only palette with explicit transparency mode.
  ///
  /// @param colors Palette colors (array must outlive the palette).
  /// @param size Number of colors in the palette.
  /// @param transparency_mode Explicit transparency mode to use.
  /// @return Read-only palette instance.
  static Palette ReadOnly(const Color* colors, int size,
                          TransparencyMode transparency_mode);

  /// Create a read/write palette backed by the given colors.
  ///
  /// Enables inverse lookup (ARGB -> index), e.g. for Offscreen. The array is
  /// not copied and must remain valid. Transparency is auto-detected.
  ///
  /// @param colors Palette colors (array must outlive the palette).
  /// @param size Number of colors in the palette.
  /// @return Read/write palette instance.
  static Palette ReadWrite(const Color* colors, int size);

  /// Read/write palette with explicit transparency mode.
  ///
  /// @param colors Palette colors (array must outlive the palette).
  /// @param size Number of colors in the palette.
  /// @param transparency_mode Explicit transparency mode to use.
  /// @return Read/write palette instance.
  static Palette ReadWrite(const Color* colors, int size,
                           TransparencyMode transparency_mode);

  /// Create a dynamic palette for drawing to offscreens.
  ///
  /// Colors are added up to `max_size`. After that, missing colors map to
  /// index 0.
  ///
  /// @param max_size Maximum number of colors to store.
  /// @return Dynamic palette instance.
  static Palette Dynamic(int max_size);

  /// Return pointer to the color table.
  const Color* colors() const { return colors_; }

  /// Return palette size.
  int size() const { return size_; }

  /// Return the `idx`-th color.
  inline Color getColorAt(int idx) const { return colors_[idx]; }

  /// Return palette transparency mode.
  TransparencyMode transparency_mode() const { return transparency_mode_; }

  /// Return the index of a specified color.
  ///
  /// Not valid for read-only palettes. For dynamic palettes, missing colors
  /// are added up to `max_size`; otherwise returns 0 when not found.
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

/// Indexed color mode with `bits` bits per pixel.
template <uint8_t bits>
class Indexed {
 public:
  static const int8_t bits_per_pixel = bits;

  Indexed(const Palette* palette) : palette_(palette) {
    assert(palette_->size() >= 0 && palette_->size() <= (1 << bits));
  }

  inline uint8_t fromArgbColor(Color color) const {
    return const_cast<Palette*>(palette_)->getIndexOfColor(color);
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