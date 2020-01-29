#pragma once

#include <pgmspace.h>

#include "font.h"

namespace roo_display {

// Anti-aliased font implementation, based on data stored in PROGMEM.
class SmoothFont : public Font {
 public:
  SmoothFont(const uint8_t *font_data PROGMEM);
  void drawHorizontalString(const Surface &s, const uint8_t *utf8_data,
                            uint32_t size, Color color) const override;

  GlyphMetrics getHorizontalStringMetrics(const uint8_t *utf8_data,
                                          uint32_t size) const override;

  bool getGlyphMetrics(unicode_t code, FontLayout layout,
                       GlyphMetrics *result) const override;

 private:
  bool rle() const { return compression_method_ > 0; }
  int16_t kerning(unicode_t left, unicode_t right) const;

  friend class GlyphMetadataReader;
  friend class GlyphPairIterator;

  // Binary-search for the glyph data.
  const uint8_t *PROGMEM findGlyph(unicode_t code) const;
  const uint8_t *PROGMEM findKernPair(unicode_t left, unicode_t right) const;

  // Helper methods for drawing glyphs.
  void drawGlyphNoBackground(DisplayOutput *device, int16_t x, int16_t y,
                             const GlyphMetrics &metrics,
                             const uint8_t *PROGMEM data, const Box &clip_box,
                             Color color) const;

  void drawGlyphWithBackground(DisplayOutput *device, int16_t x, int16_t y,
                               int16_t bgwidth, const GlyphMetrics &metrics,
                               const uint8_t *PROGMEM data, int16_t offset,
                               const Box &clip_box, Color color,
                               Color bgColor) const;

  void drawKernedGlyphsWithBackground(
      DisplayOutput *device, int16_t x, int16_t y, int16_t bgwidth,
      const GlyphMetrics &left_metrics, const uint8_t *PROGMEM left_data,
      int16_t left_offset, const GlyphMetrics &right_metrics,
      const uint8_t *PROGMEM right_data, int16_t right_offset,
      const Box &clip_box, Color color, Color bgColor) const;

  int glyph_count_;
  int glyph_metadata_size_;
  int alpha_bits_;
  int encoding_bytes_;
  int font_metric_bytes_;
  int offset_bytes_;
  int compression_method_;
  int kerning_pairs_count_;
  int glyph_kerning_size_;
  int default_space_width_;
  unicode_t default_glyph_;
  const uint8_t *glyph_metadata_begin_ PROGMEM;
  const uint8_t *glyph_kerning_begin_ PROGMEM;
  const uint8_t *glyph_data_begin_ PROGMEM;
};

}  // namespace roo_display