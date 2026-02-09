
#include "roo_display/core/raster.h"

#include <memory>
#include <random>

#include "roo_display/color/color.h"
#include "roo_display/color/color_mode_indexed.h"
#include "roo_display/io/memory.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

namespace {

void FillIndexedPalette(Color* colors, int size) {
  colors[0] = color::Transparent;
  for (int i = 1; i < size; ++i) {
    colors[i] = Color(i, i, i);
  }
}

}  // namespace

void Draw(DisplayDevice& output, int16_t x, int16_t y, const Box& clip_box,
          const Drawable& object, FillMode fill_mode = FILL_MODE_VISIBLE,
          BlendingMode blending_mode = BLENDING_MODE_SOURCE_OVER,
          Color bgcolor = color::Transparent) {
  output.begin();
  Surface s(output, x, y, clip_box, false, bgcolor, fill_mode, blending_mode);
  s.drawObject(object);
  output.end();
}

void Draw(DisplayDevice& output, int16_t x, int16_t y, const Drawable& object,
          FillMode fill_mode = FILL_MODE_VISIBLE,
          BlendingMode blending_mode = BLENDING_MODE_SOURCE_OVER,
          Color bgcolor = color::Transparent) {
  Box clip_box(0, 0, output.effective_width() - 1,
               output.effective_height() - 1);
  Draw(output, x, y, clip_box, object, fill_mode, blending_mode, bgcolor);
}

TEST(Raster, MonochromeMsbFirst) {
  unsigned char data[] = {0xC1, 0x26, 0xE7};
  ConstDramRaster<Monochrome> raster(4, 6, (const roo::byte*)data,
                                     WhiteOnBlack());
  EXPECT_THAT(raster, MatchesContent(WhiteOnBlack(), 4, 6,
                                     "**  "
                                     "   *"
                                     "  * "
                                     " ** "
                                     "*** "
                                     " ***"));
}

TEST(Raster, MonochromeLsbFirst) {
  unsigned char data[] = {0xC1, 0x26, 0xE7};
  ConstDramRaster<Monochrome, COLOR_PIXEL_ORDER_LSB_FIRST> raster(
      4, 6, (const roo::byte*)data, WhiteOnBlack());
  EXPECT_THAT(raster, MatchesContent(WhiteOnBlack(), 4, 6,
                                     "*   "
                                     "  **"
                                     " ** "
                                     " *  "
                                     "*** "
                                     " ***"));
}

TEST(Raster, Grayscale4MsbFirst) {
  unsigned char data[] = {0xC1, 0x26, 0xE7};
  ConstDramRaster<Grayscale4> raster(3, 2, (const roo::byte*)data);
  EXPECT_THAT(raster, MatchesContent(Grayscale4(), 3, 2,
                                     "C12"
                                     "6E7"));
}

TEST(Raster, Grayscale4LsbFirst) {
  unsigned char data[] = {0xC1, 0x26, 0xE7};
  ConstDramRaster<Grayscale4, COLOR_PIXEL_ORDER_LSB_FIRST> raster(
      3, 2, (const roo::byte*)data);
  EXPECT_THAT(raster, MatchesContent(Grayscale4(), 3, 2,
                                     "1C6"
                                     "27E"));
}

TEST(Raster, Indexed1MsbFirst) {
  roo::byte data[1] = {static_cast<roo::byte>(0x00)};
  SubByteColorIo<Indexed1, COLOR_PIXEL_ORDER_MSB_FIRST> io;
  const uint8_t indices[] = {0, 1, 1, 0, 1, 0, 0, 1};
  for (int i = 0; i < 8; ++i) {
    io.storeRaw(indices[i], &data[0], i);
  }
  Color colors[2];
  FillIndexedPalette(colors, 2);
  Palette palette = Palette::ReadOnly(colors, 2);
  ConstDramRaster<Indexed1> raster(4, 2, data, Indexed1(&palette));
  EXPECT_THAT(raster, MatchesContent(Indexed1(&palette), 4, 2,
                                     "0110"
                                     "1001"));
}

TEST(Raster, Indexed1LsbFirst) {
  roo::byte data[1] = {static_cast<roo::byte>(0x00)};
  SubByteColorIo<Indexed1, COLOR_PIXEL_ORDER_LSB_FIRST> io;
  const uint8_t indices[] = {0, 1, 1, 0, 1, 0, 0, 1};
  for (int i = 0; i < 8; ++i) {
    io.storeRaw(indices[i], &data[0], i);
  }
  Color colors[2];
  FillIndexedPalette(colors, 2);
  Palette palette = Palette::ReadOnly(colors, 2);
  ConstDramRaster<Indexed1, COLOR_PIXEL_ORDER_LSB_FIRST> raster(
      4, 2, data, Indexed1(&palette));
  EXPECT_THAT(raster, MatchesContent(Indexed1(&palette), 4, 2,
                                     "0110"
                                     "1001"));
}

TEST(Raster, Indexed2MsbFirst) {
  roo::byte data[2] = {static_cast<roo::byte>(0x00),
                       static_cast<roo::byte>(0x00)};
  SubByteColorIo<Indexed2, COLOR_PIXEL_ORDER_MSB_FIRST> io;
  const uint8_t indices[] = {0, 1, 2, 3, 3, 2, 1, 0};
  for (int i = 0; i < 8; ++i) {
    io.storeRaw(indices[i], &data[i / 4], i % 4);
  }
  Color colors[4];
  FillIndexedPalette(colors, 4);
  Palette palette = Palette::ReadOnly(colors, 4);
  ConstDramRaster<Indexed2> raster(4, 2, data, Indexed2(&palette));
  EXPECT_THAT(raster, MatchesContent(Indexed2(&palette), 4, 2,
                                     "0123"
                                     "3210"));
}

TEST(Raster, Indexed2LsbFirst) {
  roo::byte data[2] = {static_cast<roo::byte>(0x00),
                       static_cast<roo::byte>(0x00)};
  SubByteColorIo<Indexed2, COLOR_PIXEL_ORDER_LSB_FIRST> io;
  const uint8_t indices[] = {0, 1, 2, 3, 3, 2, 1, 0};
  for (int i = 0; i < 8; ++i) {
    io.storeRaw(indices[i], &data[i / 4], i % 4);
  }
  Color colors[4];
  FillIndexedPalette(colors, 4);
  Palette palette = Palette::ReadOnly(colors, 4);
  ConstDramRaster<Indexed2, COLOR_PIXEL_ORDER_LSB_FIRST> raster(
      4, 2, data, Indexed2(&palette));
  EXPECT_THAT(raster, MatchesContent(Indexed2(&palette), 4, 2,
                                     "0123"
                                     "3210"));
}

TEST(Raster, Indexed4MsbFirst) {
  unsigned char data[] = {0xC1, 0x26, 0xE7};
  Color colors[16];
  FillIndexedPalette(colors, 16);
  Palette palette = Palette::ReadOnly(colors, 16);
  ConstDramRaster<Indexed4> raster(3, 2, (const roo::byte*)data,
                                   Indexed4(&palette));
  EXPECT_THAT(raster, MatchesContent(Indexed4(&palette), 3, 2,
                                     "C12"
                                     "6E7"));
}

TEST(Raster, Indexed4LsbFirst) {
  unsigned char data[] = {0xC1, 0x26, 0xE7};
  Color colors[16];
  FillIndexedPalette(colors, 16);
  Palette palette = Palette::ReadOnly(colors, 16);
  ConstDramRaster<Indexed4, COLOR_PIXEL_ORDER_LSB_FIRST> raster(
      3, 2, (const roo::byte*)data, Indexed4(&palette));
  EXPECT_THAT(raster, MatchesContent(Indexed4(&palette), 3, 2,
                                     "1C6"
                                     "27E"));
}

TEST(Raster, Indexed8) {
  unsigned char data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04};
  Color colors[256];
  FillIndexedPalette(colors, 256);
  Palette palette = Palette::ReadOnly(colors, 256);
  ConstDramRaster<Indexed8> raster(3, 2, (const roo::byte*)data,
                                   Indexed8(&palette));
  EXPECT_THAT(raster, MatchesContent(Indexed8(&palette), 3, 2,
                                     "C1 26 E7"
                                     "54 F3 04"));
}

TEST(Raster, Alpha4MsbFirst) {
  unsigned char data[] = {0xC1, 0x26, 0xE7};
  ConstDramRaster<Alpha4> raster(3, 2, (const roo::byte*)data,
                                 Alpha4(color::Red));
  EXPECT_THAT(raster, MatchesContent(Argb4444(), 3, 2,
                                     "CF00 1F00 2F00"
                                     "6F00 EF00 7F00"));
}

TEST(Raster, Alpha4LsbFirst) {
  unsigned char data[] = {0xC1, 0x26, 0xE7};
  ConstDramRaster<Alpha4, COLOR_PIXEL_ORDER_LSB_FIRST> raster(
      3, 2, (const roo::byte*)data, Alpha4(color::Blue));
  EXPECT_THAT(raster, MatchesContent(Argb4444(), 3, 2,
                                     "100F C00F 600F"
                                     "200F 700F E00F"));
}

TEST(Raster, Grayscale8) {
  unsigned char data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04};
  ConstDramRaster<Grayscale8> raster(3, 2, (const roo::byte*)data);
  EXPECT_THAT(raster, MatchesContent(Grayscale8(), 3, 2,
                                     "C1 26 E7"
                                     "54 F3 04"));
}

TEST(Raster, Alpha8) {
  unsigned char data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04};
  ConstDramRaster<Alpha8> raster(3, 2, (const roo::byte*)data,
                                 Alpha8(color::Lime));
  EXPECT_THAT(raster, MatchesContent(Argb8888(), 3, 2,
                                     "C100FF00 2600FF00 E700FF00"
                                     "5400FF00 F300FF00 0400FF00"));
}

TEST(Raster, Argb4444BigEndian) {
  unsigned char data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04,
                          0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  ConstDramRaster<Argb4444> raster(3, 2, (const roo::byte*)data);
  EXPECT_THAT(raster, MatchesContent(Argb4444(), 3, 2,
                                     "C126 E754 F304"
                                     "DD39 2E80 11F5"));
}

TEST(Raster, Argb4444LittleEndian) {
  unsigned char data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04,
                          0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  ConstDramRasterLE<Argb4444> raster(3, 2, (const roo::byte*)data);
  EXPECT_THAT(raster, MatchesContent(Argb4444(), 3, 2,
                                     "26C1 54E7 04F3"
                                     "39DD 802E F511"));
}

TEST(Raster, Argb6666BigEndian) {
  unsigned char data[] = {0xFF, 0xF0, 0x00, 0xFC, 0x0F, 0xC0, 0xFC, 0x00, 0x3F,
                          0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x39, 0x2E, 0x80};
  ConstDramRaster<Argb6666> raster(3, 2, (const roo::byte*)data);
  EXPECT_THAT(raster, MatchesContent(Argb6666(), 3, 2,
                                     "**__ *_*_ *__*"
                                     "**** ____ DHv_"));
}

TEST(Raster, Argb6666LittleEndian) {
  unsigned char data[] = {0x00, 0xF0, 0xFF, 0xC0, 0x0F, 0xFC, 0x3F, 0x00, 0xFC,
                          0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x80, 0x2E, 0x39};
  ConstDramRasterLE<Argb6666> raster(3, 2, (const roo::byte*)data);
  EXPECT_THAT(raster, MatchesContent(Argb6666(), 3, 2,
                                     "**__ *_*_ *__*"
                                     "**** ____ DHv_"));
}

TEST(Raster, Argb8888BigEndian) {
  unsigned char data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04, 0x22, 0x5F,
                          0x65, 0x01, 0x86, 0x00, 0xCA, 0xFE, 0xBA, 0xBE,
                          0xDE, 0xAD, 0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  ConstDramRaster<Argb8888> raster(3, 2, (const roo::byte*)data);
  EXPECT_THAT(raster, MatchesContent(Argb8888(), 3, 2,
                                     "C126E754 F304225F 65018600"
                                     "CAFEBABE DEADDD39 2E8011F5"));
}

TEST(Raster, Argb8888LittleEndian) {
  unsigned char data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04, 0x22, 0x5F,
                          0x65, 0x01, 0x86, 0x00, 0xCA, 0xFE, 0xBA, 0xBE,
                          0xDE, 0xAD, 0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  ConstDramRasterLE<Argb8888> raster(3, 2, (const roo::byte*)data);
  EXPECT_THAT(raster, MatchesContent(Argb8888(), 3, 2,
                                     "54E726C1 5F2204F3 00860165"
                                     "BEBAFECA 39DDADDE F511802E"));
}

TEST(Raster, Rgb565BigEndian) {
  unsigned char data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04,
                          0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  ConstDramRaster<Rgb565> raster(3, 2, (const roo::byte*)data);
  EXPECT_THAT(raster, MatchesContent(Rgb565(), 3, 2,
                                     "m8B uve yN7"
                                     "seo 9p_ 3Eg"));
}

TEST(Raster, Rgb565LittleEndian) {
  unsigned char data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04,
                          0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  ConstDramRasterLE<Rgb565> raster(3, 2, (const roo::byte*)data);
  EXPECT_THAT(raster, MatchesContent(Rgb565(), 3, 2,
                                     "7r1 JcD _cc"
                                     "DDw W0R ydY"));
}

TEST(Raster, Rgb565WithTransparencyBigEndian) {
  unsigned char data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04,
                          0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  ConstDramRaster<Rgb565WithTransparency> raster(
      3, 2, (const roo::byte*)data, Rgb565WithTransparency(0x2E80));
  EXPECT_THAT(raster, MatchesContent(Argb8888(), 3, 2,
                                     "FFC62431 FFE7EBA5 FFF76121"
                                     "FFDEA6CE 00000000 FF103CAD"));
}

TEST(Raster, SubbyteDrawTo) {
  unsigned char data[] = {0xC1, 0x26, 0xE7};
  ConstDramRaster<Monochrome> raster(4, 6, (const roo::byte*)data,
                                     WhiteOnBlack());
  FakeOffscreen<Rgb565> test_screen(4, 6, color::Black);
  Draw(test_screen, 0, 0, raster);
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 4, 6,
                                          "**  "
                                          "   *"
                                          "  * "
                                          " ** "
                                          "*** "
                                          " ***"));
}

TEST(Raster, SubbyteStreamable) {
  unsigned char data[] = {0xC1, 0x26, 0xE7};
  ConstDramRaster<Monochrome> raster(4, 6, (const roo::byte*)data,
                                     WhiteOnBlack());
  FakeOffscreen<Rgb565> test_screen(4, 6, color::Black);
  Draw(test_screen, 0, 0, ForcedStreamable(&raster));
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 4, 6,
                                          "**  "
                                          "   *"
                                          "  * "
                                          " ** "
                                          "*** "
                                          " ***"));
}

TEST(Raster, SubbyteStreamableClipped) {
  unsigned char data[] = {0xC1, 0x26, 0xE7};
  ConstDramRaster<Monochrome> raster(4, 6, (const roo::byte*)data,
                                     WhiteOnBlack());
  FakeOffscreen<Rgb565> test_screen(4, 6, color::Black);
  Draw(test_screen, 0, 0, Box(0, 0, 2, 2), ForcedStreamable(&raster));
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 4, 6,
                                          "**  "
                                          "    "
                                          "  * "
                                          "    "
                                          "    "
                                          "    "));
}

TEST(Raster, MultibyteDrawTo) {
  unsigned char data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04,
                          0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  ConstDramRaster<Rgb565> raster(3, 2, (const roo::byte*)data);
  FakeOffscreen<Rgb565> test_screen(3, 2, color::Black);
  Draw(test_screen, 0, 0, raster);
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 3, 2,
                                          "m8B uve yN7"
                                          "seo 9p_ 3Eg"));
}

TEST(Raster, MultibyteStreamable) {
  unsigned char data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04,
                          0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  ConstDramRaster<Rgb565> raster(3, 2, (const roo::byte*)data);
  FakeOffscreen<Rgb565> test_screen(3, 2, color::Black);
  Draw(test_screen, 0, 0, ForcedStreamable(&raster));
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 3, 2,
                                          "m8B uve yN7"
                                          "seo 9p_ 3Eg"));
}

TEST(Raster, MultibyteStreamableClipped) {
  unsigned char data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04,
                          0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  ConstDramRaster<Rgb565> raster(3, 2, (const roo::byte*)data);
  FakeOffscreen<Rgb565> test_screen(3, 2, color::Black);
  Draw(test_screen, 0, 0, Box(0, 1, 2, 1), ForcedStreamable(&raster));
  EXPECT_THAT(test_screen, MatchesContent(Rgb565(), 3, 2,
                                          "___ ___ ___"
                                          "seo 9p_ 3Eg"));
}

TEST(Raster, Argb6666BigEndianDrawTo) {
  unsigned char data[] = {0xFF, 0xF0, 0x00, 0xFC, 0x0F, 0xC0, 0xFC, 0x00, 0x3F,
                          0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x39, 0x2E, 0x80};
  ConstDramRaster<Argb6666> raster(3, 2, (const roo::byte*)data);
  FakeOffscreen<Argb8888> test_screen(3, 2, color::Transparent);
  Draw(test_screen, 0, 0, raster);
  EXPECT_THAT(test_screen, MatchesContent(Argb6666(), 3, 2,
                                          "**__ *_*_ *__*"
                                          "**** ____ DHv_"));
}

TEST(Raster, Argb6666LittleEndianDrawTo) {
  unsigned char data[] = {0x00, 0xF0, 0xFF, 0xC0, 0x0F, 0xFC, 0x3F, 0x00, 0xFC,
                          0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x80, 0x2E, 0x39};
  ConstDramRasterLE<Argb6666> raster(3, 2, (const roo::byte*)data);
  FakeOffscreen<Argb8888> test_screen(3, 2, color::Transparent);
  Draw(test_screen, 0, 0, raster);
  EXPECT_THAT(test_screen, MatchesContent(Argb6666(), 3, 2,
                                          "**__ *_*_ *__*"
                                          "**** ____ DHv_"));
}

TEST(Raster, OffsetExtents) {
  unsigned char data[] = {0xC1, 0x26, 0xE7};
  ConstDramRaster<Monochrome> raster(Box(2, 3, 5, 8), (const roo::byte*)data,
                                     WhiteOnBlack());
  FakeOffscreen<Rgb565> test_screen(6, 9, color::Black);
  Draw(test_screen, 0, 0, raster);
  EXPECT_THAT(test_screen, MatchesContent(WhiteOnBlack(), 6, 9,
                                          "      "
                                          "      "
                                          "      "
                                          "  **  "
                                          "     *"
                                          "    * "
                                          "   ** "
                                          "  *** "
                                          "   ***"));
}

}  // namespace roo_display
