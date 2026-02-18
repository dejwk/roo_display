// Background fill optimizer benchmark for roo_display.

#include <algorithm>
#include <array>
#include <string>

#include "Arduino.h"

#ifdef ROO_TESTING

#include "roo_testing/devices/display/ili9486/ili9486spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"

using roo_testing_transducers::FlexViewport;
using roo_testing_transducers::FltkViewport;

struct Emulator {
  FltkViewport viewport;
  FlexViewport flexViewport;

  FakeIli9486Spi display;

  Emulator() : viewport(), flexViewport(viewport, 6), display(flexViewport) {
    FakeEsp32().attachSpiDevice(display, 18, 19, 23);
    FakeEsp32().gpio.attachOutput(5, display.cs());
    FakeEsp32().gpio.attachOutput(17, display.dc());
    FakeEsp32().gpio.attachOutput(27, display.rst());
  }
} emulator;

#endif

#include "roo_display.h"
#include "roo_display/filter/background_fill_optimizer.h"
#include "roo_display/shape/basic.h"
#include "roo_display/ui/text_label.h"
#include "roo_display/ui/tile.h"
#include "roo_fonts/NotoSans_Regular/27.h"

using namespace roo_display;

#include "roo_display/driver/ili9486.h"
Ili9486spi<5, 17, 27> device;

BackgroundFillOptimizerDevice optimized_device(device);
Display baseline_display(device);
Display optimized_display(optimized_device);

namespace {

constexpr int kRandomRectIterations = 3000;
constexpr int kTileHeight = 40;
constexpr int kRowCount = 40;

const std::array<Color, 4> kRowBackgrounds = {
    Color(255, 255, 255),
    Color(208, 232, 255),
    Color(255, 246, 200),
    Color(255, 221, 232),
};

struct Rng {
  uint32_t state;

  uint32_t next() {
    state = state * 1664525u + 1013904223u;
    return state;
  }

  uint8_t nextByte() { return static_cast<uint8_t>(next() >> 24); }

  int16_t nextRange(int16_t max_value) {
    if (max_value <= 0) return 0;
    return static_cast<int16_t>(next() % (static_cast<uint32_t>(max_value) + 1));
  }
};

Color randomColor(Rng& rng) {
  return Color(rng.nextByte(), rng.nextByte(), rng.nextByte());
}

Color rowBackground(int row_idx) {
  if (row_idx < 10) return kRowBackgrounds[0];
  if (row_idx < 20) return kRowBackgrounds[1];
  if (row_idx < 30) return kRowBackgrounds[2];
  return kRowBackgrounds[3];
}

unsigned long benchmarkRandomRectangles(Display& display) {
  const int16_t w = display.width();
  const int16_t h = display.height();

  {
    DrawingContext dc(display);
    dc.clear();
  }

  Rng rng{0xBADC0FFEu};
  unsigned long start = micros();
  {
    DrawingContext dc(display);
    for (int i = 0; i < kRandomRectIterations; ++i) {
      int16_t x0 = rng.nextRange(w - 1);
      int16_t x1 = rng.nextRange(w - 1);
      int16_t y0 = rng.nextRange(h - 1);
      int16_t y1 = rng.nextRange(h - 1);
      if (x1 < x0) std::swap(x0, x1);
      if (y1 < y0) std::swap(y0, y1);
      dc.draw(FilledRect(x0, y0, x1, y1, randomColor(rng)));
    }
  }
  return micros() - start;
}

void drawScrollFrame(DrawingContext& dc, int16_t width, int16_t height,
                     int16_t y_offset,
                     const std::array<std::string, kRowCount>& lines) {
  for (int row = 0; row < kRowCount; ++row) {
    const int16_t y = y_offset + row * kTileHeight;
    if (y > height - 1 || y + kTileHeight - 1 < 0) continue;

    auto label = StringViewLabel(lines[row], font_NotoSans_Regular_27(), color::Black);
    auto tile = MakeTileOf(label, Box(0, 0, width - 1, kTileHeight - 1),
                           kCenter | kMiddle, rowBackground(row));
    dc.draw(tile, 0, y);
  }
}

unsigned long benchmarkTextScroll(Display& display) {
  const int16_t w = display.width();
  const int16_t h = display.height();

  std::array<std::string, kRowCount> lines;
  for (int i = 0; i < kRowCount; ++i) {
    lines[i] = "Test Line ";
    lines[i] += std::to_string(i + 1);
  }

  const int16_t start_offset = h;
  const int16_t end_offset = -kTileHeight * kRowCount;

  unsigned long start = micros();
  for (int16_t offset = start_offset; offset >= end_offset; --offset) {
    DrawingContext dc(display);
    drawScrollFrame(dc, w, h, offset, lines);
  }
  return micros() - start;
}

void runCondition(const char* name, Display& display) {
  unsigned long random_rects = benchmarkRandomRectangles(display);
  delay(200);
  unsigned long text_scroll = benchmarkTextScroll(display);
  delay(200);

  Serial.printf("%-20s %14lu %14lu\n", name, random_rects, text_scroll);
}

void runBenchmarks() {
  Serial.println("Background fill optimizer benchmark");
  Serial.println("---------------------------------------------------------------");
  Serial.println("Condition              random_rects     text_scroll");
  Serial.println("---------------------------------------------------------------");

  runCondition("baseline", baseline_display);

  optimized_device.setPalette(
      {kRowBackgrounds[0], kRowBackgrounds[1], kRowBackgrounds[2],
       kRowBackgrounds[3], color::White},
      color::White);
  runCondition("bg_fill_optimizer", optimized_display);

  Serial.println("Done!");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  SPI.begin();

  baseline_display.init(color::White);
  optimized_display.init(color::White);
}

void loop() {
  runBenchmarks();
  delay(2000);
}
