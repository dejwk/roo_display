#pragma once

#include "font.h"

namespace roo_display {

class FontAdafruitFixed5x7 : public Font {
 public:
  FontAdafruitFixed5x7();
  void drawHorizontalString(const Surface &s, const char *utf8_data,
                            uint32_t size, Color color) const override;

  bool getGlyphMetrics(char32_t code, FontLayout layout,
                       GlyphMetrics *result) const override;

  GlyphMetrics getHorizontalStringMetrics(const char *utf8_data,
                                          uint32_t size) const override;

  uint32_t getHorizontalStringGlyphMetrics(const char *utf8_data, uint32_t size,
                                           GlyphMetrics *result,
                                           uint32_t offset,
                                           uint32_t max_count) const override;
};

}  // namespace roo_display