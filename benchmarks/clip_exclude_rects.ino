// Benchmark for RectUnionFilter partial-exclusion write/fill workloads.

#include <array>
#include <vector>

#include "Arduino.h"
#include "roo_display.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/filter/clip_exclude_rects.h"
#include "roo_display/ui/text_label.h"
#include "roo_fonts/NotoSans_Regular/12.h"
#include "roo_fonts/NotoSans_Regular/18.h"
#include "roo_fonts/NotoSans_Regular/27.h"

using namespace roo_display;

#ifndef ROO_DISPLAY_BENCHMARK_OFFSCREEN

#ifdef ROO_TESTING

#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/microcontrollers/esp32/fake_esp32.h"
#include "roo_testing/transducers/ui/viewport/flex_viewport.h"
#include "roo_testing/transducers/ui/viewport/fltk/fltk_viewport.h"

using roo_testing_transducers::FlexViewport;
using roo_testing_transducers::FltkViewport;

struct Emulator {
  FltkViewport viewport;
  FlexViewport flexViewport;

  FakeIli9341Spi display;
  FakeXpt2046Spi touch;

  Emulator()
      : viewport(),
        flexViewport(viewport, 1, FlexViewport::kRotationRight),
        display(flexViewport),
        touch(flexViewport, FakeXpt2046Spi::Calibration(269, 249, 3829, 3684,
                                                        true, false, false)) {
    FakeEsp32().attachSpiDevice(display, 4, 5, 6);
    FakeEsp32().gpio.attachOutput(7, display.cs());
    FakeEsp32().gpio.attachOutput(2, display.dc());
    FakeEsp32().gpio.attachOutput(3, display.rst());
    FakeEsp32().attachSpiDevice(touch, 4, 5, 6);
    FakeEsp32().gpio.attachOutput(1, touch.cs());
  }
} emulator;

#endif

#include "roo_display/driver/ili9341.h"
Ili9341spi<7, 2, 3> device;
Display display(device);

#endif

namespace {

#ifdef ROO_DISPLAY_BENCHMARK_OFFSCREEN
constexpr int16_t kBenchmarkScreenWidth = 320;
constexpr int16_t kBenchmarkScreenHeight = 240;
#endif

constexpr int kFillIterations = 80;
constexpr int kWriteIterations = 80;
constexpr int kTextIterationsSmall = 35;
constexpr int kTextIterationsMedium = 24;
constexpr int kTextIterationsLarge = 16;

const char* const kTextLines[] = {
    "The quick brown fox jumps over the lazy dog.",
    "Pack my box with five dozen liquor jugs.",
    "Sphinx of black quartz, judge my vow.",
};

struct FontCase {
  const char* name;
  const Font& (*font)();
  int iterations;
};

const FontCase kFonts[] = {
    {"NotoSans Regular 12", &font_NotoSans_Regular_12, kTextIterationsSmall},
    {"NotoSans Regular 18", &font_NotoSans_Regular_18, kTextIterationsMedium},
    {"NotoSans Regular 27", &font_NotoSans_Regular_27, kTextIterationsLarge},
};

template <size_t N>
RectUnion MakeUnion(const std::array<Box, N>& exclusions) {
  return RectUnion(exclusions.data(), exclusions.data() + exclusions.size());
}

std::vector<Color> MakeGradient(size_t count) {
  std::vector<Color> colors;
  colors.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    colors.emplace_back(static_cast<uint8_t>(24 + (i * 5) % 200),
                        static_cast<uint8_t>(40 + (i * 9) % 180),
                        static_cast<uint8_t>(64 + (i * 13) % 160));
  }
  return colors;
}

template <size_t N>
unsigned long BenchmarkFillCase(Display& target,
                                const std::array<Box, N>& exclusions,
                                const Box& window, int iterations,
                                Color color) {
  RectUnion exclusion = MakeUnion(exclusions);
  unsigned long start = micros();
  for (int i = 0; i < iterations; ++i) {
    DisplayOutput& output = target.output();
    output.begin();
    RectUnionFilter filter(output, &exclusion);
    filter.setAddress(window.xMin(), window.yMin(), window.xMax(),
                      window.yMax(), BlendingMode::kSource);
    filter.fill(color, static_cast<uint32_t>(window.area()));
    output.end();
  }
  return micros() - start;
}

template <size_t N>
unsigned long BenchmarkWriteCase(Display& target,
                                 const std::array<Box, N>& exclusions,
                                 const Box& window, int iterations,
                                 const std::vector<Color>& colors) {
  RectUnion exclusion = MakeUnion(exclusions);
  unsigned long start = micros();
  for (int i = 0; i < iterations; ++i) {
    DisplayOutput& output = target.output();
    output.begin();
    RectUnionFilter filter(output, &exclusion);
    filter.setAddress(window.xMin(), window.yMin(), window.xMax(),
                      window.yMax(), BlendingMode::kSource);
    filter.write(const_cast<Color*>(colors.data()), colors.size());
    output.end();
  }
  return micros() - start;
}

template <size_t N>
unsigned long BenchmarkTextCase(Display& target, const Font& font,
                                const std::array<Box, N>& exclusions,
                                int iterations) {
  RectUnion exclusion = MakeUnion(exclusions);
  unsigned long start = micros();
  for (int i = 0; i < iterations; ++i) {
    DisplayOutput& output = target.output();
    output.begin();
    RectUnionFilter filter(output, &exclusion);
    Surface surface(&filter, target.extents(), false, color::White,
                    FillMode::kExtents, BlendingMode::kSource);
    int16_t y = 12 + font.metrics().ascent();
    for (const char* text : kTextLines) {
      surface.drawObject(
          StringViewLabel(text, font, color::Black, FillMode::kExtents), 12, y);
      y += font.metrics().linespace() + 8;
    }
    output.end();
  }
  return micros() - start;
}

double AverageMicros(unsigned long total, int iterations) {
  return iterations == 0 ? 0.0 : static_cast<double>(total) / iterations;
}

template <size_t N>
void RunSyntheticCase(Display& target, const char* name,
                      const std::array<Box, N>& exclusions, const Box& window) {
  target.clear();
  std::vector<Color> colors = MakeGradient(static_cast<size_t>(window.area()));
  unsigned long fill_us = BenchmarkFillCase(target, exclusions, window,
                                            kFillIterations, Color(0xFF224488));
  unsigned long write_us =
      BenchmarkWriteCase(target, exclusions, window, kWriteIterations, colors);
  Serial.printf("%-28s %12lu %12.1f %12lu %12.1f\n", name, fill_us,
                AverageMicros(fill_us, kFillIterations), write_us,
                AverageMicros(write_us, kWriteIterations));
}

void RunTextBenchmarks(Display& target) {
  const std::array<Box, 3> exclusions = {
      Box(36, 28, 244, 44),
      Box(72, 62, 206, 88),
      Box(112, 98, 288, 132),
  };

  Serial.println();
  Serial.println("Text benchmark (FillMode::kExtents)");
  Serial.println(
      "----------------------------------------------------------------");
  Serial.println("Font                         Total us      Avg/frame us");
  Serial.println(
      "----------------------------------------------------------------");

  for (const FontCase& font_case : kFonts) {
    target.clear();
    unsigned long total = BenchmarkTextCase(target, font_case.font(),
                                            exclusions, font_case.iterations);
    Serial.printf("%-28s %12lu %16.1f\n", font_case.name, total,
                  AverageMicros(total, font_case.iterations));
  }
}

void RunBenchmarksOnTarget(Display& target) {
  const Box window(20, 72, 299, 196);
  const std::array<Box, 1> single_gap = {Box(120, 72, 148, 196)};
  const std::array<Box, 5> alternating_bands = {
      Box(28, 72, 48, 196),   Box(76, 72, 96, 196),   Box(124, 72, 144, 196),
      Box(172, 72, 192, 196), Box(220, 72, 240, 196),
  };
  const std::array<Box, 12> dense_transitions = {
      Box(20, 72, 27, 196),   Box(44, 72, 51, 196),   Box(68, 72, 75, 196),
      Box(92, 72, 99, 196),   Box(116, 72, 123, 196), Box(140, 72, 147, 196),
      Box(164, 72, 171, 196), Box(188, 72, 195, 196), Box(212, 72, 219, 196),
      Box(236, 72, 243, 196), Box(260, 72, 267, 196), Box(284, 72, 291, 196),
  };

  Serial.println("clip_exclude_rects benchmark");
  Serial.print("backend=");
#ifdef ROO_DISPLAY_BENCHMARK_OFFSCREEN
  Serial.println("offscreen");
#else
  Serial.println("ili9341");
#endif
  Serial.println(
      "------------------------------------------------------------------------"
      "--------");
  Serial.println(
      "Scenario                         Fill total    Fill avg     Write total "
      "  Write avg");
  Serial.println(
      "------------------------------------------------------------------------"
      "--------");

  RunSyntheticCase(target, "single gap", single_gap, window);
  RunSyntheticCase(target, "alternating bands", alternating_bands, window);
  RunSyntheticCase(target, "dense transitions", dense_transitions, window);

  RunTextBenchmarks(target);
  Serial.println();
  Serial.println("Done!");
}

}  // namespace

void setup() {
  Serial.begin(115200);
#ifndef ROO_DISPLAY_BENCHMARK_OFFSCREEN
  SPI.begin(4, 5, 6);
  display.init(color::White);
#endif
}

void loop() {
#ifdef ROO_DISPLAY_BENCHMARK_OFFSCREEN
  std::vector<roo::byte> raster(static_cast<size_t>(kBenchmarkScreenWidth) *
                                kBenchmarkScreenHeight * 2);
  OffscreenDevice<Argb4444> offscreen(
      kBenchmarkScreenWidth, kBenchmarkScreenHeight, raster.data(), Argb4444());
  Display offscreen_display(offscreen);
  RunBenchmarksOnTarget(offscreen_display);
#else
  RunBenchmarksOnTarget(display);
#endif
  delay(1000);
}