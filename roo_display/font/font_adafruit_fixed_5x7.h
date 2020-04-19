#pragma once

#include "font.h"

namespace roo_display {

class FontAdafruitFixed5x7 : public Font {
 public:
  FontAdafruitFixed5x7();
  void drawHorizontalString(const Surface &s, const uint8_t *utf8_data,
                            uint32_t size, Color color) const override;

  bool getGlyphMetrics(unicode_t code, FontLayout layout,
                       GlyphMetrics *result) const override;

  GlyphMetrics getHorizontalStringMetrics(const uint8_t *utf8_data,
                                          uint32_t size) const override;
};

}  // namespace roo_display