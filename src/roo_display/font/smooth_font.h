#pragma once

#ifdef __AVR__
#include <avr/pgmspace.h>
#elif defined(ESP8266) || defined(ESP32)
#include <pgmspace.h>
#else
#define pgm_read_byte(addr) (*(const unsigned char *)(addr))
#endif

#include <type_traits>

#include "font.h"
#include "roo_backport/byte.h"

namespace roo_display {

// Anti-aliased font implementation, based on data stored in PROGMEM.
class SmoothFont : public Font {
 public:
  // Legacy constructor to work with old fonts when byte is actually defined as
  // uint8_t.
  template <typename U = uint8_t,
            typename std::enable_if<!std::is_same<U, roo::byte>::value,
                                    int>::type = 0>
  SmoothFont(const U *font_data PROGMEM)
      : SmoothFont((const roo::byte *PROGMEM)font_data) {}

  SmoothFont(const roo::byte *font_data PROGMEM);
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

 private:
  bool rle() const { return compression_method_ > 0; }
  int16_t kerning(char32_t left, char32_t right) const;

  friend class GlyphMetadataReader;
  friend class GlyphPairIterator;

  // Binary-search for the glyph data.
  const roo::byte *PROGMEM findGlyph(char32_t code) const;
  const roo::byte *PROGMEM findKernPair(char32_t left, char32_t right) const;

  // Helper methods for drawing glyphs.
  void drawGlyphModeVisible(DisplayOutput &output, int16_t x, int16_t y,
                            const GlyphMetrics &metrics,
                            const roo::byte *PROGMEM data,
                            const Box &clip_box, Color color, Color bgcolor,
                            BlendingMode blending_mode) const;

  void drawBordered(DisplayOutput &output, int16_t x, int16_t y,
                    int16_t bgwidth, const Drawable &glyph, const Box &clip_box,
                    Color bgColor, BlendingMode blending_mode) const;

  void drawGlyphModeFill(DisplayOutput &output, int16_t x, int16_t y,
                         int16_t bgwidth, const GlyphMetrics &metrics,
                         const roo::byte *PROGMEM data, int16_t offset,
                         const Box &clip_box, Color color, Color bgColor,
                         BlendingMode blending_mode) const;

  void drawKernedGlyphsModeFill(
      DisplayOutput &output, int16_t x, int16_t y, int16_t bgwidth,
      const GlyphMetrics &left_metrics, const roo::byte *PROGMEM left_data,
      int16_t left_offset, const GlyphMetrics &right_metrics,
      const roo::byte *PROGMEM right_data, int16_t right_offset,
      const Box &clip_box, Color color, Color bgColor,
      BlendingMode blending_mode) const;

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
  const roo::byte *glyph_metadata_begin_ PROGMEM;
  const roo::byte *glyph_kerning_begin_ PROGMEM;
  const roo::byte *glyph_data_begin_ PROGMEM;
};

}  // namespace roo_display
