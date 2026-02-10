// Benchmark for drawDirectRect via Offscreen rendering.

#include <algorithm>
#include <cstdint>

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
#include "roo_display/color/gradient.h"
#include "roo_display/color/hsv.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSerif_Italic/40.h"
#include "roo_fonts/NotoSerif_Italic/60.h"

using namespace roo_display;

// Change these two lines to use a different driver, transport, or pins.
#include "roo_display/driver/ili9486.h"
Ili9486spi<5, 17, 27> device;

Display display(device);

namespace {

constexpr int kIterationsClipped = 500;
constexpr int kIterationsFull = 50;
// Single deterministic seed used for all benchmark cases.
constexpr uint32_t kSeed = 0xA13F1234u;

struct SizeCase {
  const char* name;
  int16_t width;
  int16_t height;
};

struct RandState {
  uint32_t state;
  uint32_t next() {
    state = state * 1664525u + 1013904223u;
    return state;
  }

  int16_t nextRange(int16_t max_value) {
    if (max_value <= 0) return 0;
    return static_cast<int16_t>(next() % (max_value + 1));
  }

  int16_t nextRangeSigned(int16_t min_value, int16_t max_value) {
    if (max_value <= min_value) return min_value;
    uint32_t span = static_cast<uint32_t>(max_value - min_value + 1);
    return static_cast<int16_t>(min_value + (next() % span));
  }
};

const Font& SelectFont(int16_t width, int16_t height) {
  int16_t min_dim = std::min(width, height);
  if (min_dim >= 160) {
    return font_NotoSerif_Italic_60();
  }
  return font_NotoSerif_Italic_40();
}

Color PickGradientColor(int16_t width, int16_t height) {
  // Deterministic but pleasing background color derived from a fixed seed.
  RandState rng{kSeed};
  float hue = static_cast<float>(rng.next() % 360u);
  return HsvToRgb(hue, 0.7f, 0.95f);
}

template <typename OffscreenT>
void PopulateSource(OffscreenT& offscreen, int16_t width, int16_t height) {
  {
    DrawingContext dc(offscreen);
    int16_t cx = width / 2;
    int16_t cy = height / 2;
    // Choose radius squared so the radial gradient reaches all corners.
    int16_t rx = std::max<int16_t>(cx, width - 1 - cx);
    int16_t ry = std::max<int16_t>(cy, height - 1 - cy);
    float radius_sq = static_cast<float>(rx * rx + ry * ry);
    Color base = PickGradientColor(width, height);
    Color edge = HsvToRgb(0.0f, 0.0f, 0.12f);
    auto gradient = RadialGradientSq(
        {cx, cy},
        ColorGradient({{0, base}, {radius_sq, edge}}, ColorGradient::EXTENDED));
    dc.setBackground(&gradient);
    dc.clear();
    dc.draw(TextLabel("&", SelectFont(width, height), color::White),
            kCenter | kMiddle);
  }
}

int16_t pickFullOffset(int16_t max_value, RandState& rng) {
  int16_t value = rng.nextRange(max_value);
  return value;
}

int16_t pickPartialPosition(int16_t screen_size, int16_t size, RandState& rng) {
  // Ensure the image overlaps the screen by at least 1 pixel.
  int16_t min_pos = 1 - size;
  int16_t max_pos = screen_size - 1;
  if (min_pos > max_pos) min_pos = max_pos;
  return rng.nextRangeSigned(min_pos, max_pos);
}

Box pickClipBox(int16_t screen_w, int16_t screen_h, const Box& image,
                RandState& rng) {
  const int16_t max_x = screen_w - 1;
  const int16_t max_y = screen_h - 1;
  const Box screen(0, 0, max_x, max_y);
  for (int attempt = 0; attempt < 8; ++attempt) {
    int16_t x0 = rng.nextRange(max_x);
    int16_t x1 = rng.nextRangeSigned(x0, max_x);
    int16_t y0 = rng.nextRange(max_y);
    int16_t y1 = rng.nextRangeSigned(y0, max_y);
    Box clip(x0, y0, x1, y1);
    if (clip.intersects(image)) {
      return clip;
    }
  }
  return Box::Intersect(screen, image);
}

template <typename OffscreenT>
unsigned long runCase(Display& display, const OffscreenT& source,
                      int16_t src_width, int16_t src_height, bool partial,
                      int iterations) {
  int16_t screen_w = display.width();
  int16_t screen_h = display.height();
  int16_t max_x = screen_w - src_width;
  int16_t max_y = screen_h - src_height;
  if (max_x < 0) max_x = 0;
  if (max_y < 0) max_y = 0;

  RandState rng{kSeed};
  unsigned long start = micros();
  {
    DrawingContext dc(display);
    for (int i = 0; i < iterations; ++i) {
      int16_t dst_x = partial ? pickPartialPosition(screen_w, src_width, rng)
                              : pickFullOffset(max_x, rng);
      int16_t dst_y = partial ? pickPartialPosition(screen_h, src_height, rng)
                              : pickFullOffset(max_y, rng);
      if (partial) {
        Box image(dst_x, dst_y, dst_x + src_width - 1, dst_y + src_height - 1);
        Box clip = pickClipBox(screen_w, screen_h, image, rng);
        dc.setClipBox(clip);
      }
      dc.draw(source, dst_x, dst_y);
    }
  }
  return micros() - start;
}

void runBenchmarks() {
  const Orientation orientations[] = {Orientation::Default(),
                                      Orientation().rotateLeft()};
  const char* orientation_labels[] = {"natural", "rotated"};

  Serial.println("drawDirectRect benchmark (via Offscreen)");
  Serial.println(
      "------------------------------------------------------------------");
  Serial.println(
      "case                                     time (microseconds)");
  Serial.println(
      "------------------------------------------------------------------");

  for (int o = 0; o < 2; ++o) {
    display.setOrientation(orientations[o]);

    int16_t screen_w = display.width();
    int16_t screen_h = display.height();

    SizeCase sizes[] = {
        {"small", std::min<int16_t>(120, screen_w),
         std::min<int16_t>(60, screen_h)},
        {"medium", std::min<int16_t>(240, screen_w),
         std::min<int16_t>(140, screen_h)},
        {"large", screen_w, screen_h},
    };

    for (const auto& size : sizes) {
      auto source = OffscreenForDevice(
          device, Box(0, 0, size.width - 1, size.height - 1));
      PopulateSource(source, size.width, size.height);

      for (int partial = 0; partial < 2; ++partial) {
        const char* partial_label = partial ? "clipped" : "full";
        int iterations = partial ? kIterationsClipped : kIterationsFull;
        unsigned long elapsed = runCase(display, source, size.width,
                                        size.height, partial, iterations);

        Serial.printf("%-7s/%-6s/%-7s %12lu\n", orientation_labels[o],
                      size.name, partial_label, elapsed);
        delay(10);
        delay(200);
      }
    }
  }

  Serial.println("Done!");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  SPI.begin();
  display.init(color::Black);
}

void loop() {
  runBenchmarks();
  delay(1000);
}
