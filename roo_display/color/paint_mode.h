#pragma once

namespace roo_display {

// When drawing using semi-transparent colors, specified if and how the
// previous content is combined with the new content.
enum PaintMode {
  // The new ARGB8888 value is alpha-blended over the old one. This is the
  // default paint mode.
  PAINT_MODE_BLEND,

  // The new ARGB8888 value completely replaces the old one.
  PAINT_MODE_REPLACE,
};

}  // namespace roo_display