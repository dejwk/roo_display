// SmoothFontV2 binary format (version 0x0200)
//
// All multi-byte integers are big-endian. Offsets are from the start of the
// font data buffer.
//
// Header (variable size; depends on font_metric_bytes and encoding_bytes):
//   u16  version                 = 0x0200
//   u8   alpha_bits              (must be 4)
//   u8   encoding_bytes          (1=ASCII, 2=Unicode BMP)
//   u8   font_metric_bytes       (1 or 2)
//   u8   offset_bytes            (1..3)
//   u8   compression_method      (0=none, 1=RLE)
//   u16  glyph_count
//   u16  cmap_entries_count
//   s8/s16 metrics[11]           (xMin, yMin, xMax, yMax, ascent, descent,
//                                linegap, min_advance, max_advance,
//                                max_right_overhang, default_space_width)
//   u8/u16 default_glyph_code
//   u8   kerning_flags           (bits 1..0: 0=none, 1=pairs, 2=classes)
//   u16  glyph_metrics_offset    (absolute offset to glyph metrics section)
//   u24  glyph_data_offset       (absolute offset to glyph data)
//   u8   reserved                (must be 0)
//   u8   reserved                (must be 0)
//
// Cmap section (variable):
//   Repeated cmap_entries_count times (12 bytes each):
//     u16 range_start
//     u16 range_length
//     u16 glyph_id_offset
//     u16 data_entries_count
//     u24 data_offset             (to sparse indirection table)
//     u8  format                  (0=dense, 1=sparse)
//   Sparse indirection tables: for each sparse entry, data_entries_count
//   u16 values listing glyph offsets in the range.
//
// Glyph metrics section (offset = glyph_metrics_offset):
//   glyph_count entries, each:
//     s8/s16 xMin   (top two bits encode compression flag; see encoder)
//     s8/s16 yMin
//     s8/s16 xMax
//     s8/s16 yMax
//     s8/s16 advance
//     u8/u16/u24 data_offset       (offset into glyph data)
//
// Kerning section (starts immediately after glyph metrics):
//   Kerning header (type-specific):
//     format 1 (pairs): u16 kerning_pairs_count
//     format 2 (classes):
//       u16 kerning_class_count
//       u16 kerning_source_count
//       u16 kerning_class_entries_count
//   format 0: no data
//   format 1 (pairs): kerning_pairs_count entries (glyph indices):
//     u8/u16 left_glyph_index
//     u8/u16 right_glyph_index
//     u8     weight
//   format 2 (classes, glyph indices):
//     source table (kerning_source_count entries):
//       u8/u16 glyph_index, u8 class_id
//     class table (kerning_class_count entries):
//       u16 entry_offset, u16 entry_count
//     class entries (kerning_class_entries_count entries):
//       u8/u16 dest_glyph_index, u8 weight
//
// Glyph data section (offset = glyph_data_offset):
//   Concatenated glyph bitmaps, each compressed or uncompressed per
//   glyph metadata flag.

#include "roo_display/font/smooth_font_v2.h"

#include "roo_display/core/raster.h"
#include "roo_display/image/image.h"
#include "roo_display/internal/raw_streamable_overlay.h"
#include "roo_display/io/memory.h"
#include "roo_io/core/input_iterator.h"
#include "roo_io/text/unicode.h"
#include "roo_logging.h"

namespace roo_display {

namespace {
constexpr uint8_t kKerningFormatNone = 0;
constexpr uint8_t kKerningFormatPairs = 1;
constexpr uint8_t kKerningFormatClasses = 2;
}  // namespace

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
        layout == FontLayout::kHorizontal
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
  kerning_pairs_count_ = 0;
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

  uint8_t kerning_flags = roo_io::ReadU8(reader);
  kerning_format_ = kerning_flags & 0x03;
  uint16_t glyph_metrics_offset = roo_io::ReadBeU16(reader);
  uint32_t glyph_data_offset = ((uint32_t)roo_io::ReadU8(reader) << 16) |
                               ((uint32_t)roo_io::ReadU8(reader) << 8) |
                               (uint32_t)roo_io::ReadU8(reader);
  roo_io::ReadU8(reader);  // Reserved.
  roo_io::ReadU8(reader);  // Reserved.
  kerning_class_count_ = 0;
  kerning_source_count_ = 0;
  kerning_class_entries_count_ = 0;

  font_begin_ = font_data;
  cmap_entries_begin_ = reader.ptr();
  const int kCmapEntrySize = 12;
  if (cmap_entries_count_ > 0) {
    cmap_entries_ =
        std::unique_ptr<CmapEntry[]>(new CmapEntry[cmap_entries_count_]);
  }
  for (int i = 0; i < cmap_entries_count_; ++i) {
    const roo::byte* PROGMEM entry = cmap_entries_begin_ + i * kCmapEntrySize;
    CmapEntry& dst = cmap_entries_[i];
    dst.range_start = readUWord(entry + 0);
    uint16_t range_length = readUWord(entry + 2);
    dst.range_end = (uint32_t)dst.range_start + range_length;
    dst.glyph_id_offset = readUWord(entry + 4);
    dst.data_entries_count = readUWord(entry + 6);
    dst.data_offset = readTriWord(entry + 8);
    dst.format = readByte(entry + 11);
    CHECK_LT(dst.data_offset, (1u << 24)) << "Cmap data offset out of range";
  }

  glyph_metadata_begin_ = font_begin_ + glyph_metrics_offset;
  glyph_metadata_size_ = (5 * font_metric_bytes_) + offset_bytes_;
  glyph_kerning_begin_ =
      glyph_metadata_begin_ + glyph_metadata_size_ * glyph_count_;
  glyph_data_begin_ = font_begin_ + glyph_data_offset;
  glyph_index_bytes_ = (glyph_count_ < (1 << 8)) ? 1 : 2;

  kerning_source_table_begin_ = nullptr;
  kerning_class_table_begin_ = nullptr;
  kerning_class_entries_begin_ = nullptr;
  kerning_source_index_bytes_ = 0;
  kerning_class_entries_count_ = 0;
  glyph_kerning_size_ = 0;

  if (kerning_format_ == kKerningFormatPairs) {
    kerning_pairs_count_ = readUWord(glyph_kerning_begin_ + 0);
    glyph_kerning_begin_ += 2;
    glyph_kerning_size_ = 2 * glyph_index_bytes_ + 1;
    // glyph_data_begin_ already set from header.
  } else if (kerning_format_ == kKerningFormatClasses) {
    kerning_class_count_ = readUWord(glyph_kerning_begin_ + 0);
    kerning_source_count_ = readUWord(glyph_kerning_begin_ + 2);
    kerning_class_entries_count_ = readUWord(glyph_kerning_begin_ + 4);
    glyph_kerning_begin_ += 6;
    kerning_source_index_bytes_ = glyph_index_bytes_;
    kerning_source_table_begin_ = glyph_kerning_begin_;
    int source_entry_size = kerning_source_index_bytes_ + 1;
    int source_table_size = kerning_source_count_ * source_entry_size;
    kerning_class_table_begin_ =
        kerning_source_table_begin_ + source_table_size;
    kerning_class_entries_begin_ =
        kerning_class_table_begin_ + kerning_class_count_ * 4;
    // kerning_class_entries_count_ read from header.
  } else {
    glyph_data_begin_ = glyph_kerning_begin_;
  }

  Font::init(
      FontMetrics(ascent, descent, linegap, xMin, yMin, xMax, yMax,
                  max_right_overhang),
      FontProperties(
          encoding_bytes_ > 1 ? FontProperties::Charset::kUnicodeBmp
                              : FontProperties::Charset::kAscii,
          min_advance == max_advance && kerning_format_ == kKerningFormatNone
              ? FontProperties::Spacing::kMonospace
              : FontProperties::Spacing::kProportional,
          alpha_bits_ > 1 ? FontProperties::Smoothing::kGrayscale
                          : FontProperties::Smoothing::kNone,
          kerning_format_ != kKerningFormatNone
              ? FontProperties::Kerning::kPairs
              : FontProperties::Kerning::kNone));

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
            false, bgcolor, FillMode::kVisible, blending_mode);
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
      (blending_mode == BlendingMode::kSourceOver ||
       blending_mode == BlendingMode::kSourceOverOpaque)) {
    // All souce pixels will be fully opaque.
    blending_mode = BlendingMode::kSource;
  }

  if (outer.clip(clip_box) == Box::ClipResult::kEmpty) return;
  Box inner = glyph.extents().translate(x, y);
  if (inner.clip(clip_box) == Box::ClipResult::kEmpty) {
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
  Surface s(output, x, y, clip_box, false, bgColor, FillMode::kExtents,
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

  int left_glyph_index() const {
    return swapped_ ? glyph_index_2_ : glyph_index_1_;
  }

  int right_glyph_index() const {
    return swapped_ ? glyph_index_1_ : glyph_index_2_;
  }

  void push(char32_t code) {
    swapped_ = !swapped_;
    if (is_space(code)) {
      *mutable_right_metrics() =
          GlyphMetrics(0, 0, -1, -1, font_->default_space_width_);
      *mutable_right_data_offset() = 0;
      *mutable_right_glyph_index() = -1;
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
      *mutable_right_glyph_index() = -1;
    } else {
      class GlyphMetadataReader glyph_meta(*font_, glyph_index);
      *mutable_right_metrics() = glyph_meta.readMetrics(
          FontLayout::kHorizontal, mutable_right_compressed());
      *mutable_right_data_offset() = glyph_meta.data_offset();
      *mutable_right_glyph_index() = glyph_index;
    }
  }

  void pushNull() {
    swapped_ = !swapped_;
    *mutable_right_glyph_index() = -1;
  }

 private:
  GlyphMetrics* mutable_right_metrics() { return swapped_ ? &m1_ : &m2_; }

  long* mutable_right_data_offset() {
    return swapped_ ? &data_offset_1_ : &data_offset_2_;
  }

  int* mutable_right_glyph_index() {
    return swapped_ ? &glyph_index_1_ : &glyph_index_2_;
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
  int glyph_index_1_ = -1;
  int glyph_index_2_ = -1;
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
    has_more = decoder.next(next_code);
    int16_t kern;
    if (has_more) {
      glyphs.push(next_code);
      kern = kerning(glyphs.left_glyph_index(), glyphs.right_glyph_index());
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
    has_more = decoder.next(next_code);
    int16_t kern;
    if (has_more) {
      glyphs.push(next_code);
      kern = kerning(glyphs.left_glyph_index(), glyphs.right_glyph_index());
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
    has_more = decoder.next(next_code);
    int16_t kern;
    if (has_more) {
      glyphs.push(next_code);
      kern = kerning(glyphs.left_glyph_index(), glyphs.right_glyph_index());
    } else {
      next_code = 0;
      glyphs.pushNull();
      kern = 0;
    }
    if (s.fill_mode() == FillMode::kVisible) {
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
  for (int i = 0; i < cmap_entries_count_; ++i) {
    const CmapEntry& entry = cmap_entries_[i];
    if (ucode < entry.range_start) return -1;
    if (ucode >= entry.range_end) continue;
    uint16_t rel = (uint16_t)(ucode - entry.range_start);
    if (entry.format == 0) {
      return entry.glyph_id_offset + rel;
    }
    if (entry.format == 1) {
      const roo::byte* PROGMEM data = font_begin_ + entry.data_offset;
      int lo = 0;
      int hi = (int)entry.data_entries_count - 1;
      while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint16_t val = readUWord(data + mid * 2);
        if (val == rel) return entry.glyph_id_offset + mid;
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

static uint16_t read_glyph_index(const roo::byte* PROGMEM address,
                                 int index_bytes) {
  return index_bytes == 1 ? pgm_read_byte(address) : readUWord(address);
}

const roo::byte* PROGMEM SmoothFontV2::findKernPair(char32_t left,
                                                    char32_t right) const {
  // Legacy API no longer used; keep for compatibility if needed.
  return nullptr;
}

int16_t SmoothFontV2::kerning(int left_glyph_index,
                              int right_glyph_index) const {
  if (left_glyph_index < 0 || right_glyph_index < 0) return 0;
  if (kerning_format_ == kKerningFormatPairs) {
    // Pair format: binary search over sorted (left,right) glyph index pairs.
    int lo = 0;
    int hi = kerning_pairs_count_ - 1;
    while (lo <= hi) {
      int mid = (lo + hi) / 2;
      const roo::byte* PROGMEM entry =
          glyph_kerning_begin_ + mid * glyph_kerning_size_;
      uint16_t left = read_glyph_index(entry, glyph_index_bytes_);
      uint16_t right =
          read_glyph_index(entry + glyph_index_bytes_, glyph_index_bytes_);
      if (left == left_glyph_index && right == right_glyph_index) {
        return pgm_read_byte(entry + 2 * glyph_index_bytes_);
      }
      if (left < left_glyph_index ||
          (left == left_glyph_index && right < right_glyph_index)) {
        lo = mid + 1;
      } else {
        hi = mid - 1;
      }
    }
    return 0;
  }
  if (kerning_format_ == kKerningFormatClasses) {
    return kerningWithClassFormat(left_glyph_index, right_glyph_index);
  }
  return 0;
}

int16_t SmoothFontV2::kerningWithClassFormat(int left_glyph_index,
                                             int right_glyph_index) const {
  if (kerning_source_count_ == 0 || kerning_class_count_ == 0) return 0;

  // 1) Find the class assigned to the left glyph using binary search
  //    over the source table (glyph_index -> class_id).
  int entry_size = kerning_source_index_bytes_ + 1;
  int lo = 0;
  int hi = kerning_source_count_ - 1;
  uint8_t class_id = 0xFF;

  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    const roo::byte* PROGMEM entry =
        kerning_source_table_begin_ + mid * entry_size;
    uint16_t glyph_index = read_glyph_index(entry, kerning_source_index_bytes_);
    if (glyph_index == left_glyph_index) {
      class_id = pgm_read_byte(entry + kerning_source_index_bytes_);
      break;
    }
    if (glyph_index < left_glyph_index) {
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }

  if (class_id == 0xFF || class_id >= kerning_class_count_) return 0;

  // 2) Look up the class entry range (offset,count).
  const roo::byte* PROGMEM cls = kerning_class_table_begin_ + class_id * 4;
  uint16_t offset = readUWord(cls + 0);
  uint16_t count = readUWord(cls + 2);
  if (count == 0) return 0;

  // 3) Binary search within the class entries for the right glyph index.
  const roo::byte* PROGMEM entries =
      kerning_class_entries_begin_ + offset * entry_size;
  lo = 0;
  hi = count - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    const roo::byte* PROGMEM entry = entries + mid * entry_size;
    uint16_t glyph_index = read_glyph_index(entry, kerning_source_index_bytes_);
    if (glyph_index == right_glyph_index) {
      return pgm_read_byte(entry + kerning_source_index_bytes_);
    }
    if (glyph_index < right_glyph_index) {
      lo = mid + 1;
    } else {
      hi = mid - 1;
    }
  }
  return 0;
}

}  // namespace roo_display