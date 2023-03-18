#include "roo_display/font/smooth_font.h"

#include <HardwareSerial.h>
#include <WString.h>  // Pretty much for debug output only.

#include "roo_display/core/raster.h"
#include "roo_display/image/image.h"
#include "roo_display/internal/raw_streamable_overlay.h"
#include "roo_display/io/memory.h"

namespace roo_display {

void fatalError(const String &string);

class FontMetricReader {
 public:
  FontMetricReader(internal::ProgMemPtrStream *reader, int font_metric_bytes)
      : reader_(reader), font_metric_bytes_(font_metric_bytes) {}

  int read() {
    return font_metric_bytes_ == 1 ? (int8_t)reader_->read()
                                   : (int16_t)read_uint16_be(reader_);
  }

 private:
  internal::ProgMemPtrStream *reader_;
  int font_metric_bytes_;
};

static int8_t readByte(const uint8_t *PROGMEM ptr) {
  return pgm_read_byte(ptr);
}

static int16_t readWord(const uint8_t *PROGMEM ptr) {
  return (pgm_read_byte(ptr) << 8) | pgm_read_byte(ptr + 1);
}

class GlyphMetadataReader {
 public:
  GlyphMetadataReader(const SmoothFont &font, const uint8_t *PROGMEM ptr)
      : font_(font), ptr_(ptr + font.encoding_bytes_) {}

  GlyphMetrics readMetrics(FontLayout layout) {
    int16_t glyphXMin, glyphYMin, glyphXMax, glyphYMax, x_advance;
    if (font_.font_metric_bytes_ == 1) {
      glyphXMin = readByte(ptr_ + 0);
      glyphYMin = readByte(ptr_ + 1);
      glyphXMax = readByte(ptr_ + 2);
      glyphYMax = readByte(ptr_ + 3);
      x_advance = readByte(ptr_ + 4);
    } else {
      glyphXMin = readWord(ptr_ + 0);
      glyphYMin = readWord(ptr_ + 2);
      glyphXMax = readWord(ptr_ + 4);
      glyphYMax = readWord(ptr_ + 6);
      x_advance = readWord(ptr_ + 8);
    }
    return GlyphMetrics(
        glyphXMin, glyphYMin, glyphXMax, glyphYMax,
        layout == FONT_LAYOUT_HORIZONTAL
            ? x_advance
            : glyphYMax - glyphYMin + 1 + font_.metrics().linegap());
  }

  long data_offset() const {
    int offset_bytes = font_.offset_bytes_;
    const uint8_t *PROGMEM ptr = ptr_ + 5 * font_.font_metric_bytes_;
    return offset_bytes == 2
               ? (pgm_read_byte(ptr) << 8) | pgm_read_byte(ptr + 1)
           : offset_bytes == 3
               ? (pgm_read_byte(ptr) << 16) | (pgm_read_byte(ptr + 1) << 8) |
                     pgm_read_byte(ptr + 2)
               : pgm_read_byte(ptr);
  }

 private:
  const SmoothFont &font_;
  const uint8_t *PROGMEM ptr_;
};

SmoothFont::SmoothFont(const uint8_t *font_data PROGMEM)
    : glyph_count_(0), default_glyph_(0), default_space_width_(0) {
  internal::ProgMemPtrStream reader(font_data);
  uint16_t version = read_uint16_be(&reader);

  if (version != 0x0101) {
    fatalError(String("Unsupported version number: ") + version);
    return;
  }

  alpha_bits_ = reader.read();
  if (alpha_bits_ != 4) {
    fatalError(String("Unsupported alpha encoding: ") + alpha_bits_);
    return;
  }

  encoding_bytes_ = reader.read();
  if (encoding_bytes_ > 2) {
    fatalError(String("Unsupported character encoding: ") + encoding_bytes_);
    return;
  }

  font_metric_bytes_ = reader.read();
  if (font_metric_bytes_ > 2) {
    fatalError(String("Unsupported count of metric bytes: ") + encoding_bytes_);
    return;
  }

  offset_bytes_ = reader.read();
  if (offset_bytes_ > 3) {
    fatalError(String("Unsupported count of offset bytes: ") + offset_bytes_);
    return;
  }

  compression_method_ = reader.read();
  if (compression_method_ > 1) {
    fatalError(String("Unsupported compression method: ") +
               compression_method_);
    return;
  }

  glyph_count_ = read_uint16_be(&reader);
  kerning_pairs_count_ = read_uint16_be(&reader);

  FontMetricReader fm_reader(&reader, font_metric_bytes_);
  int xMin = fm_reader.read();
  int yMin = fm_reader.read();
  int xMax = fm_reader.read();
  int yMax = fm_reader.read();
  int ascent = fm_reader.read();
  int descent = fm_reader.read();
  int linegap = fm_reader.read();
  int min_advance = fm_reader.read();
  int max_advance = fm_reader.read();
  int max_right_overhang = fm_reader.read();
  default_space_width_ = fm_reader.read();

  if (encoding_bytes_ == 1) {
    default_glyph_ = reader.read();
  } else {
    default_glyph_ = read_uint16_be(&reader);
  }

  glyph_metadata_begin_ = reader.ptr();
  glyph_metadata_size_ =
      (5 * font_metric_bytes_) + offset_bytes_ + encoding_bytes_;
  glyph_kerning_size_ = 2 * encoding_bytes_ + 1;
  glyph_kerning_begin_ =
      glyph_metadata_begin_ + glyph_metadata_size_ * glyph_count_;
  glyph_data_begin_ =
      glyph_kerning_begin_ + glyph_kerning_size_ * kerning_pairs_count_;

  Font::init(
      FontMetrics(ascent, descent, linegap, xMin, yMin, xMax, yMax,
                  max_right_overhang),
      FontProperties(encoding_bytes_ > 1 ? FontProperties::CHARSET_UNICODE_BMP
                                         : FontProperties::CHARSET_ASCII,
                     min_advance == max_advance && kerning_pairs_count_ == 0
                         ? FontProperties::SPACING_MONOSPACE
                         : FontProperties::SPACING_PROPORTIONAL,
                     alpha_bits_ > 1 ? FontProperties::SMOOTHING_GRAYSCALE
                                     : FontProperties::SMOOTHING_NONE,
                     kerning_pairs_count_ > 0 ? FontProperties::KERNING_PAIRS
                                              : FontProperties::KERNING_NONE));

  // Serial.println(String() + "Loaded font with " + glyph_count_ +
  //                " glyphs, size " + (ascent - descent));
}

bool is_space(unicode_t code) {
  // http://en.cppreference.com/w/cpp/string/wide/iswspace; see POSIX
  return code == 0x0020 || code == 0x00A0 ||
         (code >= 0x0009 && code <= 0x000D) || code == 0x1680 ||
         code == 0x180E || (code >= 0x2000 && code <= 0x200B) ||
         code == 0x2028 || code == 0x2029 || code == 0x205F || code == 0x3000 ||
         code == 0xFEFF;
}

void SmoothFont::drawGlyphModeVisible(DisplayOutput &output, int16_t x,
                                      int16_t y, const GlyphMetrics &metrics,
                                      const uint8_t *PROGMEM data,
                                      const Box &clip_box, Color color,
                                      Color bgcolor,
                                      PaintMode paint_mode) const {
  Surface s(output, x + metrics.bearingX(), y - metrics.bearingY(), clip_box,
            false, bgcolor, FILL_MODE_VISIBLE, paint_mode);
  if (rle()) {
    RleImage4bppxBiased<Alpha4> glyph(metrics.width(), metrics.height(), data,
                                      color);
    streamToSurface(s, std::move(glyph));
  } else {
    // Identical as above, but using Raster<> instead of MonoAlpha4RleImage.
    Raster<const uint8_t PROGMEM *, Alpha4> glyph(
        metrics.width(), metrics.height(), data, color);
    streamToSurface(s, std::move(glyph));
  }
}

void SmoothFont::drawBordered(DisplayOutput &output, int16_t x, int16_t y,
                              int16_t bgwidth, const Drawable &glyph,
                              const Box &clip_box, Color bgColor,
                              PaintMode paint_mode) const {
  Box outer(x, y - metrics().glyphYMax(), x + bgwidth - 1,
            y - metrics().glyphYMin());
  if (bgColor.a() == 0xFF) paint_mode = PAINT_MODE_REPLACE;
  if (outer.clip(clip_box) == Box::CLIP_RESULT_EMPTY) return;
  Box inner = glyph.extents().translate(x, y);
  if (inner.clip(clip_box) == Box::CLIP_RESULT_EMPTY) {
    output.fillRect(paint_mode, outer, bgColor);
    return;
  }
  if (outer.yMin() < inner.yMin()) {
    output.fillRect(
        paint_mode,
        Box(outer.xMin(), outer.yMin(), outer.xMax(), inner.yMin() - 1),
        bgColor);
  }
  if (outer.xMin() < inner.xMin()) {
    output.fillRect(
        paint_mode,
        Box(outer.xMin(), inner.yMin(), inner.xMin() - 1, inner.yMax()),
        bgColor);
  }
  Surface s(output, x, y, clip_box, false, bgColor, FILL_MODE_RECTANGLE,
            paint_mode);
  s.drawObject(glyph);
  if (outer.xMax() > inner.xMax()) {
    output.fillRect(
        paint_mode,
        Box(inner.xMax() + 1, inner.yMin(), outer.xMax(), inner.yMax()),
        bgColor);
  }
  if (outer.yMax() > inner.yMax()) {
    output.fillRect(
        paint_mode,
        Box(outer.xMin(), inner.yMax() + 1, outer.xMax(), outer.yMax()),
        bgColor);
  }
}

void SmoothFont::drawGlyphModeFill(DisplayOutput &output, int16_t x, int16_t y,
                                   int16_t bgwidth,
                                   const GlyphMetrics &glyph_metrics,
                                   const uint8_t *PROGMEM data, int16_t offset,
                                   const Box &clip_box, Color color,
                                   Color bgColor, PaintMode paint_mode) const {
  Box box = glyph_metrics.screen_extents().translate(offset, 0);
  if (rle()) {
    auto glyph = MakeDrawableRawStreamable(
        RleImage4bppxBiased<Alpha4>(box, data, color));
    drawBordered(output, x, y, bgwidth, glyph, clip_box, bgColor, paint_mode);
  } else {
    // Identical as above, but using Raster<>
    auto glyph = MakeDrawableRawStreamable(
        Raster<const uint8_t PROGMEM *, Alpha4>(box, data, color));
    drawBordered(output, x, y, bgwidth, glyph, clip_box, bgColor, paint_mode);
  }
}

void SmoothFont::drawKernedGlyphsModeFill(
    DisplayOutput &output, int16_t x, int16_t y, int16_t bgwidth,
    const GlyphMetrics &left_metrics, const uint8_t *PROGMEM left_data,
    int16_t left_offset, const GlyphMetrics &right_metrics,
    const uint8_t *PROGMEM right_data, int16_t right_offset,
    const Box &clip_box, Color color, Color bgColor,
    PaintMode paint_mode) const {
  Box lb = left_metrics.screen_extents().translate(left_offset, 0);
  Box rb = right_metrics.screen_extents().translate(right_offset, 0);
  if (rle()) {
    auto glyph = MakeDrawableRawStreamable(
        Overlay(RleImage4bppxBiased<Alpha4>(lb, left_data, color), 0, 0,
                RleImage4bppxBiased<Alpha4>(rb, right_data, color), 0, 0));
    drawBordered(output, x, y, bgwidth, glyph, clip_box, bgColor, paint_mode);
  } else {
    // Identical as above, but using Raster<>
    auto glyph = MakeDrawableRawStreamable(Overlay(
        Raster<const uint8_t PROGMEM *, Alpha4>(lb, left_data, color), 0, 0,
        Raster<const uint8_t PROGMEM *, Alpha4>(rb, right_data, color), 0, 0));
    drawBordered(output, x, y, bgwidth, glyph, clip_box, bgColor, paint_mode);
  }
}

class GlyphPairIterator {
 public:
  GlyphPairIterator(const SmoothFont *font) : font_(font), swapped_(false) {}

  const GlyphMetrics &left_metrics() const { return swapped_ ? m2_ : m1_; }

  const GlyphMetrics &right_metrics() const { return swapped_ ? m1_ : m2_; }

  const uint8_t *PROGMEM left_data() const {
    return font_->glyph_data_begin_ +
           (swapped_ ? data_offset_2_ : data_offset_1_);
  }

  const uint8_t *PROGMEM right_data() const {
    return font_->glyph_data_begin_ +
           (swapped_ ? data_offset_1_ : data_offset_2_);
  }

  void push(unicode_t code) {
    swapped_ = !swapped_;
    if (is_space(code)) {
      *mutable_right_metrics() =
          GlyphMetrics(0, 0, -1, -1, font_->default_space_width_);
      *mutable_right_data_offset() = 0;
      return;
    }
    const uint8_t *PROGMEM glyph = font_->findGlyph(code);
    if (glyph == nullptr) {
      glyph = font_->findGlyph(font_->default_glyph_);
    }
    if (glyph == nullptr) {
      *mutable_right_metrics() =
          GlyphMetrics(0, 0, -1, -1, font_->default_space_width_);
      *mutable_right_data_offset() = 0;
    } else {
      class GlyphMetadataReader glyph_meta(*font_, glyph);
      *mutable_right_metrics() = glyph_meta.readMetrics(FONT_LAYOUT_HORIZONTAL);
      *mutable_right_data_offset() = glyph_meta.data_offset();
    }
  }

  void pushNull() { swapped_ = !swapped_; }

 private:
  GlyphMetrics *mutable_right_metrics() { return swapped_ ? &m1_ : &m2_; }

  long *mutable_right_data_offset() {
    return swapped_ ? &data_offset_1_ : &data_offset_2_;
  }

  const SmoothFont *font_;
  bool swapped_;
  GlyphMetrics m1_;
  GlyphMetrics m2_;
  long data_offset_1_;
  long data_offset_2_;
};

GlyphMetrics SmoothFont::getHorizontalStringMetrics(const uint8_t *utf8_data,
                                                    uint32_t size) const {
  Utf8LookAheadDecoder decoder(utf8_data, size);
  if (!decoder.has_next()) {
    // Nothing to draw.
    return GlyphMetrics(0, 0, -1, -1, 0);
  }
  unicode_t next_code = decoder.next();
  GlyphPairIterator glyphs(this);
  glyphs.push(next_code);
  bool has_more;
  int16_t advance = 0;
  int16_t yMin = 32767;
  int16_t yMax = -32768;
  int16_t xMin = glyphs.right_metrics().lsb();
  do {
    unicode_t code = next_code;
    has_more = decoder.has_next();
    int16_t kern;
    if (has_more) {
      next_code = decoder.next();
      glyphs.push(next_code);
      kern = kerning(code, next_code);
    } else {
      next_code = 0;
      glyphs.pushNull();
      kern = 0;
    }
    advance += (glyphs.left_metrics().advance() - kern);
    const GlyphMetrics &metrics = glyphs.left_metrics();
    if (yMax < metrics.glyphYMax()) {
      yMax = metrics.glyphYMax();
    }
    if (yMin > metrics.glyphYMin()) {
      yMin = metrics.glyphYMin();
    }
  } while (has_more);
  return GlyphMetrics(xMin, yMin, advance - glyphs.left_metrics().rsb() - 1,
                      yMax, advance);
}

uint32_t SmoothFont::getHorizontalStringGlyphMetrics(const uint8_t *utf8_data,
                                                     uint32_t size,
                                                     GlyphMetrics *result,
                                                     uint32_t offset,
                                                     uint32_t max_count) const {
  Utf8LookAheadDecoder decoder(utf8_data, size);
  if (!decoder.has_next()) {
    // Nothing to measure.
    return 0;
  }
  uint32_t glyph_idx = 0;
  uint32_t glyph_count = 0;
  unicode_t next_code = decoder.next();
  GlyphPairIterator glyphs(this);
  glyphs.push(next_code);
  bool has_more;
  int16_t advance = 0;
  do {
    if (glyph_count >= max_count) return glyph_count;
    unicode_t code = next_code;
    has_more = decoder.has_next();
    int16_t kern;
    if (has_more) {
      next_code = decoder.next();
      glyphs.push(next_code);
      kern = kerning(code, next_code);
    } else {
      next_code = 0;
      glyphs.pushNull();
      kern = 0;
    }
    if (glyph_idx >= offset) {
      result[glyph_count++] =
          GlyphMetrics(glyphs.left_metrics().glyphXMin() + advance,
                       glyphs.left_metrics().glyphYMin(),
                       glyphs.left_metrics().glyphXMax() + advance,
                       glyphs.left_metrics().glyphYMax(),
                       glyphs.left_metrics().advance() + advance);
    }
    ++glyph_idx;
    advance += (glyphs.left_metrics().advance() - kern);
  } while (has_more);
  return glyph_count;
}

void SmoothFont::drawHorizontalString(const Surface &s,
                                      const uint8_t *utf8_data, uint32_t size,
                                      Color color) const {
  Utf8LookAheadDecoder decoder(utf8_data, size);
  if (!decoder.has_next()) {
    // Nothing to draw.
    return;
  }
  unicode_t next_code = decoder.next();
  int16_t x = s.dx();
  int16_t y = s.dy();
  DisplayOutput &output = s.out();

  GlyphPairIterator glyphs(this);
  glyphs.push(next_code);
  int16_t preadvanced = 0;
  if (glyphs.right_metrics().lsb() < 0) {
    preadvanced = glyphs.right_metrics().lsb();
    x += preadvanced;
  }
  bool has_more;
  do {
    unicode_t code = next_code;
    has_more = decoder.has_next();
    int16_t kern;
    if (has_more) {
      next_code = decoder.next();
      glyphs.push(next_code);
      kern = kerning(code, next_code);
    } else {
      next_code = 0;
      glyphs.pushNull();
      kern = 0;
    }
    if (s.fill_mode() == FILL_MODE_VISIBLE) {
      // No fill; simply draw and shift.
      drawGlyphModeVisible(output, x - preadvanced, y, glyphs.left_metrics(),
                           glyphs.left_data(), s.clip_box(), color, s.bgcolor(),
                           s.paint_mode());
      x += (glyphs.left_metrics().advance() - kern);
    } else {
      // General case. We may have two glyphs to worry about, and we may be
      // pre-advanced. Let's determine our bounding box, taking the
      // pre-advancement and kerning into account.
      int16_t advance = glyphs.left_metrics().advance() - kern;
      int16_t gap = 0;
      if (has_more) {
        gap = glyphs.left_metrics().rsb() + glyphs.right_metrics().lsb() - kern;
      }
      // Calculate the total width of a rectangle that we will need to fill with
      // content (glyphs + background).
      int16_t total_rect_width =
          glyphs.left_metrics().glyphXMax() + 1 - preadvanced;
      if (gap < 0) {
        // Glyphs overlap; need to use the overlay method.
        drawKernedGlyphsModeFill(
            output, x, y, total_rect_width, glyphs.left_metrics(),
            glyphs.left_data(), -preadvanced, glyphs.right_metrics(),
            glyphs.right_data(), advance - preadvanced,
            Box::intersect(s.clip_box(), Box(x, y - metrics().glyphYMax(),
                                             x + total_rect_width - 1,
                                             y - metrics().glyphYMin())),
            color, s.bgcolor(), s.paint_mode());
      } else {
        // Glyphs do not overlap; can draw them one by one.
        if (has_more) {
          // Include the interim whitespace gap in the rectangle that we'll fill
          // (the gap will be filled with background). This way, we draw longer
          // horizontal strikes which translates to fewer SPI operations.
          total_rect_width += gap;
        } else if (glyphs.left_metrics().rsb() > 0) {
          // After the last character, fill up the right-side bearing.
          total_rect_width += glyphs.left_metrics().rsb();
        }
        drawGlyphModeFill(
            output, x, y, total_rect_width, glyphs.left_metrics(),
            glyphs.left_data(), -preadvanced,
            Box::intersect(s.clip_box(), Box(x, y - metrics().glyphYMax(),
                                             x + total_rect_width - 1,
                                             y - metrics().glyphYMin())),
            color, s.bgcolor(), s.paint_mode());
      }
      x += total_rect_width;
      preadvanced = total_rect_width - (advance - preadvanced);
    }
  } while (has_more);
}

bool SmoothFont::getGlyphMetrics(unicode_t code, FontLayout layout,
                                 GlyphMetrics *result) const {
  const uint8_t *PROGMEM glyph = findGlyph(code);
  if (glyph == nullptr) return false;
  GlyphMetadataReader reader(*this, glyph);
  *result = reader.readMetrics(layout);
  return true;
}

template <int encoding_bytes>
unicode_t read_unicode(const uint8_t *PROGMEM address);

template <>
unicode_t read_unicode<1>(const uint8_t *PROGMEM address) {
  return pgm_read_byte(address);
}

template <>
unicode_t read_unicode<2>(const uint8_t *PROGMEM address) {
  return ((unicode_t)pgm_read_byte(address) << 8) | pgm_read_byte(address + 1);
}

template <int encoding_bytes>
const uint8_t *PROGMEM indexSearch(unicode_t c, const uint8_t *PROGMEM data,
                                   int glyph_size, int start, int stop) {
  int pivot = (start + stop) / 2;
  const uint8_t *PROGMEM pivot_ptr = data + (pivot * glyph_size);
  uint16_t pivot_value = read_unicode<encoding_bytes>(pivot_ptr);
  if (c == pivot_value) return pivot_ptr;
  if (start >= stop) return nullptr;
  if (c < pivot_value)
    return indexSearch<encoding_bytes>(c, data, glyph_size, start, pivot - 1);
  return indexSearch<encoding_bytes>(c, data, glyph_size, pivot + 1, stop);
}

const uint8_t *PROGMEM SmoothFont::findGlyph(unicode_t code) const {
  switch (encoding_bytes_) {
    case 1:
      return indexSearch<1>(code, glyph_metadata_begin_, glyph_metadata_size_,
                            0, glyph_count_);
    case 2:
      return indexSearch<2>(code, glyph_metadata_begin_, glyph_metadata_size_,
                            0, glyph_count_);
    default:
      return nullptr;
  }
}

template <int encoding_bytes>
const uint8_t *PROGMEM kernIndexSearch(uint32_t lookup,
                                       const uint8_t *PROGMEM data,
                                       int kern_size, int start, int stop) {
  int pivot = (start + stop) / 2;
  const uint8_t *PROGMEM pivot_ptr = data + (pivot * kern_size);
  uint16_t left = read_unicode<encoding_bytes>(pivot_ptr);
  uint16_t right = read_unicode<encoding_bytes>(pivot_ptr + encoding_bytes);
  uint32_t pivot_value = left << 16 | right;
  if (lookup == pivot_value) {
    return pivot_ptr;
  }
  if (start >= stop) {
    return nullptr;
  }
  if (lookup < pivot_value)
    return kernIndexSearch<encoding_bytes>(lookup, data, kern_size, start,
                                           pivot - 1);
  return kernIndexSearch<encoding_bytes>(lookup, data, kern_size, pivot + 1,
                                         stop);
}

const uint8_t *PROGMEM SmoothFont::findKernPair(unicode_t left,
                                                unicode_t right) const {
  uint32_t lookup = left << 16 | right;
  switch (encoding_bytes_) {
    case 1:
      return kernIndexSearch<1>(lookup, glyph_kerning_begin_,
                                glyph_kerning_size_, 0, kerning_pairs_count_);
    case 2:
      return kernIndexSearch<2>(lookup, glyph_kerning_begin_,
                                glyph_kerning_size_, 0, kerning_pairs_count_);
    default:
      return nullptr;
  }
}

int16_t SmoothFont::kerning(unicode_t left, unicode_t right) const {
  const uint8_t *PROGMEM kern = findKernPair(left, right);
  if (kern == 0) {
    return 0;
  } else {
    return pgm_read_byte(kern + 2 * encoding_bytes_);
  }
}

void fatalError(const String &string) {
  Serial.println();
  Serial.print(
      "--------------- FATAL ERROR in smooth_font.cpp ------------------");
  Serial.println(string);
}

}  // namespace roo_display