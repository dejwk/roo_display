
#include "roo_display/driver/common/addr_window_device.h"

#include <memory>
#include <random>

#include "gtest/gtest-param-test.h"
#include "roo_io/data/byte_order.h"
#include "testing_display_device.h"

using namespace testing;

namespace roo_display {

template <typename ColorModeP, ByteOrder byte_order_p>
class TestTarget {
 public:
  typedef ColorModeP ColorMode;
  static constexpr ByteOrder byte_order = byte_order_p;

  TestTarget(int16_t width, int16_t height, Color bg)
      : width_(width),
        height_(height),
        data_(new Color[width * height]),
        orientation_(Orientation::Default()),
        xMin_(-1),
        yMin_(-1),
        xMax_(-1),
        yMax_(-1),
        xCursor_(-1),
        yCursor_(-1),
        initialized_(false),
        inTransaction_(false),
        inRamWrite_(false) {
    ColorMode mode;
    bg = mode.toArgbColor(mode.fromArgbColor(bg));
    std::fill(&data_[0], &data_[width * height], bg);
  }

  int16_t width() const { return width_; }
  int16_t height() const { return height_; }

  void init() { initialized_ = true; }

  void begin() {
    ASSERT_FALSE(inTransaction_);
    inTransaction_ = true;
  }

  void end() {
    ASSERT_TRUE(inTransaction_);
    inTransaction_ = false;
  }

  void setAddrWindow(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    xMin_ = x0;
    yMin_ = y0;
    xMax_ = x1;
    yMax_ = y1;
    xCursor_ = xMin_;
    yCursor_ = yMin_;
    inRamWrite_ = true;
  }

  void setOrientation(Orientation orientation) { orientation_ = orientation; }

  void ramWrite(ColorStorageType<ColorMode>* raw_color, size_t count) {
    EXPECT_TRUE(inRamWrite_);
    ColorMode color_mode;
    while (count-- > 0) {
      Color color = color_mode.toArgbColor(
          roo_io::toh<ColorStorageType<ColorMode>, byte_order>(*raw_color++));
      setPixel(xCursor_, yCursor_, color);
      xCursor_++;
      if (xCursor_ > xMax_) {
        xCursor_ = xMin_;
        yCursor_++;
      }
    }
  }

  void ramFill(ColorStorageType<ColorMode> raw_color, size_t count) {
    EXPECT_TRUE(inRamWrite_);
    ColorMode color_mode;
    Color color = color_mode.toArgbColor(
        roo_io::toh<ColorStorageType<ColorMode>, byte_order>(raw_color));
    while (count-- > 0) {
      setPixel(xCursor_, yCursor_, color);
      xCursor_++;
      if (xCursor_ > xMax_) {
        xCursor_ = xMin_;
        yCursor_++;
      }
    }
  }

  const Color* data() const { return data_.get(); }

 private:
  void setPixel(int16_t x, int16_t y, Color color) {
    ASSERT_GE(x, 0);
    ASSERT_GE(y, 0);
    if (orientation_.isXYswapped()) {
      std::swap(x, y);
    }
    ASSERT_LT(x, width());
    ASSERT_LT(y, height());
    if (orientation_.isRightToLeft()) x = width() - x - 1;
    if (orientation_.isBottomToTop()) y = height() - y - 1;
    data_[x + y * width()] = color;
  }

  int16_t width_, height_;
  std::unique_ptr<Color[]> data_;
  Orientation orientation_;

  int16_t xMin_, yMin_, xMax_, yMax_;
  int16_t xCursor_, yCursor_;

  bool initialized_;
  bool inTransaction_;
  bool inRamWrite_;
};

template <typename ColorMode, ByteOrder byte_order>
class TestDevice : public AddrWindowDevice<TestTarget<ColorMode, byte_order>> {
 public:
  typedef AddrWindowDevice<TestTarget<ColorMode, byte_order>> Base;

  TestDevice(int16_t width, int16_t height, Color color)
      : Base(TestTarget<ColorMode, byte_order>(width, height, color)) {
    Base::init();
  }

  const Color* data() const { return Base::target_.data(); }
};

template <typename ColorMode, ByteOrder byte_order>
const TestColorStreamable<ColorMode> RasterOf(
    const TestDevice<ColorMode, byte_order>& device) {
  return TestColorStreamable<ColorMode>(device.raw_width(), device.raw_height(),
                                        device.data());
}

typedef TestDevice<Rgb565, roo_io::kBigEndian> Rgb565Device;

class AddrWindowDeviceTest
    : public testing::TestWithParam<std::tuple<BlendingMode, Orientation>> {};

TEST_P(AddrWindowDeviceTest, FillRects) {
  TestFillRects<Rgb565Device, FakeOffscreen<Rgb565>>(std::get<0>(GetParam()),
                                                     std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, FillHLines) {
  TestFillHLines<Rgb565Device, FakeOffscreen<Rgb565>>(std::get<0>(GetParam()),
                                                      std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, FillVLines) {
  TestFillVLines<Rgb565Device, FakeOffscreen<Rgb565>>(std::get<0>(GetParam()),
                                                      std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, FillDegeneratePixels) {
  TestFillDegeneratePixels<Rgb565Device, FakeOffscreen<Rgb565>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, FillPixels) {
  TestFillPixels<Rgb565Device, FakeOffscreen<Rgb565>>(std::get<0>(GetParam()),
                                                      std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, WriteRects) {
  TestWriteRects<Rgb565Device, FakeOffscreen<Rgb565>>(std::get<0>(GetParam()),
                                                      std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, WriteHLines) {
  TestWriteHLines<Rgb565Device, FakeOffscreen<Rgb565>>(std::get<0>(GetParam()),
                                                       std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, WriteVLines) {
  TestWriteVLines<Rgb565Device, FakeOffscreen<Rgb565>>(std::get<0>(GetParam()),
                                                       std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, WriteDegeneratePixels) {
  TestWriteDegeneratePixels<Rgb565Device, FakeOffscreen<Rgb565>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, WritePixels) {
  TestWritePixels<Rgb565Device, FakeOffscreen<Rgb565>>(std::get<0>(GetParam()),
                                                       std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, WritePixelsStress) {
  TestWritePixelsStress<Rgb565Device, FakeOffscreen<Rgb565>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, WritePixelsSnake) {
  TestWritePixelsSnake<Rgb565Device, FakeOffscreen<Rgb565>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, WriteRectWindowSimple) {
  TestWriteRectWindowSimple<Rgb565Device, FakeOffscreen<Rgb565>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, WriteRectWindowStress) {
  TestWriteRectWindowStress<Rgb565Device, FakeOffscreen<Rgb565>>(
      std::get<0>(GetParam()), std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, WriteRectWindowGrayscale8) {
  TestWriteRectWindowSimple<TestDevice<Grayscale8, roo_io::kBigEndian>,
                            FakeOffscreen<Grayscale8>>(std::get<0>(GetParam()),
                                                       std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, WriteRectWindowSimpleLE) {
  TestWriteRectWindowSimple<TestDevice<Rgb565, roo_io::kLittleEndian>,
                            FakeOffscreen<Rgb565>>(std::get<0>(GetParam()),
                                                   std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, WriteRectWindowSimpleArgb4444LE) {
  TestWriteRectWindowSimple<TestDevice<Argb4444, roo_io::kLittleEndian>,
                            FakeOffscreen<Argb4444>>(std::get<0>(GetParam()),
                                                     std::get<1>(GetParam()));
}

TEST_P(AddrWindowDeviceTest, WriteRectWindowSimpleArgb8888LE) {
  TestWriteRectWindowSimple<TestDevice<Argb8888, roo_io::kLittleEndian>,
                            FakeOffscreen<Argb8888>>(std::get<0>(GetParam()),
                                                     std::get<1>(GetParam()));
}

INSTANTIATE_TEST_CASE_P(
    AddrWindowDeviceTests, AddrWindowDeviceTest,
    testing::Combine(
        testing::Values(BLENDING_MODE_SOURCE),
        testing::Values(Orientation::RightDown(), Orientation::DownRight(),
                        Orientation::LeftDown(), Orientation::DownLeft(),
                        Orientation::RightUp(), Orientation::UpRight(),
                        Orientation::LeftUp(), Orientation::UpLeft())));

}  // namespace roo_display
