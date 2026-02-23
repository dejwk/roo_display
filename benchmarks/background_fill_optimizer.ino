// Background fill optimizer benchmark for roo_display.

#include <algorithm>
#include <array>
#include <random>
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
#include "roo_display/shape/basic.h"
#include "roo_display/ui/text_label.h"
#include "roo_display/ui/tile.h"
#include "roo_fonts/NotoSans_Regular/27.h"

using namespace roo_display;

#include "roo_display/driver/ili9486.h"
Ili9486spi<5, 17, 27> device;

Display display(device);

namespace {

constexpr int kRandomRectIterations = 500;
constexpr int kRandomPixelIterations = 80000;
constexpr int kRandomStreamFillRectIterations = 300;
constexpr int kAlignedSmallRectIterations = 50000;
constexpr int kTileHeight = 40;
constexpr int kRowCount = 20;

const std::array<Color, 4> kRowBackgrounds = {
    Color(255, 255, 255),
    Color(208, 232, 255),
    Color(255, 246, 200),
    Color(255, 221, 232),
};

const std::array<Color, 4> kSmallRectPalette = {
    Color(255, 255, 255),
    Color(208, 232, 255),
    Color(255, 246, 200),
    Color(255, 221, 232),
};

struct Rng {
  explicit Rng(uint64_t seed, uint64_t seq = 0xDA3E39CB94B95BDBull) : engine() {
    const uint32_t seed_values[] = {
        static_cast<uint32_t>(seed),
        static_cast<uint32_t>(seed >> 32),
        static_cast<uint32_t>(seq),
        static_cast<uint32_t>(seq >> 32),
        0x9E3779B9u,
        0x243F6A88u,
    };
    std::seed_seq seed_data(seed_values, seed_values + 6);
    engine.seed(seed_data);
  }

  uint32_t next() { return static_cast<uint32_t>(engine()); }

  uint32_t nextBounded(uint32_t bound) {
    if (bound == 0) return 0;
    std::uniform_int_distribution<uint32_t> dist(0, bound - 1);
    return dist(engine);
  }

  uint8_t nextByte() { return static_cast<uint8_t>(nextBounded(256u)); }

  int16_t nextRange(int16_t max_value) {
    if (max_value <= 0) return 0;
    return static_cast<int16_t>(
        nextBounded(static_cast<uint32_t>(max_value) + 1));
  }

 private:
  std::mt19937_64 engine;
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

class StreamFillRectDrawable : public Drawable {
 public:
  StreamFillRectDrawable(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                         Color color)
      : bounds_(x0, y0, x1, y1), color_(color) {}

  Box extents() const override { return bounds_; }

 private:
  void drawTo(const Surface& s) const override {
    DisplayOutput& out = s.out();
    out.setAddress(bounds_.xMin() + s.dx(), bounds_.yMin() + s.dy(),
                   bounds_.xMax() + s.dx(), bounds_.yMax() + s.dy(),
                   kBlendingSource);
    uint32_t total_pixels = bounds_.area();
    while (total_pixels > 0) {
      uint32_t batch_pixels = std::min<uint32_t>(total_pixels, 64u);
      out.fill(color_, batch_pixels);
      total_pixels -= batch_pixels;
    }
  }

  Box bounds_;
  Color color_;
};

class UniformWriteRectDrawable : public Drawable {
 public:
  UniformWriteRectDrawable(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                           Color color)
      : bounds_(x0, y0, x1, y1), color_(color) {}

  Box extents() const override { return bounds_; }

 private:
  void drawTo(const Surface& s) const override {
    DisplayOutput& out = s.out();
    size_t pixel_count = bounds_.area();
    std::array<Color, 64> pixels;
    std::fill_n(pixels.begin(), pixel_count, color_);
    out.setAddress(bounds_.xMin() + s.dx(), bounds_.yMin() + s.dy(),
                   bounds_.xMax() + s.dx(), bounds_.yMax() + s.dy(),
                   kBlendingSource);
    out.write(pixels.data(), pixel_count);
  }

  Box bounds_;
  Color color_;
  uint16_t pixel_count_;
};

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

unsigned long benchmarkRandomPixelsDrawPixels(Display& display) {
  const int16_t w = display.width();
  const int16_t h = display.height();

  {
    DrawingContext dc(display);
    dc.clear();
  }

  Rng rng{0xA11CE55u};
  unsigned long start = micros();
  {
    DrawingContext dc(display);
    dc.drawPixels([&](PixelWriter& writer) {
      for (int i = 0; i < kRandomPixelIterations; ++i) {
        writer.writePixel(rng.nextRange(w - 1), rng.nextRange(h - 1),
                          randomColor(rng));
      }
    });
  }
  return micros() - start;
}

unsigned long benchmarkRandomRectanglesStreamFill(Display& display) {
  const int16_t w = display.width();
  const int16_t h = display.height();

  {
    DrawingContext dc(display);
    dc.clear();
  }

  Rng rng{0xC001D00Du};

  unsigned long start = micros();
  {
    DrawingContext dc(display);
    for (int i = 0; i < kRandomStreamFillRectIterations; ++i) {
      int16_t x0 = rng.nextRange(w - 1);
      int16_t x1 = rng.nextRange(w - 1);
      int16_t y0 = rng.nextRange(h - 1);
      int16_t y1 = rng.nextRange(h - 1);
      if (x1 < x0) std::swap(x0, x1);
      if (y1 < y0) std::swap(y0, y1);
      Color color = randomColor(rng);

      StreamFillRectDrawable drawable(x0, y0, x1, y1, color);
      dc.draw(drawable);
    }
  }
  return micros() - start;
}

unsigned long benchmarkAlignedSmallRectsWrite(Display& display) {
  const int16_t w = display.width();
  const int16_t h = display.height();

  {
    DrawingContext dc(display);
    dc.clear();
  }

  Rng rng{0x51A11E7u};

  unsigned long start = micros();
  {
    DrawingContext dc(display);
    for (int i = 0; i < kAlignedSmallRectIterations; ++i) {
      int16_t rect_w;
      int16_t rect_h;
      do {
        rect_w = static_cast<int16_t>((1 + rng.nextBounded(4u)) * 4);
        rect_h = static_cast<int16_t>((1 + rng.nextBounded(4u)) * 4);
      } while (static_cast<uint32_t>(rect_w) * static_cast<uint32_t>(rect_h) >
               64u);

      const int16_t max_x_cells = (w - rect_w) / 4;
      const int16_t max_y_cells = (h - rect_h) / 4;
      const int16_t x0 = static_cast<int16_t>(4 * rng.nextRange(max_x_cells));
      const int16_t y0 = static_cast<int16_t>(4 * rng.nextRange(max_y_cells));
      const int16_t x1 = x0 + rect_w - 1;
      const int16_t y1 = y0 + rect_h - 1;

      const Color color = kSmallRectPalette[rng.nextBounded(4u)];

      UniformWriteRectDrawable drawable(x0, y0, x1, y1, color);
      dc.draw(drawable);
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

    auto label =
        StringViewLabel(lines[row], font_NotoSans_Regular_27(), color::Black);
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
  const int16_t end_offset = h - kTileHeight * kRowCount;

  unsigned long start = micros();
  for (int16_t offset = start_offset; offset >= end_offset; offset -= 12) {
    DrawingContext dc(display);
    drawScrollFrame(dc, w, h, offset, lines);
  }
  return micros() - start;
}

void runCondition(const char* name, Display& display) {
  unsigned long random_rects = benchmarkRandomRectangles(display);
  delay(200);
  unsigned long random_pixels = benchmarkRandomPixelsDrawPixels(display);
  delay(200);
  unsigned long random_stream_fill =
      benchmarkRandomRectanglesStreamFill(display);
  delay(200);
  unsigned long aligned_small_write = benchmarkAlignedSmallRectsWrite(display);
  delay(200);
  unsigned long text_scroll = benchmarkTextScroll(display);
  delay(200);

  Serial.printf("%-20s %14lu %14lu %14lu %14lu %14lu\n", name, random_rects,
                random_pixels, random_stream_fill, aligned_small_write,
                text_scroll);
}

void runBenchmarks() {
  Serial.println("Background fill optimizer benchmark");
  Serial.println(
      "------------------------------------------------------------------------"
      "---------------------");
  Serial.println(
      "Condition              random_rects  random_pixels   random_fill64 "
      "aligned_write4      text_scroll");
  Serial.println(
      "------------------------------------------------------------------------"
      "---------------------");

  runCondition("baseline", display);

  display.enableTurbo();
  runCondition("bg_fill_optimizer", display);
  display.disableTurbo();

  Serial.println("Done!");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  SPI.begin();

  display.init(color::White);
}

void loop() {
  runBenchmarks();
  delay(2000);
}
