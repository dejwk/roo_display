#include <Arduino.h>

#include <random>
#include <vector>

#include "SPI.h"
#include "gtest/gtest.h"
#include "roo_display.h"
#include "roo_display/color/color_modes.h"
#include "roo_display/driver/ili9341.h"
#include "roo_display/internal/color_io.h"
#include "roo_display/shape/basic.h"
#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "testing_viewport.h"

using namespace roo_display;

namespace {

constexpr int kSpiSck = 18;
constexpr int kSpiMiso = 19;
constexpr int kSpiMosi = 23;
constexpr int kPinCs = 5;
constexpr int kPinDc = 2;
constexpr int kPinRst = 4;

uint32_t ToRgb565Argb(Color color) {
  // Round-trip through the device color mode to simulate on-device
  // truncation before comparing against emulator output.
  Rgb565 mode;
  return mode.toArgbColor(mode.fromArgbColor(color)).asArgb();
}

struct Ili9341Emulator {
  testing::TestViewport viewport;
  FakeIli9341Spi display;

  Ili9341Emulator() : viewport(), display(viewport) {
    FakeEsp32().attachSpiDevice(display, kSpiSck, kSpiMiso, kSpiMosi);
    FakeEsp32().gpio.attachOutput(kPinCs, display.cs());
    FakeEsp32().gpio.attachOutput(kPinDc, display.dc());
    FakeEsp32().gpio.attachOutput(kPinRst, display.rst());
  }
};

struct Ili9341TestRig {
  Ili9341Emulator emu;
  Ili9341spi<kPinCs, kPinDc, kPinRst> device;
  Display display;

  Ili9341TestRig() : emu(), device(), display(device) {
    SPI.begin();
    display.init(color::Black);
  }

  void Reset(Color bg) {
    display.init(bg);
    emu.viewport.clear(ToRgb565Argb(bg));
  }
};

Ili9341TestRig& GetIli9341TestRig() {
  static Ili9341TestRig rig;
  return rig;
}

}  // namespace

TEST(Ili9341Driver, DrawFilledRect) {
  auto& rig = GetIli9341TestRig();
  rig.Reset(color::Black);

  {
    DrawingContext dc(rig.display);
    dc.draw(FilledRect(10, 20, 12, 21, color::Red));
    dc.draw(FilledRect(2, 3, 2, 3, color::Gray));
    dc.draw(FilledRect(4, 5, 4, 5, color::DarkGray));
    dc.draw(FilledRect(6, 7, 6, 7, color::DeepSkyBlue));
  }

  FakeEsp32().flush();

  EXPECT_EQ(ToRgb565Argb(color::Red), rig.emu.viewport.getPixel(10, 20));
  EXPECT_EQ(ToRgb565Argb(color::Black), rig.emu.viewport.getPixel(0, 0));
  EXPECT_EQ(ToRgb565Argb(color::Gray), rig.emu.viewport.getPixel(2, 3));
  EXPECT_EQ(ToRgb565Argb(color::DarkGray), rig.emu.viewport.getPixel(4, 5));
  EXPECT_EQ(ToRgb565Argb(color::DeepSkyBlue), rig.emu.viewport.getPixel(6, 7));
}

TEST(Ili9341Driver, DrawDirectRect) {
  constexpr int16_t kScreenWidth = 240;
  constexpr int16_t kScreenHeight = 320;
  constexpr int16_t kDataWidth = kScreenWidth + 32;
  constexpr int16_t kDataHeight = kScreenHeight + 32;
  constexpr size_t kRowWidthBytes =
      static_cast<size_t>(kDataWidth) * sizeof(uint16_t);

  std::vector<roo::byte> data(kRowWidthBytes * kDataHeight);
  std::mt19937 rng(0x9341);
  std::uniform_int_distribution<uint16_t> dist(0, 0xFFFF);

  Rgb565 mode;
  ColorIo<Rgb565, roo_io::kBigEndian> io;
  for (int16_t y = 0; y < kDataHeight; ++y) {
    for (int16_t x = 0; x < kDataWidth; ++x) {
      uint16_t raw = dist(rng);
      Color color = mode.toArgbColor(raw);
      roo::byte* ptr = data.data() + static_cast<size_t>(y) * kRowWidthBytes +
                       static_cast<size_t>(x) * sizeof(uint16_t);
      io.store(color, ptr, mode);
    }
  }

  struct DirectRectCase {
    const char* name;
    int16_t src_x0;
    int16_t src_y0;
    int16_t src_x1;
    int16_t src_y1;
    int16_t dst_x0;
    int16_t dst_y0;
  };

  const std::vector<DirectRectCase> cases = {
      {"single_pixel", 3, 5, 3, 5, 7, 9},
      {"small_2x2", 10, 10, 11, 11, 20, 30},
      {"block_8x8", 16, 16, 23, 23, 0, 0},
      {"full_width", 0, 50, kScreenWidth - 1, 52, 0, 100},
      {"full_height", 5, 0, 7, kScreenHeight - 1, 100, 0},
      {"large_rect", 30, 40, 149, 239, 60, 70},
  };

  auto& rig = GetIli9341TestRig();
  rig.Reset(color::Black);

  for (const auto& test_case : cases) {
    rig.emu.viewport.clear(ToRgb565Argb(color::Black));

    std::vector<uint32_t> expected(
        static_cast<size_t>(kScreenWidth) * kScreenHeight,
        ToRgb565Argb(color::Black));

    auto set_expected = [&](int16_t x, int16_t y, uint32_t argb) {
      expected[static_cast<size_t>(x) + static_cast<size_t>(y) * kScreenWidth] =
          argb;
    };

    int16_t rect_w = test_case.src_x1 - test_case.src_x0 + 1;
    int16_t rect_h = test_case.src_y1 - test_case.src_y0 + 1;
    for (int16_t y = 0; y < rect_h; ++y) {
      for (int16_t x = 0; x < rect_w; ++x) {
        const roo::byte* ptr =
            data.data() +
            static_cast<size_t>(test_case.src_y0 + y) * kRowWidthBytes +
            static_cast<size_t>(test_case.src_x0 + x) * sizeof(uint16_t);
        Color color = io.load(ptr, mode);
        set_expected(test_case.dst_x0 + x, test_case.dst_y0 + y,
                     ToRgb565Argb(color));
      }
    }

    rig.display.output().begin();
    rig.display.output().drawDirectRect(
        data.data(), kRowWidthBytes, test_case.src_x0, test_case.src_y0,
        test_case.src_x1, test_case.src_y1, test_case.dst_x0, test_case.dst_y0);
    rig.display.output().end();

    FakeEsp32().flush();

    for (int16_t y = 0; y < kScreenHeight; ++y) {
      for (int16_t x = 0; x < kScreenWidth; ++x) {
        EXPECT_EQ(expected[static_cast<size_t>(x) +
                           static_cast<size_t>(y) * kScreenWidth],
                  rig.emu.viewport.getPixel(x, y))
            << test_case.name << " at (" << x << ", " << y << ")";
      }
    }
  }
}
