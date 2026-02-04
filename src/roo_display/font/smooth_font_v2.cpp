#include "roo_display/font/smooth_font_v2.h"

#include "roo_display/core/raster.h"
#include "roo_display/image/image.h"
#include "roo_display/internal/raw_streamable_overlay.h"
#include "roo_display/io/memory.h"
#include "roo_io/core/input_iterator.h"
#include "roo_io/text/unicode.h"
#include "roo_logging.h"

namespace roo_display {

class FontMetricReader {
 public:
  FontMetricReader(
      roo_io::UnsafeGenericMemoryIterator<const roo::byte PROGMEM*>& reader,
      int font_metric_bytes)
      : reader_(reader), font_metric_bytes_(font_metric_bytes) {}

  int read() {
    return font_metric_bytes_ == 1 ? roo_io::ReadS8(reader_)
                                   : roo_io::ReadBeS16(reader_);
  }

 private:
  roo_io::UnsafeGenericMemoryIterator<const roo::byte PROGMEM*>& reader_;
  int font_metric_bytes_;
};

static int8_t readByte(const roo::byte* PROGMEM ptr) {
  return pgm_read_byte(ptr);
}

static int16_t readWord(const roo::byte* PROGMEM ptr) {
  return (pgm_read_byte(ptr) << 8) | pgm_read_byte(ptr + 1);
}

static uint16_t readUWord(const roo::byte* PROGMEM ptr) {
  return ((uint16_t)pgm_read_byte(ptr) << 8) | pgm_read_byte(ptr + 1);
}

static uint32_t readTriWord(const roo::byte* PROGMEM ptr) {
  return (uint32_t)pgm_read_byte(ptr) << 16 |
         (uint32_t)pgm_read_byte(ptr + 1) << 8 |
         (uint32_t)pgm_read_byte(ptr + 2);
}

class SmoothFontV2::GlyphMetadataReader {
 public:
  GlyphMetadataReader(const SmoothFontV2& font, int glyph_index)
      : font_(font),
        ptr_(font.glyph_metadata_begin_ +
             glyph_index * font.glyph_metadata_size_) {}

  GlyphMetrics readMetrics(FontLayout layout, bool& compressed) {
    int16_t glyphXMin, glyphYMin, glyphXMax, glyphYMax, x_advance;
    if (font_.font_metric_bytes_ == 1) {
      uint8_t b = readByte(ptr_ + 0);
      compressed = ((b >> 7) == ((b >> 6) & 1));
      glyphXMin = (int8_t)(b ^ ((!compressed) << 6));
      glyphYMin = readByte(ptr_ + 1);
      glyphXMax = readByte(ptr_ + 2);
      glyphYMax = readByte(ptr_ + 3);
      x_advance = readByte(ptr_ + 4);
    } else {
      uint16_t b = readWord(ptr_ + 0);
      compressed = ((b >> 15) == ((b >> 14) & 1));
      glyphXMin = (int16_t)(b ^ ((!compressed) << 14));
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
    const roo::byte* PROGMEM ptr = ptr_ + 5 * font_.font_metric_bytes_;
    return offset_bytes == 2
               ? (pgm_read_byte(ptr) << 8) | pgm_read_byte(ptr + 1)
           : offset_bytes == 3
               ? (pgm_read_byte(ptr) << 16) | (pgm_read_byte(ptr + 1) << 8) |
                     pgm_read_byte(ptr + 2)
               : pgm_read_byte(ptr);
  }

 private:
  const SmoothFontV2& font_;
  const roo::byte* PROGMEM ptr_;
};

SmoothFontV2::SmoothFontV2(const roo::byte* font_data PROGMEM)
    : glyph_count_(0), default_glyph_(0), default_space_width_(0) {
  roo_io::UnsafeGenericMemoryIterator<const roo::byte PROGMEM*> reader(
      font_data);
  uint16_t version = roo_io::ReadBeU16(reader);
  CHECK(version == 0x0200) << "Unsupported version number: " << version;

  alpha_bits_ = roo_io::ReadU8(reader);
  CHECK_EQ(alpha_bits_, 4) << "Unsupported alpha encoding: " << alpha_bits_;
  encoding_bytes_ = roo_io::ReadU8(reader);
  CHECK_LE(encoding_bytes_, 2)
      << "Unsupported character encoding: " << encoding_bytes_;
  font_metric_bytes_ = roo_io::ReadU8(reader);
  CHECK_LE(font_metric_bytes_, 2)
      << "Unsupported count of metric bytes: " << encoding_bytes_;

  offset_bytes_ = roo_io::ReadU8(reader);
  CHECK_LE(offset_bytes_, 3)
      << "Unsupported count of offset bytes: " << offset_bytes_;

  compression_method_ = roo_io::ReadU8(reader);
  CHECK_LE(compression_method_, 1)
      << "Unsupported compression method: " << compression_method_;

  glyph_count_ = roo_io::ReadBeU16(reader);
  kerning_pairs_count_ = roo_io::ReadBeU16(reader);
  cmap_entries_count_ = roo_io::ReadBeU16(reader);

  FontMetricReader fm_reader(reader, font_metric_bytes_);
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
    default_glyph_ = roo_io::ReadU8(reader);
  } else {
    default_glyph_ = roo_io::ReadBeU16(reader);
  }

  font_begin_ = font_data;
  cmap_entries_begin_ = reader.ptr();
  const int kCmapEntrySize = 12;
  const roo::byte* PROGMEM cmap_entries_end =
      cmap_entries_begin_ + cmap_entries_count_ * kCmapEntrySize;
  uint32_t max_indirection_end = (uint32_t)(cmap_entries_end - font_begin_);
  for (int i = 0; i < cmap_entries_count_; ++i) {
    const roo::byte* PROGMEM entry = cmap_entries_begin_ + i * kCmapEntrySize;
    uint16_t data_entries_count = readUWord(entry + 6);
    uint32_t data_offset = readTriWord(entry + 8);
    uint8_t format = readByte(entry + 11);
    CHECK_LT(data_offset, (1u << 24)) << "Cmap data offset out of range";
    if (format == 1 && data_entries_count != 0) {
      uint32_t end = data_offset + data_entries_count * 2;
      if (end > max_indirection_end) max_indirection_end = end;
    }
  }

  glyph_metadata_begin_ = font_begin_ + max_indirection_end;
  glyph_metadata_size_ = (5 * font_metric_bytes_) + offset_bytes_;
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

bool is_space(char32_t code) {
  // http://en.cppreference.com/w/cpp/string/wide/iswspace; see POSIX
  return code == 0x0020 || code == 0x00A0 ||
         (code >= 0x0009 && code <= 0x000D) || code == 0x1680 ||
         code == 0x180E || (code >= 0x2000 && code <= 0x200B) ||
         code == 0x2028 || code == 0x2029 || code == 0x205F || code == 0x3000 ||
         code == 0xFEFF;
}

void SmoothFontV2::drawGlyphModeVisible(
    DisplayOutput& output, int16_t x, int16_t y, const GlyphMetrics& metrics,
    bool compressed, const roo::byte* PROGMEM data, const Box& clip_box,
    Color color, Color bgcolor, BlendingMode blending_mode) const {
  Surface s(output, x + metrics.bearingX(), y - metrics.bearingY(), clip_box,
            false, bgcolor, FILL_MODE_VISIBLE, blending_mode);
  if (rle() && compressed) {
    RleImage4bppxBiased<Alpha4> glyph(metrics.width(), metrics.height(), data,
                                      color);
    streamToSurface(s, std::move(glyph));
  } else {
    // Identical as above, but using Raster<> instead of MonoAlpha4RleImage.
    ProgMemRaster<Alpha4> glyph(metrics.width(), metrics.height(), data, color);
    streamToSurface(s, std::move(glyph));
  }
}

void SmoothFontV2::drawBordered(DisplayOutput& output, int16_t x, int16_t y,
                                int16_t bgwidth, const Drawable& glyph,
                                const Box& clip_box, Color bgColor,
                                BlendingMode blending_mode) const {
  Box outer(x, y - metrics().glyphYMax(), x + bgwidth - 1,
            y - metrics().glyphYMin());

  // NOTE: bgColor is part of the source, not destination.
  if (bgColor.a() == 0xFF &&
      (blending_mode == BLENDING_MODE_SOURCE_OVER ||
       blending_mode == BLENDING_MODE_SOURCE_OVER_OPAQUE)) {
    // All souce pixels will be fully opaque.
    blending_mode = BLENDING_MODE_SOURCE;
  }

  if (outer.clip(clip_box) == Box::CLIP_RESULT_EMPTY) return;
  Box inner = glyph.extents().translate(x, y);
  if (inner.clip(clip_box) == Box::CLIP_RESULT_EMPTY) {
    output.fillRect(blending_mode, outer, bgColor);
    return;
  }
  if (outer.yMin() < inner.yMin()) {
    output.fillRect(
        blending_mode,
        Box(outer.xMin(), outer.yMin(), outer.xMax(), inner.yMin() - 1),
        bgColor);
  }
  if (outer.xMin() < inner.xMin()) {
    output.fillRect(
        blending_mode,
        Box(outer.xMin(), inner.yMin(), inner.xMin() - 1, inner.yMax()),
        bgColor);
  }
  Surface s(output, x, y, clip_box, false, bgColor, FILL_MODE_RECTANGLE,
            blending_mode);
  s.drawObject(glyph);
  if (outer.xMax() > inner.xMax()) {
    output.fillRect(
        blending_mode,
        Box(inner.xMax() + 1, inner.yMin(), outer.xMax(), inner.yMax()),
        bgColor);
  }
  if (outer.yMax() > inner.yMax()) {
    output.fillRect(
        blending_mode,
        Box(outer.xMin(), inner.yMax() + 1, outer.xMax(), outer.yMax()),
        bgColor);
  }
}

void SmoothFontV2::drawGlyphModeFill(
    DisplayOutput& output, int16_t x, int16_t y, int16_t bgwidth,
    const GlyphMetrics& glyph_metrics, bool compressed,
    const roo::byte* PROGMEM data, int16_t offset, const Box& clip_box,
    Color color, Color bgColor, BlendingMode blending_mode) const {
  Box box = glyph_metrics.screen_extents().translate(offset, 0);
  if (rle() && compressed) {
    auto glyph = MakeDrawableRawStreamable(
        RleImage4bppxBiased<Alpha4>(box, data, color));
    drawBordered(output, x, y, bgwidth, glyph, clip_box, bgColor,
                 blending_mode);
  } else {
    // Identical as above, but using Raster<>
    auto glyph =
        MakeDrawableRawStreamable(ProgMemRaster<Alpha4>(box, data, color));
    drawBordered(output, x, y, bgwidth, glyph, clip_box, bgColor,
                 blending_mode);
  }
}

void SmoothFontV2::drawKernedGlyphsModeFill(
    DisplayOutput& output, int16_t x, int16_t y, int16_t bgwidth,
    const GlyphMetrics& left_metrics, bool left_compressed,
    const roo::byte* PROGMEM left_data, int16_t left_offset,
    const GlyphMetrics& right_metrics, bool right_compressed,
    const roo::byte* PROGMEM right_data, int16_t right_offset,
    const Box& clip_box, Color color, Color bgColor,
    BlendingMode blending_mode) const {
  Box lb = left_metrics.screen_extents().translate(left_offset, 0);
  Box rb = right_metrics.screen_extents().translate(right_offset, 0);
  if (rle() && left_compressed && right_compressed) {
    auto glyph = MakeDrawableRawStreamable(
        Overlay(RleImage4bppxBiased<Alpha4>(lb, left_data, color), 0, 0,
                RleImage4bppxBiased<Alpha4>(rb, right_data, color), 0, 0));
    drawBordered(output, x, y, bgwidth, glyph, clip_box, bgColor,
                 blending_mode);
  } else if (rle() && left_compressed) {
    auto glyph = MakeDrawableRawStreamable(
        Overlay(RleImage4bppxBiased<Alpha4>(lb, left_data, color), 0, 0,
                ProgMemRaster<Alpha4>(rb, right_data, color), 0, 0));
    drawBordered(output, x, y, bgwidth, glyph, clip_box, bgColor,
                 blending_mode);
  } else if (rle() && right_compressed) {
    auto glyph = MakeDrawableRawStreamable(
        Overlay(ProgMemRaster<Alpha4>(lb, left_data, color), 0, 0,
                RleImage4bppxBiased<Alpha4>(rb, right_data, color), 0, 0));
    drawBordered(output, x, y, bgwidth, glyph, clip_box, bgColor,
                 blending_mode);
  } else {
    auto glyph = MakeDrawableRawStreamable(
        Overlay(ProgMemRaster<Alpha4>(lb, left_data, color), 0, 0,
                ProgMemRaster<Alpha4>(rb, right_data, color), 0, 0));
    drawBordered(output, x, y, bgwidth, glyph, clip_box, bgColor,
                 blending_mode);
  }
}

class SmoothFontV2::GlyphPairIterator {
 public:
  GlyphPairIterator(const SmoothFontV2* font) : font_(font), swapped_(false) {}

  const GlyphMetrics& left_metrics() const { return swapped_ ? m2_ : m1_; }

  const GlyphMetrics& right_metrics() const { return swapped_ ? m1_ : m2_; }

  const roo::byte* PROGMEM left_data() const {
    return font_->glyph_data_begin_ +
           (swapped_ ? data_offset_2_ : data_offset_1_);
  }

  const roo::byte* PROGMEM right_data() const {
    return font_->glyph_data_begin_ +
           (swapped_ ? data_offset_1_ : data_offset_2_);
  }

  bool left_compressed() const {
    return swapped_ ? compressed_2_ : compressed_1_;
  }
  bool right_compressed() const {
    return swapped_ ? compressed_1_ : compressed_2_;
  }

  void push(char32_t code) {
    swapped_ = !swapped_;
    if (is_space(code)) {
      *mutable_right_metrics() =
          GlyphMetrics(0, 0, -1, -1, font_->default_space_width_);
      *mutable_right_data_offset() = 0;
      return;
    }
    int glyph_index = font_->findGlyphIndex(code);
    if (glyph_index == -1) {
      glyph_index = font_->findGlyphIndex(font_->default_glyph_);
    }
    if (glyph_index == -1) {
      *mutable_right_metrics() =
          GlyphMetrics(0, 0, -1, -1, font_->default_space_width_);
      *mutable_right_data_offset() = 0;
    } else {
      class GlyphMetadataReader glyph_meta(*font_, glyph_index);
      *mutable_right_metrics() = glyph_meta.readMetrics(
          FONT_LAYOUT_HORIZONTAL, mutable_right_compressed());
      *mutable_right_data_offset() = glyph_meta.data_offset();
    }
  }

  void pushNull() { swapped_ = !swapped_; }

 private:
  GlyphMetrics* mutable_right_metrics() { return swapped_ ? &m1_ : &m2_; }

  long* mutable_right_data_offset() {
    return swapped_ ? &data_offset_1_ : &data_offset_2_;
  }

  bool& mutable_right_compressed() {
    return swapped_ ? compressed_1_ : compressed_2_;
  }

  const SmoothFontV2* font_;
  bool swapped_;
  GlyphMetrics m1_;
  GlyphMetrics m2_;
  long data_offset_1_;
  long data_offset_2_;
  bool compressed_1_;
  bool compressed_2_;
};

GlyphMetrics SmoothFontV2::getHorizontalStringMetrics(const char* utf8_data,
                                                      uint32_t size) const {
  roo_io::Utf8Decoder decoder(utf8_data, size);
  char32_t next_code;
  if (!decoder.next(next_code)) {
    // Nothing to draw.
    return GlyphMetrics(0, 0, -1, -1, 0);
  }
  GlyphPairIterator glyphs(this);
  glyphs.push(next_code);
  bool has_more;
  int16_t advance = 0;
  int16_t yMin = 32767;
  int16_t yMax = -32768;
  int16_t xMin = glyphs.right_metrics().lsb();
  int16_t xMax = xMin;
  do {
    char32_t code = next_code;
    has_more = decoder.next(next_code);
    int16_t kern;
    if (has_more) {
      glyphs.push(next_code);
      kern = kerning(code, next_code);
    } else {
      next_code = 0;
      glyphs.pushNull();
      kern = 0;
    }
    advance += (glyphs.left_metrics().advance() - kern);
    const GlyphMetrics& metrics = glyphs.left_metrics();
    if (yMax < metrics.glyphYMax()) {
      yMax = metrics.glyphYMax();
    }
    if (yMin > metrics.glyphYMin()) {
      yMin = metrics.glyphYMin();
    }
    // NOTE: need to do this at every glyph, because of possibly trailing
    // spaces.
    int16_t xm = advance - glyphs.left_metrics().rsb() - 1;
    if (xm > xMax) {
      xMax = xm;
    }
  } while (has_more);
  return GlyphMetrics(xMin, yMin, xMax, yMax, advance);
}

uint32_t SmoothFontV2::getHorizontalStringGlyphMetrics(
    const char* utf8_data, uint32_t size, GlyphMetrics* result, uint32_t offset,
    uint32_t max_count) const {
  roo_io::Utf8Decoder decoder(utf8_data, size);
  char32_t next_code;
  if (!decoder.next(next_code)) {
    // Nothing to measure.
    return 0;
  }
  uint32_t glyph_idx = 0;
  uint32_t glyph_count = 0;
  GlyphPairIterator glyphs(this);
  glyphs.push(next_code);
  bool has_more;
  int16_t advance = 0;
  do {
    if (glyph_count >= max_count) return glyph_count;
    char32_t code = next_code;
    has_more = decoder.next(next_code);
    int16_t kern;
    if (has_more) {
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

void SmoothFontV2::drawHorizontalString(const Surface& s, const char* utf8_data,
                                        uint32_t size, Color color) const {
  roo_io::Utf8Decoder decoder(utf8_data, size);
  char32_t next_code;
  if (!decoder.next(next_code)) {
    // Nothing to draw.
    return;
  }
  int16_t x = s.dx();
  int16_t y = s.dy();
  DisplayOutput& output = s.out();

  GlyphPairIterator glyphs(this);
  glyphs.push(next_code);
  int16_t preadvanced = 0;
  if (glyphs.right_metrics().lsb() < 0) {
    preadvanced = glyphs.right_metrics().lsb();
    x += preadvanced;
  }
  bool has_more;
  do {
    char32_t code = next_code;
    has_more = decoder.next(next_code);
    int16_t kern;
    if (has_more) {
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
                           glyphs.left_compressed(), glyphs.left_data(),
                           s.clip_box(), color, s.bgcolor(), s.blending_mode());
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
        drawKernedGlyphsModeFill(
            output, x, y, total_rect_width, glyphs.left_metrics(),
            glyphs.left_compressed(), glyphs.left_data(), -preadvanced,
            glyphs.right_metrics(), glyphs.right_compressed(),
            glyphs.right_data(), advance - preadvanced,
            Box::Intersect(s.clip_box(), Box(x, y - metrics().glyphYMax(),
                                             x + total_rect_width - 1,
                                             y - metrics().glyphYMin())),
            color, s.bgcolor(), s.blending_mode());
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
            glyphs.left_compressed(), glyphs.left_data(), -preadvanced,
            Box::Intersect(s.clip_box(), Box(x, y - metrics().glyphYMax(),
                                             x + total_rect_width - 1,
                                             y - metrics().glyphYMin())),
            color, s.bgcolor(), s.blending_mode());
      }
      x += total_rect_width;
      preadvanced = total_rect_width - (advance - preadvanced);
    }
  } while (has_more);
}

bool SmoothFontV2::getGlyphMetrics(char32_t code, FontLayout layout,
                                   GlyphMetrics* result) const {
  int glyph_index = findGlyphIndex(code);
  if (glyph_index == -1) return false;
  GlyphMetadataReader reader(*this, glyph_index);
  bool compressed;
  *result = reader.readMetrics(layout, compressed);
  return true;
}

int SmoothFontV2::findGlyphIndex(char32_t code) const {
  if (code > 0xFFFF) return -1;
  uint16_t ucode = (uint16_t)code;
  const int kCmapEntrySize = 12;
  for (int i = 0; i < cmap_entries_count_; ++i) {
    const roo::byte* PROGMEM entry = cmap_entries_begin_ + i * kCmapEntrySize;
    uint16_t range_start = readUWord(entry + 0);
    if (ucode < range_start) return -1;
    uint16_t range_length = readUWord(entry + 2);
    uint32_t range_end = (uint32_t)range_start + range_length;
    if (ucode >= range_end) continue;
    uint16_t glyph_id_offset = readUWord(entry + 4);
    uint8_t format = readByte(entry + 11);
    uint16_t rel = (uint16_t)(ucode - range_start);
    if (format == 0) {
      return glyph_id_offset + rel;
    }
    if (format == 1) {
      uint16_t data_entries_count = readUWord(entry + 6);
      uint32_t data_offset = readTriWord(entry + 8);
      const roo::byte* PROGMEM data = font_begin_ + data_offset;
      int lo = 0;
      int hi = (int)data_entries_count - 1;
      while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint16_t val = readUWord(data + mid * 2);
        if (val == rel) return glyph_id_offset + mid;
        if (rel < val) {
          hi = mid - 1;
        } else {
          lo = mid + 1;
        }
      }
      return -1;
    }
    return -1;
  }
  return -1;
}

template <int encoding_bytes>
char32_t read_unicode(const roo::byte* PROGMEM address);

template <>
char32_t read_unicode<1>(const roo::byte* PROGMEM address) {
  return pgm_read_byte(address);
}

template <>
char32_t read_unicode<2>(const roo::byte* PROGMEM address) {
  return ((char32_t)pgm_read_byte(address) << 8) | pgm_read_byte(address + 1);
}

template <int encoding_bytes>
const roo::byte* PROGMEM kernIndexSearch(uint32_t lookup,
                                         const roo::byte* PROGMEM data,
                                         int kern_size, int start, int stop) {
  int pivot = (start + stop) / 2;
  const roo::byte* PROGMEM pivot_ptr = data + (pivot * kern_size);
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

const roo::byte* PROGMEM SmoothFontV2::findKernPair(char32_t left,
                                                    char32_t right) const {
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

int16_t SmoothFontV2::kerning(char32_t left, char32_t right) const {
  const roo::byte* PROGMEM kern = findKernPair(left, right);
  if (kern == 0) {
    return 0;
  } else {
    return pgm_read_byte(kern + 2 * encoding_bytes_);
  }
}

}  // namespace roo_display