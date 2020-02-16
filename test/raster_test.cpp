
#include "roo_display/core/raster.h"

#include <memory>
#include <random>

#include "roo_display/core/color.h"
#include "roo_display/io/memory.h"
#include "testing.h"

using namespace testing;

namespace roo_display {

TEST(Raster, MonochromeMsbFirst) {
  uint8_t data[] = {0xC1, 0x26, 0xE7};
  RasterMonochrome<const uint8_t*> raster(4, 6, data, WhiteOnBlack());
  EXPECT_THAT(raster, MatchesContent(WhiteOnBlack(), 4, 6,
                                     "**  "
                                     "   *"
                                     "  * "
                                     " ** "
                                     "*** "
                                     " ***"));
}

TEST(Raster, MonochromeLsbFirst) {
  uint8_t data[] = {0xC1, 0x26, 0xE7};
  RasterMonochrome<const uint8_t*, COLOR_PIXEL_ORDER_LSB_FIRST> raster(
      4, 6, data, WhiteOnBlack());
  EXPECT_THAT(raster, MatchesContent(WhiteOnBlack(), 4, 6,
                                     "*   "
                                     "  **"
                                     " ** "
                                     " *  "
                                     "*** "
                                     " ***"));
}

TEST(Raster, Grayscale4MsbFirst) {
  uint8_t data[] = {0xC1, 0x26, 0xE7};
  RasterGrayscale4<const uint8_t*> raster(3, 2, data);
  EXPECT_THAT(raster, MatchesContent(Grayscale4(), 3, 2,
                                     "C12"
                                     "6E7"));
}

TEST(Raster, Grayscale4LsbFirst) {
  uint8_t data[] = {0xC1, 0x26, 0xE7};
  RasterGrayscale4<const uint8_t*, COLOR_PIXEL_ORDER_LSB_FIRST> raster(3, 2,
                                                                       data);
  EXPECT_THAT(raster, MatchesContent(Grayscale4(), 3, 2,
                                     "1C6"
                                     "27E"));
}

TEST(Raster, Alpha4MsbFirst) {
  uint8_t data[] = {0xC1, 0x26, 0xE7};
  RasterAlpha4<const uint8_t*> raster(3, 2, data, Alpha4(color::Red));
  EXPECT_THAT(raster, MatchesContent(Argb4444(), 3, 2,
                                     "CF00 1F00 2F00"
                                     "6F00 EF00 7F00"));
}

TEST(Raster, Alpha4LsbFirst) {
  uint8_t data[] = {0xC1, 0x26, 0xE7};
  RasterAlpha4<const uint8_t*, COLOR_PIXEL_ORDER_LSB_FIRST> raster(
      3, 2, data, Alpha4(color::Blue));
  EXPECT_THAT(raster, MatchesContent(Argb4444(), 3, 2,
                                     "100F C00F 600F"
                                     "200F 700F E00F"));
}

TEST(Raster, Grayscale8) {
  uint8_t data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04};
  RasterGrayscale8<const uint8_t*> raster(3, 2, data);
  EXPECT_THAT(raster, MatchesContent(Grayscale8(), 3, 2,
                                     "C1 26 E7"
                                     "54 F3 04"));
}

TEST(Raster, Alpha8) {
  uint8_t data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04};
  RasterAlpha8<const uint8_t*> raster(3, 2, data, Alpha8(color::Lime));
  EXPECT_THAT(raster, MatchesContent(Argb8888(), 3, 2,
                                     "C100FF00 2600FF00 E700FF00"
                                     "5400FF00 F300FF00 0400FF00"));
}

TEST(Raster, Argb4444BigEndian) {
  uint8_t data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04,
                    0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  RasterArgb4444<const uint8_t*> raster(3, 2, data);
  EXPECT_THAT(raster, MatchesContent(Argb4444(), 3, 2,
                                     "C126 E754 F304"
                                     "DD39 2E80 11F5"));
}

TEST(Raster, Argb4444LittleEndian) {
  uint8_t data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04,
                    0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  RasterArgb4444<const uint8_t*, BYTE_ORDER_LITTLE_ENDIAN> raster(3, 2, data);
  EXPECT_THAT(raster, MatchesContent(Argb4444(), 3, 2,
                                     "26C1 54E7 04F3"
                                     "39DD 802E F511"));
}

TEST(Raster, Argb6666BigEndian) {
  uint8_t data[] = {0xFF, 0xF0, 0x00, 0xFC, 0x0F, 0xC0, 0xFC, 0x00, 0x3F,
                    0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x39, 0x2E, 0x80};
  RasterArgb6666<const uint8_t*> raster(3, 2, data);
  EXPECT_THAT(raster, MatchesContent(Argb6666(), 3, 2,
                                     "**__ *_*_ *__*"
                                     "**** ____ DHv_"));
}

TEST(Raster, Argb6666LittleEndian) {
  uint8_t data[] = {0x00, 0xF0, 0xFF, 0xC0, 0x0F, 0xFC, 0x3F, 0x00, 0xFC,
                    0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x80, 0x2E, 0x39};
  RasterArgb6666<const uint8_t*, BYTE_ORDER_LITTLE_ENDIAN> raster(3, 2, data);
  EXPECT_THAT(raster, MatchesContent(Argb6666(), 3, 2,
                                     "**__ *_*_ *__*"
                                     "**** ____ DHv_"));
}

TEST(Raster, Argb8888BigEndian) {
  uint8_t data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04, 0x22, 0x5F,
                    0x65, 0x01, 0x86, 0x00, 0xCA, 0xFE, 0xBA, 0xBE,
                    0xDE, 0xAD, 0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  RasterArgb8888<const uint8_t*> raster(3, 2, data);
  EXPECT_THAT(raster, MatchesContent(Argb8888(), 3, 2,
                                     "C126E754 F304225F 65018600"
                                     "CAFEBABE DEADDD39 2E8011F5"));
}

TEST(Raster, Argb8888LittleEndian) {
  uint8_t data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04, 0x22, 0x5F,
                    0x65, 0x01, 0x86, 0x00, 0xCA, 0xFE, 0xBA, 0xBE,
                    0xDE, 0xAD, 0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  RasterArgb8888<const uint8_t*, BYTE_ORDER_LITTLE_ENDIAN> raster(3, 2, data);
  EXPECT_THAT(raster, MatchesContent(Argb8888(), 3, 2,
                                     "54E726C1 5F2204F3 00860165"
                                     "BEBAFECA 39DDADDE F511802E"));
}

TEST(Raster, Rgb565BigEndian) {
  uint8_t data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04,
                    0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  RasterRgb565<const uint8_t*> raster(3, 2, data);
  EXPECT_THAT(raster, MatchesContent(Rgb565(), 3, 2,
                                     "m8B uve yN7"
                                     "seo 9p_ 3Eg"));
}

TEST(Raster, Rgb565LittleEndian) {
  uint8_t data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04,
                    0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  RasterRgb565<const uint8_t*, BYTE_ORDER_LITTLE_ENDIAN> raster(3, 2, data);
  EXPECT_THAT(raster, MatchesContent(Rgb565(), 3, 2,
                                     "7r1 JcD _cc"
                                     "DDw W0R ydY"));
}

TEST(Raster, Rgb565WithTransparencyBigEndian) {
  uint8_t data[] = {0xC1, 0x26, 0xE7, 0x54, 0xF3, 0x04,
                    0xDD, 0x39, 0x2E, 0x80, 0x11, 0xF5};
  RasterRgb565WithTransparency<const uint8_t*> raster(
      3, 2, data, Rgb565WithTransparency(0x2E80));
  EXPECT_THAT(raster, MatchesContent(Argb8888(), 3, 2,
                                     "FFC62431 FFE7EBA5 FFF76121"
                                     "FFDEA6CE 00000000 FF103CAD"));
}

}  // namespace roo_display
