#include "roo_display/color/color_mode_indexed.h"

namespace roo_display {

namespace {
Color transparent = color::Transparent;

TransparencyMode DeterminePaletteTransparencyMode(const Color* palette,
                                                  int palette_size) {
  TransparencyMode mode = TransparencyMode::kNone;
  for (int i = 0; i < palette_size; ++i) {
    uint8_t a = palette[i].a();
    if (a < 255) {
      if (a == 0) {
        mode = TransparencyMode::kCrude;
      } else {
        return TransparencyMode::kFull;
      }
    }
  }
  return mode;
}

}  // namespace

Palette::Palette()
    : Palette(nullptr, &transparent, 1, TransparencyMode::kFull) {}

Palette Palette::ReadOnly(const Color* colors, int size) {
  return Palette(nullptr, colors, size,
                 DeterminePaletteTransparencyMode(colors, size));
}

Palette Palette::ReadOnly(const Color* colors, int size,
                          TransparencyMode transparency_mode) {
  return Palette(nullptr, colors, size, transparency_mode);
}

Palette Palette::ReadWrite(const Color* colors, int size) {
  return ReadWrite(colors, size,
                   DeterminePaletteTransparencyMode(colors, size));
}

Palette Palette::ReadWrite(const Color* colors, int size,
                           TransparencyMode transparency_mode) {
  std::unique_ptr<internal::PaletteIndex> index(
      new internal::PaletteIndex(colors, size));
  return Palette(std::move(index), colors, size, transparency_mode);
}

Palette Palette::Dynamic(int size) {
  // Create a dynamic index.
  std::unique_ptr<internal::PaletteIndex> index(
      new internal::PaletteIndex(size));
  const Color* palette = index->palette();
  return Palette(std::move(index), palette, 0, TransparencyMode::kNone);
}

// Returns the index of a specified color, if exists in the palette.
// If the palette does not contain the specified color, or if buildIndex() has
// not been called, returns zero.
uint8_t Palette::getIndexOfColor(Color color) {
  assert(index_ != nullptr);
  auto itr = index_->find(color);
  if (itr != index_->end()) return *itr;
  uint8_t idx = index_->maybeAddColor(color);
  if (idx >= size_) {
    // Indeed, added.
    size_ = idx + 1;
    if (color.a() != 0xFF) {
      if (color.a() != 0) {
        transparency_mode_ = TransparencyMode::kFull;
      } else if (transparency_mode_ == TransparencyMode::kNone) {
        transparency_mode_ = TransparencyMode::kCrude;
      }
    }
  }
  return idx;
}

}  // namespace roo_display
