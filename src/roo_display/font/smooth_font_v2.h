#pragma once

#include "roo_display/font/font.h"

namespace roo_display {

// Version 2.0 smooth font with split cmap and glyph metrics format.
class SmoothFontV2 : public Font {
 public:
  SmoothFontV2(const roo::byte* font_data PROGMEM);

  bool getGlyphMetrics(char32_t code, FontLayout layout,
                       GlyphMetrics* result) const override;

  int16_t kerning(char32_t left, char32_t right) const override;

 private:
  // Binary-search for the glyph index in the cmap.
  int findGlyphIndex(char32_t code) const;
  const roo::byte* PROGMEM findKernPair(char32_t left, char32_t right) const;

  void drawHorizontalString(const Surface& s, const char* utf8_data,
                            uint32_t size, Color color) const override;
  GlyphMetrics getHorizontalStringMetrics(const char* utf8_data,
                                          uint32_t size) const override;
  uint32_t getHorizontalStringGlyphMetrics(const char* utf8_data,
                                           uint32_t size,
                                           GlyphMetrics* result,
                                           uint32_t offset,
                                           uint32_t max_count) const override;

  void drawGlyphModeVisible(DisplayOutput& output, int16_t x, int16_t y,
                            const GlyphMetrics& metrics, bool compressed,
                            const roo::byte* PROGMEM data, const Box& clip_box,
                            Color color, Color bgcolor,
                            BlendingMode blending_mode) const;

  void drawGlyphModeFill(DisplayOutput& output, int16_t x, int16_t y,
                         int16_t bgwidth, const GlyphMetrics& glyph_metrics,
                         bool compressed, const roo::byte* PROGMEM data,
                         int16_t offset, const Box& clip_box, Color color,
                         Color bgColor, BlendingMode blending_mode) const;

  void drawKernedGlyphsModeFill(
      DisplayOutput& output, int16_t x, int16_t y, int16_t bgwidth,
      const GlyphMetrics& left_metrics, bool left_compressed,
      const roo::byte* PROGMEM left_data, int16_t left_offset,
      const GlyphMetrics& right_metrics, bool right_compressed,
      const roo::byte* PROGMEM right_data, int16_t right_offset,
      const Box& clip_box, Color color, Color bgColor,
      BlendingMode blending_mode) const;

  void drawBordered(DisplayOutput& output, int16_t x, int16_t y,
                    int16_t bgwidth, const Drawable& glyph,
                    const Box& clip_box, Color bgColor,
                    BlendingMode blending_mode) const;

  bool rle() const { return compression_method_ > 0; }

  int glyph_count_;
  int glyph_metadata_size_;
  int alpha_bits_;
  int encoding_bytes_;
  int font_metric_bytes_;
  int offset_bytes_;
  int compression_method_;
  int kerning_pairs_count_;
  int glyph_kerning_size_;
  char32_t default_glyph_;
  int default_space_width_;
  const roo::byte* glyph_cmap_begin_ PROGMEM;
  const roo::byte* glyph_metadata_begin_ PROGMEM;
  const roo::byte* glyph_kerning_begin_ PROGMEM;
  const roo::byte* glyph_data_begin_ PROGMEM;
};

}  // namespace roo_display