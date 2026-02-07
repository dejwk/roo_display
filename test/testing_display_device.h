
#include <memory>
#include <random>

#include "gtest/gtest-param-test.h"
#include "roo_display/color/color.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/internal/color_io.h"
#include "testing.h"

using namespace testing;

static std::default_random_engine generator;

namespace roo_display {

// Helper class to test behavior of arbitrary display devices, including
// offscreen, base classes of real device drivers, and various filtering
// devices. Instantiates both the device under test, and a 'reference device'
// which is meant to be a trivially implemented 'reference' implementation, 'too
// simple to fail'. The test device sends identical commands to both the test
// and reference devices, and, in the end, compares results for identity.
template <typename TestedDevice, typename ReferenceDevice>
class TestDisplayDevice : public DisplayDevice {
 public:
  TestDisplayDevice(int16_t width, int16_t height,
                    Color fill = color::Transparent)
      : DisplayDevice(width, height),
        refc_(width, height, fill),
        test_(width, height, fill) {}

  void orientationUpdated() override {
    refc_.setOrientation(orientation());
    test_.setOrientation(orientation());
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    refc_.setAddress(x0, y0, x1, y1, mode);
    test_.setAddress(x0, y0, x1, y1, mode);
  }

  void write(Color* color, uint32_t pixel_count) override {
    refc_.write(color, pixel_count);
    test_.write(color, pixel_count);
  }

  void writePixels(BlendingMode mode, Color* color, int16_t* x, int16_t* y,
                   uint16_t pixel_count) override {
    refc_.writePixels(mode, color, x, y, pixel_count);
    test_.writePixels(mode, color, x, y, pixel_count);
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* x, int16_t* y,
                  uint16_t pixel_count) override {
    refc_.fillPixels(mode, color, x, y, pixel_count);
    test_.fillPixels(mode, color, x, y, pixel_count);
  }

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    refc_.writeRects(mode, color, x0, y0, x1, y1, count);
    test_.writeRects(mode, color, x0, y0, x1, y1, count);
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    refc_.fillRects(mode, color, x0, y0, x1, y1, count);
    test_.fillRects(mode, color, x0, y0, x1, y1, count);
  }

  const ReferenceDevice& refc() const { return refc_; };
  const TestedDevice& test() const { return test_; }

 private:
  ReferenceDevice refc_;
  TestedDevice test_;
};

#define EXPECT_CONSISTENT(screen) \
  EXPECT_THAT(RasterOf(screen.test()), MatchesContent(RasterOf(screen.refc())))

// 'Fill' tests.

template <typename TestedDevice, typename ReferenceDevice>
void TestFillRects(BlendingMode blending_mode, Orientation orientation) {
  TestDisplayDevice<TestedDevice, ReferenceDevice> screen(32, 35,
                                                          Color(0xFF101050));
  screen.setOrientation(orientation);
  int16_t x0[] = {4, 14, 7};
  int16_t y0[] = {14, 1, 12};
  int16_t x1[] = {12, 31, 8};
  int16_t y1[] = {18, 14, 30};
  screen.fillRects(blending_mode, Color(0x77145456), x0, y0, x1, y1, 3);
  EXPECT_CONSISTENT(screen);
}

template <typename TestedDevice, typename ReferenceDevice>
void TestFillHLines(BlendingMode blending_mode, Orientation orientation) {
  TestDisplayDevice<TestedDevice, ReferenceDevice> screen(37, 36,
                                                          Color(0xFF101050));
  screen.setOrientation(orientation);
  int16_t x0[] = {4, 14, 7};
  int16_t x1[] = {12, 31, 8};
  int16_t y[] = {14, 1, 12};
  screen.fillRects(blending_mode, Color(0xF27445A6), x0, y, x1, y, 3);
  EXPECT_CONSISTENT(screen);
}

template <typename TestedDevice, typename ReferenceDevice>
void TestFillVLines(BlendingMode blending_mode, Orientation orientation) {
  TestDisplayDevice<TestedDevice, ReferenceDevice> screen(31, 35,
                                                          Color(0xFF101050));
  screen.setOrientation(orientation);
  int16_t x[] = {4, 14, 7};
  int16_t y0[] = {14, 1, 12};
  int16_t y1[] = {18, 1, 30};
  screen.fillRects(blending_mode, Color(0xDD991133), x, y0, x, y1, 3);
  EXPECT_CONSISTENT(screen);
}

template <typename TestedDevice, typename ReferenceDevice>
void TestFillDegeneratePixels(BlendingMode blending_mode,
                              Orientation orientation) {
  TestDisplayDevice<TestedDevice, ReferenceDevice> screen(30, 33,
                                                          Color(0xFF101050));
  screen.setOrientation(orientation);
  int16_t x[] = {4, 14, 7};
  int16_t y[] = {14, 1, 12};
  screen.fillRects(blending_mode, Color(0xDD991133), x, y, x, y, 3);
  EXPECT_CONSISTENT(screen);
}

template <typename TestedDevice, typename ColorMode>
void TestFillPixels(BlendingMode blending_mode, Orientation orientation) {
  TestDisplayDevice<TestedDevice, ColorMode> screen(34, 30, Color(0xFF101050));
  screen.setOrientation(orientation);
  int16_t x[] = {4, 14, 7};
  int16_t y[] = {14, 1, 12};
  screen.fillPixels(blending_mode, Color(0xDD991133), x, y, 3);
  EXPECT_CONSISTENT(screen);
}

// 'Write' tests.

template <typename TestedDevice, typename ReferenceDevice>
void TestWriteRects(BlendingMode blending_mode, Orientation orientation) {
  TestDisplayDevice<TestedDevice, ReferenceDevice> screen(35, 32,
                                                          Color(0xFF101050));
  screen.setOrientation(orientation);
  int16_t x0[] = {4, 14, 7};
  int16_t y0[] = {14, 1, 12};
  int16_t x1[] = {12, 31, 8};
  int16_t y1[] = {18, 14, 30};
  Color c[] = {Color(0x77145456), Color(0xF27445AE), Color(0xDD991133)};
  screen.writeRects(blending_mode, c, x0, y0, x1, y1, 3);
  EXPECT_CONSISTENT(screen);
}

template <typename TestedDevice, typename ReferenceDevice>
void TestWriteHLines(BlendingMode blending_mode, Orientation orientation) {
  TestDisplayDevice<TestedDevice, ReferenceDevice> screen(35, 27,
                                                          Color(0xFF101050));
  screen.setOrientation(orientation);
  int16_t x0[] = {4, 14, 7};
  int16_t x1[] = {12, 21, 8};
  int16_t y[] = {14, 1, 12};
  Color c[] = {Color(0x77145456), Color(0xF27445AE), Color(0xDD991133)};
  screen.writeRects(blending_mode, c, x0, y, x1, y, 3);
  EXPECT_CONSISTENT(screen);
}

template <typename TestedDevice, typename ReferenceDevice>
void TestWriteVLines(BlendingMode blending_mode, Orientation orientation) {
  TestDisplayDevice<TestedDevice, ReferenceDevice> screen(36, 32,
                                                          Color(0xFF101050));
  screen.setOrientation(orientation);
  int16_t x[] = {4, 14, 7};
  int16_t y0[] = {14, 1, 12};
  int16_t y1[] = {18, 1, 30};
  Color c[] = {Color(0x77145456), Color(0xF27445AE), Color(0xDD991133)};
  screen.writeRects(blending_mode, c, x, y0, x, y1, 3);
  EXPECT_CONSISTENT(screen);
}

template <typename TestedDevice, typename ReferenceDevice>
void TestWriteDegeneratePixels(BlendingMode blending_mode,
                               Orientation orientation) {
  TestDisplayDevice<TestedDevice, ReferenceDevice> screen(32, 36,
                                                          Color(0xFF101050));
  screen.setOrientation(orientation);
  int16_t x[] = {4, 14, 7};
  int16_t y[] = {14, 1, 12};
  Color c[] = {Color(0x77145456), Color(0xF27445AE), Color(0xDD991133)};
  screen.writeRects(blending_mode, c, x, y, x, y, 3);
  EXPECT_CONSISTENT(screen);
}

template <typename TestedDevice, typename ReferenceDevice>
void TestWritePixels(BlendingMode blending_mode, Orientation orientation) {
  TestDisplayDevice<TestedDevice, ReferenceDevice> screen(32, 18,
                                                          Color(0xFF101050));
  screen.setOrientation(orientation);
  int16_t x[] = {4, 14, 7};
  int16_t y[] = {14, 1, 12};
  Color c[] = {Color(0x77145456), Color(0xF27445AE), Color(0xDD991133)};
  screen.writePixels(blending_mode, c, x, y, 3);
  EXPECT_CONSISTENT(screen);
}

template <typename TestedDevice, typename ReferenceDevice>
void TestWritePixelsStress(BlendingMode blending_mode,
                           Orientation orientation) {
  TestDisplayDevice<TestedDevice, ReferenceDevice> screen(50, 90,
                                                          Color(0x12345678));
  screen.setOrientation(orientation);

  std::uniform_int_distribution<
      ColorStorageType<ColorModeOfDevice<ReferenceDevice>>>
      color_distribution;
  std::uniform_int_distribution<uint16_t> len_distribution(1, 128);
  std::uniform_int_distribution<uint16_t> x_distribution(
      0, screen.effective_width() - 1);
  std::uniform_int_distribution<uint16_t> y_distribution(
      0, screen.effective_height() - 1);
  const size_t kTestSize = 1024;
  int16_t x[kTestSize];
  int16_t y[kTestSize];
  Color color[kTestSize];
  ColorModeOfDevice<ReferenceDevice> color_mode;
  for (size_t i = 0; i < kTestSize; i++) {
    x[i] = x_distribution(generator);
    y[i] = y_distribution(generator);
    color[i] = color_mode.toArgbColor(color_distribution(generator));
  }
  size_t start, end = 0;
  while (end < kTestSize) {
    uint32_t batch = len_distribution(generator);
    start = end;
    end += batch;
    if (end > kTestSize) end = kTestSize;
    screen.writePixels(blending_mode, color + start, x + start, y + start,
                       end - start);
  }
  EXPECT_CONSISTENT(screen);
}

template <typename TestedDevice, typename ReferenceDevice>
void TestWritePixelsSnake(BlendingMode blending_mode, Orientation orientation) {
  enum WriteDirection { RIGHT = 0, DOWN = 1, LEFT = 2, UP = 3 };

  TestDisplayDevice<TestedDevice, ReferenceDevice> screen(50, 90,
                                                          Color(0x12345678));
  screen.setOrientation(orientation);

  std::uniform_int_distribution<
      ColorStorageType<ColorModeOfDevice<ReferenceDevice>>>
      color_distribution;
  std::uniform_int_distribution<uint16_t> len_distribution(1, 128);
  std::uniform_int_distribution<uint16_t> segment_distribution(1, 128);
  std::uniform_int_distribution<uint16_t> x_distribution(
      0, screen.effective_width() - 1);
  std::uniform_int_distribution<uint16_t> y_distribution(
      0, screen.effective_height() - 1);
  int16_t x = x_distribution(generator);
  int16_t y = y_distribution(generator);

  std::uniform_int_distribution<int> direction_distribution(0, 3);
  const size_t kTestSize = 8192;
  int16_t xs[kTestSize];
  int16_t ys[kTestSize];
  Color color[kTestSize];
  ColorModeOfDevice<ReferenceDevice> color_mode;
  int16_t segment_remaining = 0;
  int direction = 0;
  for (size_t i = 0; i < kTestSize; i++) {
    color[i] = color_mode.toArgbColor(color_distribution(generator));
    while (segment_remaining == 0) {
      // Generate a new random segment
      segment_remaining = segment_distribution(generator);
      direction = direction_distribution(generator);
      switch (direction) {
        case UP: {
          segment_remaining = std::min(segment_remaining, y);
          break;
        }
        case DOWN: {
          segment_remaining = std::min<int16_t>(
              segment_remaining, screen.effective_height() - y - 1);
          break;
        }
        case LEFT: {
          segment_remaining = std::min(segment_remaining, x);
          break;
        }
        case RIGHT: {
          segment_remaining = std::min<int16_t>(
              segment_remaining, screen.effective_width() - x - 1);
        }
      }
    }
    switch (direction) {
      case UP: {
        y--;
        break;
      }
      case DOWN: {
        y++;
        break;
      }
      case LEFT: {
        x--;
        break;
      }
      case RIGHT: {
        x++;
      }
    }
    segment_remaining--;
    xs[i] = x;
    ys[i] = y;
  }

  size_t start, end = 0;
  while (end < kTestSize) {
    uint32_t batch = len_distribution(generator);
    start = end;
    end += batch;
    if (end > kTestSize) end = kTestSize;
    screen.writePixels(blending_mode, color + start, xs + start, ys + start,
                       end - start);
  }
  EXPECT_CONSISTENT(screen);
}

template <typename TestedDevice, typename ReferenceDevice>
void fillRandom(TestDisplayDevice<TestedDevice, ReferenceDevice>* screen,
                BlendingMode mode, int16_t x0, int16_t y0, int16_t x1,
                int16_t y1) {
  std::uniform_int_distribution<
      ColorStorageType<ColorModeOfDevice<ReferenceDevice>>>
      color_distribution;
  std::uniform_int_distribution<uint16_t> len_distribution(1, 128);
  screen->setAddress(x0, y0, x1, y1, mode);
  uint32_t remaining = (x1 - x0 + 1) * (y1 - y0 + 1);
  Color colors[128];
  ColorModeOfDevice<ReferenceDevice> color_mode;
  for (int i = 0; i < 128; ++i) {
    colors[i] = color_mode.toArgbColor(color_distribution(generator));
  }
  while (remaining > 0) {
    uint32_t batch = len_distribution(generator);
    if (batch > remaining) batch = remaining;
    screen->write(colors, batch);
    remaining -= batch;
  }
}

template <typename TestedDevice, typename ReferenceDevice>
void TestWriteRectWindowSimple(BlendingMode blending_mode,
                               Orientation orientation) {
  TestDisplayDevice<TestedDevice, ReferenceDevice> screen(8, 12,
                                                          Color(0x00000000));
  screen.setOrientation(orientation);
  fillRandom(&screen, blending_mode, 2, 3, 6, 7);
  EXPECT_CONSISTENT(screen);
}

template <typename TestedDevice, typename ReferenceDevice>
void TestWriteRectWindowStress(BlendingMode blending_mode,
                               Orientation orientation) {
  TestDisplayDevice<TestedDevice, ReferenceDevice> screen(50, 90,
                                                          Color(0x12345678));
  screen.setOrientation(orientation);
  std::uniform_int_distribution<uint16_t> x_distribution(
      0, screen.effective_width() - 1);
  std::uniform_int_distribution<uint16_t> y_distribution(
      0, screen.effective_height() - 1);
  for (int i = 0; i < 200; i++) {
    int16_t x0 = x_distribution(generator);
    int16_t x1 = x_distribution(generator);
    if (x1 < x0) std::swap(x0, x1);
    int16_t y0 = y_distribution(generator);
    int16_t y1 = y_distribution(generator);
    if (y1 < y0) std::swap(y0, y1);
    fillRandom(&screen, blending_mode, x0, y0, x1, y1);
  }
  EXPECT_CONSISTENT(screen);
}

std::ostream& operator<<(std::ostream& os,
                         const std::tuple<BlendingMode, Orientation>& pair) {
  os << std::get<0>(pair) << ", " << std::get<1>(pair);
  return os;
}

}  // namespace roo_display
