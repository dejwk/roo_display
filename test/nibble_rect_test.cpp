
#include "roo_display/internal/nibble_rect.h"

#include "roo_display.h"
#include "roo_display/core/color.h"
#include "roo_display/core/offscreen.h"
#include "testing.h"
#include "testing_display_device.h"

using namespace testing;

namespace roo_display {
namespace internal {

TEST(NibbleRect, Basics) {
  uint8_t buffer[40];
  memset(buffer, 0, 40);
  RasterGrayscale4<const uint8_t*> raster(10, 8, buffer);
  NibbleRect nibble_rect(buffer, 5, 8);
  EXPECT_EQ(5, nibble_rect.width_bytes());
  EXPECT_EQ(10, nibble_rect.width());
  EXPECT_EQ(8, nibble_rect.height());
}

TEST(NibbleRect, SetGet) {
  uint8_t buffer[40];
  memset(buffer, 0, 40);
  RasterGrayscale4<const uint8_t*> raster(10, 8, buffer);
  NibbleRect nibble_rect(buffer, 5, 8);
  EXPECT_THAT(raster, MatchesContent(Grayscale4(), 10, 8,
                                     "          "
                                     "          "
                                     "          "
                                     "          "
                                     "          "
                                     "          "
                                     "          "
                                     "          "));
  nibble_rect.set(1, 2, 13);
  nibble_rect.set(4, 3, 1);
  nibble_rect.set(9, 7, 15);
  nibble_rect.set(7, 2, 3);
  EXPECT_THAT(raster, MatchesContent(Grayscale4(), 10, 8,
                                     "          "
                                     "          "
                                     " D     3  "
                                     "    1     "
                                     "          "
                                     "          "
                                     "          "
                                     "         F"));
  EXPECT_EQ(0, nibble_rect.get(0, 0));
  EXPECT_EQ(13, nibble_rect.get(1, 2));
  EXPECT_EQ(1, nibble_rect.get(4, 3));
  EXPECT_EQ(15, nibble_rect.get(9, 7));
  EXPECT_EQ(3, nibble_rect.get(7, 2));
}

TEST(NibbleRect, FillRect) {
  uint8_t buffer[40];
  memset(buffer, 0, 40);
  RasterGrayscale4<const uint8_t*> raster(10, 8, buffer);
  NibbleRect nibble_rect(buffer, 5, 8);
  nibble_rect.fillRect(Box(1, 6, 9, 7), 1);
  nibble_rect.fillRect(Box(2, 0, 5, 6), 13);
  nibble_rect.fillRect(Box(1, 2, 3, 4), 3);
  nibble_rect.fillRect(Box(7, 7, 7, 7), 7);
  EXPECT_THAT(raster, MatchesContent(Grayscale4(), 10, 8,
                                     "  DDDD    "
                                     "  DDDD    "
                                     " 333DD    "
                                     " 333DD    "
                                     " 333DD    "
                                     "  DDDD    "
                                     " 1DDDD1111"
                                     " 111111711"));
}

void initNibbleRect(NibbleRect* rect, const char* input) {
  OffscreenDisplay<Grayscale4> offscreen(rect->width(), rect->height(),
                                         rect->buffer());
  {
    DrawingContext dc(&offscreen);
    dc.draw(
        MakeTestStreamable(Grayscale4(), rect->width(), rect->height(), input));
  }
}

// Helper to be able to draw nibble rectangle windows as if they were
// Grayscale4 streams.
class NibbleRectWindowIteratorRawStream {
 public:
  typedef const uint8_t* ptr_type;
  typedef Grayscale4 ColorMode;

  NibbleRectWindowIteratorRawStream(const NibbleRect* rect, const Box& window)
      : itr_(rect, window.xMin(), window.yMin(), window.xMax(), window.yMax()) {
  }

  Color next() { return Grayscale4().toArgbColor(itr_.next()); }

  TransparencyMode transparency() const { return TRANSPARENCY_NONE; }

 private:
  internal::NibbleRectWindowIterator itr_;
};

class NibbleRectWindowStreamable {
 public:
  NibbleRectWindowStreamable(const NibbleRect* rect, const Box& window)
      : rect_(rect), window_(window) {}

  const Box& extents() const { return window_; }
  std::unique_ptr<NibbleRectWindowIteratorRawStream> CreateRawStream() const {
    return std::unique_ptr<NibbleRectWindowIteratorRawStream>(
        new NibbleRectWindowIteratorRawStream(rect_, window_));
  }

  Grayscale4 color_mode() const { return Grayscale4(); }

 private:
  const NibbleRect* rect_;
  Box window_;
};

TEST(NibbleRect, WindowIteratorFull) {
  uint8_t buffer[40];
  NibbleRect nibble_rect(buffer, 5, 8);
  initNibbleRect(&nibble_rect,
                 "123456789A"
                 "BCDEF01234"
                 "56789ABCDE"
                 "F012345678"
                 "9ABCDEF012"
                 "3456789ABC"
                 "DEF0123456"
                 "789ABCDEF0");
  EXPECT_THAT(NibbleRectWindowStreamable(&nibble_rect, Box(0, 0, 9, 7)),
              MatchesContent(Grayscale4(), 10, 8,
                             "123456789A"
                             "BCDEF01234"
                             "56789ABCDE"
                             "F012345678"
                             "9ABCDEF012"
                             "3456789ABC"
                             "DEF0123456"
                             "789ABCDEF0"));
}

TEST(NibbleRect, WindowIteratorTopLeftOffset) {
  uint8_t buffer[40];
  NibbleRect nibble_rect(buffer, 5, 8);
  initNibbleRect(&nibble_rect,
                 "123456789A"
                 "BCDEF01234"
                 "56789ABCDE"
                 "F012345678"
                 "9ABCDEF012"
                 "3456789ABC"
                 "DEF0123456"
                 "789ABCDEF0");
  EXPECT_THAT(NibbleRectWindowStreamable(&nibble_rect, Box(1, 2, 9, 7)),
              MatchesContent(Grayscale4(), 9, 6,
                             "6789ABCDE"
                             "012345678"
                             "ABCDEF012"
                             "456789ABC"
                             "EF0123456"
                             "89ABCDEF0"));
}

TEST(NibbleRect, WindowIteratorMiddle) {
  uint8_t buffer[40];
  NibbleRect nibble_rect(buffer, 5, 8);
  initNibbleRect(&nibble_rect,
                 "123456789A"
                 "BCDEF01234"
                 "56789ABCDE"
                 "F012345678"
                 "9ABCDEF012"
                 "3456789ABC"
                 "DEF0123456"
                 "789ABCDEF0");
  EXPECT_THAT(NibbleRectWindowStreamable(&nibble_rect, Box(2, 3, 6, 5)),
              MatchesContent(Grayscale4(), 5, 3,
                             "12345"
                             "BCDEF"
                             "56789"));
}

TEST(NibbleRect, WindowIteratorSinglePixel) {
  uint8_t buffer[40];
  NibbleRect nibble_rect(buffer, 5, 8);
  initNibbleRect(&nibble_rect,
                 "123456789A"
                 "BCDEF01234"
                 "56789ABCDE"
                 "F012345678"
                 "9ABCDEF012"
                 "3456789ABC"
                 "DEF0123456"
                 "789ABCDEF0");
  EXPECT_THAT(NibbleRectWindowStreamable(&nibble_rect, Box(7, 7, 7, 7)),
              MatchesContent(Grayscale4(), 1, 1, "E"));
}

}  // namespace internal
}  // namespace roo_display
