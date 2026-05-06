// Micro-benchmark for small StreamableStack and RasterizableStack compositions.

#include <stdint.h>

#include <algorithm>
#include <vector>

#include "Arduino.h"
#include "roo_display.h"
#include "roo_display/composition/rasterizable_stack.h"
#include "roo_display/composition/streamable_stack.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/filter/foreground.h"
#include "roo_display/shape/basic.h"
#include "roo_display/shape/smooth.h"

using namespace roo_display;

#ifndef ROO_DISPLAY_BENCHMARK_OFFSCREEN

#ifdef ROO_TESTING
#include "roo_testing/devices/display/ili9341/ili9341spi.h"
#include "roo_testing/devices/touch/xpt2046/xpt2046spi.h"
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
#include "roo_display/driver/touch_xpt2046.h"

static constexpr int kCsPin = 7;
static constexpr int kDcPin = 2;
static constexpr int kRstPin = 3;
static constexpr int kBlPin = 20;

static constexpr int kSpiSckPin = 4;
static constexpr int kSpiMisoPin = 5;
static constexpr int kSpiMosiPin = 6;

static constexpr int kTouchCsPin = 1;

Ili9341spi<kCsPin, kDcPin, kRstPin> screen(Orientation().rotateLeft());
TouchXpt2046<kTouchCsPin> touch;

roo_display::Display display(screen, touch,
                             TouchCalibration(269, 249, 3829, 3684,
                                              Orientation::LeftDown()));

void initDisplay(Color color) {
  SPI.begin(kSpiSckPin, kSpiMisoPin, kSpiMosiPin);
  display.init(color);
}

#endif

namespace {

constexpr uint8_t kScales[] = {75, 100, 150, 200};
enum class CompositionApproach : uint8_t {
  kStreamableStack,
  kRasterizableStack,
};

enum class OverlayApproach : uint8_t {
  kNone,
  kPointOverlay,
};

constexpr CompositionApproach kApproaches[] = {
    CompositionApproach::kStreamableStack,
    CompositionApproach::kRasterizableStack,
};

constexpr OverlayApproach kOverlayApproaches[] = {
    OverlayApproach::kNone,
    OverlayApproach::kPointOverlay,
};

// One cycle renders: stable off, 4 intermediate forward frames, stable on,
// then 4 intermediate backward frames. The next cycle starts with the stable
// off frame again, so frame delta stays constant throughout the run.
constexpr uint8_t kFrameTimesMs[] = {0, 20, 40, 60, 80, 100, 80, 60, 40, 20};
constexpr int kFrameCountPerCycle =
    sizeof(kFrameTimesMs) / sizeof(kFrameTimesMs[0]);
constexpr int kCycles = 20;
constexpr int kAnimationDurationMs = 100;

constexpr Color kSurfaceColor = Color(0xFFFFFBFE);
constexpr Color kSelectedTrackColor = Color(0xFF6750A4);
constexpr Color kSelectedThumbColor = Color(0xFFFFFFFF);
constexpr Color kUnselectedTrackColor = Color(0xFFE7E0EC);
constexpr Color kUnselectedBorderColor = Color(0xFF79747E);
constexpr Color kUnselectedThumbColor = Color(0xFF79747E);
constexpr Color kPointOverlayColor = Color(0x608750A4);
constexpr int16_t kPointOverlayBaseDiameter = 40;
#ifdef ROO_DISPLAY_BENCHMARK_OFFSCREEN
constexpr int16_t kBenchmarkScreenWidth = 320;
constexpr int16_t kBenchmarkScreenHeight = 240;
#endif

struct Tokens {
  Color track;
  Color border;
  Color thumb;
};

struct Geometry {
  int16_t track_width;
  int16_t track_height;
  int16_t track_radius;
  int16_t track_outline_width;
  int16_t selected_thumb_diameter;
  int16_t unselected_thumb_diameter;
  int16_t point_overlay_diameter;
  float track_center_y;
};

struct BenchmarkResult {
  uint32_t total_us;
  uint32_t frames;
};

const char* CompositionApproachName(CompositionApproach approach) {
  switch (approach) {
    case CompositionApproach::kStreamableStack:
      return "streamable_stack";
    case CompositionApproach::kRasterizableStack:
      return "rasterizable_stack";
  }
  return "unknown";
}

const char* OverlayApproachName(OverlayApproach approach) {
  switch (approach) {
    case OverlayApproach::kNone:
      return "none";
    case OverlayApproach::kPointOverlay:
      return "point_overlay";
  }
  return "unknown";
}

int16_t ScaleDim(int16_t value, uint8_t scale_pct) {
  return (static_cast<int32_t>(value) * scale_pct + 50) / 100;
}

Geometry MakeGeometry(uint8_t scale_pct) {
  int16_t track_height = ScaleDim(32, scale_pct);
  return Geometry{
      .track_width = ScaleDim(52, scale_pct),
      .track_height = track_height,
      .track_radius = static_cast<int16_t>(track_height / 2),
      .track_outline_width = std::max<int16_t>(1, ScaleDim(2, scale_pct)),
      .selected_thumb_diameter = ScaleDim(24, scale_pct),
      .unselected_thumb_diameter = ScaleDim(16, scale_pct),
      .point_overlay_diameter = ScaleDim(kPointOverlayBaseDiameter, scale_pct),
      .track_center_y = 0.5f * static_cast<float>(track_height - 1),
  };
}

Tokens ResolveTokens(bool on) {
  if (on) {
    return Tokens{
        .track = kSelectedTrackColor,
        .border = color::Transparent,
        .thumb = kSelectedThumbColor,
    };
  }
  return Tokens{
      .track = kUnselectedTrackColor,
      .border = kUnselectedBorderColor,
      .thumb = kUnselectedThumbColor,
  };
}

int16_t ToggleAnimationFraction(uint8_t time_ms) {
  int16_t progress =
      (static_cast<int32_t>(time_ms) * 256 + kAnimationDurationMs / 2) /
      kAnimationDurationMs;
  if (progress < 0) progress = 0;
  if (progress > 256) progress = 256;
  return progress;
}

int16_t CurrentThumbDiameter(const Geometry& geometry, int16_t fraction) {
  return (geometry.unselected_thumb_diameter * (256 - fraction) +
          geometry.selected_thumb_diameter * fraction + 128) /
         256;
}

float CurrentThumbCenterX(const Geometry& geometry, int16_t fraction) {
  return geometry.track_center_y +
         (static_cast<float>(geometry.track_width - geometry.track_height) *
          static_cast<float>(fraction)) /
             256.0f;
}

template <typename Target>
Box TargetBounds(const Target& target) {
  return Box(0, 0, target.width() - 1, target.height() - 1);
}

#ifndef ROO_DISPLAY_BENCHMARK_OFFSCREEN
Box TargetBounds(const Display&) {
  return Box(0, 0, screen.effective_width() - 1, screen.effective_height() - 1);
}
#endif

template <typename Target>
void DrawFrame(Target& target, const Geometry& geometry, bool on,
               uint8_t time_ms, CompositionApproach approach,
               OverlayApproach overlay_approach) {
  Tokens tokens = ResolveTokens(on);
  int16_t fraction = ToggleAnimationFraction(time_ms);
  float track_outline_inset = 0.5f * geometry.track_outline_width;
  float border_radius = geometry.track_radius - track_outline_inset;
  if (border_radius < 0) {
    border_radius = 0;
  }

  Box target_bounds = TargetBounds(target);
  int16_t x0 =
      target_bounds.xMin() + (target_bounds.width() - geometry.track_width) / 2;
  int16_t y0 = target_bounds.yMin() +
               (target_bounds.height() - geometry.track_height) / 2;
  float track_center_y = y0 + geometry.track_center_y;

  DrawingContext dc(target);
  dc.setBackgroundColor(kSurfaceColor);
  dc.setFillMode(FillMode::kExtents);

  auto track = SmoothFilledRoundRect(
      x0 - 0.5f, y0 - 0.5f, x0 + geometry.track_width - 0.5f,
      y0 + geometry.track_height - 0.5f, geometry.track_radius, tokens.track);

  SmoothShape border_shape;
  if (tokens.border.a() != 0) {
    border_shape = SmoothThickRoundRect(
        x0 - 0.5f + track_outline_inset, y0 - 0.5f + track_outline_inset,
        x0 + geometry.track_width - 0.5f - track_outline_inset,
        y0 + geometry.track_height - 0.5f - track_outline_inset, border_radius,
        geometry.track_outline_width, tokens.border);
  }

  int16_t thumb_diameter = CurrentThumbDiameter(geometry, fraction);
  int16_t thumb_radius = thumb_diameter / 2;
  float thumb_center_x = x0 + CurrentThumbCenterX(geometry, fraction);
  auto thumb = SmoothFilledCircle({thumb_center_x, track_center_y},
                                  thumb_radius - 0.5f, tokens.thumb);

  Box extents(x0, y0, x0 + geometry.track_width - 1,
              y0 + geometry.track_height - 1);
  auto draw_with_optional_overlay = [&](const auto& drawable) {
    if (overlay_approach == OverlayApproach::kPointOverlay) {
      auto overlay = SmoothFilledCircle(
          {thumb_center_x, track_center_y},
          geometry.point_overlay_diameter * 0.5f - 0.5f, kPointOverlayColor);
      roo_display::ForegroundFilter filter(target.output(), &overlay);
      Surface surface(&filter, target_bounds, false, kSurfaceColor,
                      FillMode::kExtents);
      surface.drawObject(drawable);
      return;
    }
    dc.draw(drawable);
  };

  switch (approach) {
    case CompositionApproach::kStreamableStack: {
      StreamableStack composite(extents);
      composite.addInput(&track);
      if (tokens.border.a() != 0) {
        composite.addInput(&border_shape);
      }
      composite.addInput(&thumb);
      draw_with_optional_overlay(composite);
      return;
    }
    case CompositionApproach::kRasterizableStack: {
      RasterizableStack composite(extents);
      composite.addInput(&track);
      if (tokens.border.a() != 0) {
        composite.addInput(&border_shape);
      }
      composite.addInput(&thumb);
      draw_with_optional_overlay(composite);
      return;
    }
  }
}

template <typename Target>
BenchmarkResult RunScaleCaseOnTarget(Target& target, uint8_t scale_pct,
                                     CompositionApproach approach,
                                     OverlayApproach overlay_approach) {
  display.clear();
  Geometry geometry = MakeGeometry(scale_pct);

  uint32_t start = micros();
  uint32_t frames = 0;
  for (int cycle = 0; cycle < kCycles; ++cycle) {
    DrawFrame(target, geometry, false, kFrameTimesMs[0], approach,
              overlay_approach);
    ++frames;
    for (int frame = 1; frame < 6; ++frame) {
      DrawFrame(target, geometry, true, kFrameTimesMs[frame], approach,
                overlay_approach);
      ++frames;
    }
    for (int frame = 6; frame < kFrameCountPerCycle; ++frame) {
      DrawFrame(target, geometry, false, kFrameTimesMs[frame], approach,
                overlay_approach);
      ++frames;
    }
  }
  return BenchmarkResult{static_cast<uint32_t>(micros() - start), frames};
}

#ifdef ROO_DISPLAY_BENCHMARK_OFFSCREEN
BenchmarkResult RunScaleCase(uint8_t scale_pct, CompositionApproach approach,
                             OverlayApproach overlay_approach) {
  std::vector<roo::byte> raster(static_cast<size_t>(kBenchmarkScreenWidth) *
                                kBenchmarkScreenHeight * 2);
  OffscreenDevice<Argb4444> offscreen(
      kBenchmarkScreenWidth, kBenchmarkScreenHeight, raster.data(), Argb4444());
  Display offscreen_display(offscreen);
  return RunScaleCaseOnTarget(offscreen_display, scale_pct, approach,
                              overlay_approach);
}
#else
BenchmarkResult RunScaleCase(uint8_t scale_pct, CompositionApproach approach,
                             OverlayApproach overlay_approach) {
  return RunScaleCaseOnTarget(display, scale_pct, approach, overlay_approach);
}
#endif

double FramesPerSecond(const BenchmarkResult& result) {
  if (result.total_us == 0) return 0.0;
  return (1000000.0 * static_cast<double>(result.frames)) /
         static_cast<double>(result.total_us);
}

double AvgFrameUs(const BenchmarkResult& result) {
  if (result.frames == 0) return 0.0;
  return static_cast<double>(result.total_us) /
         static_cast<double>(result.frames);
}

void RunBenchmarks() {
  Serial.println();
  Serial.println("Composition benchmark");
  Serial.print("backend=");
#ifdef ROO_DISPLAY_BENCHMARK_OFFSCREEN
  Serial.println("offscreen");
#else
  Serial.println("ili9341");
#endif
  Serial.print("cycles=");
  Serial.print(kCycles);
  Serial.print(", frame_dt_ms=");
  Serial.print(kFrameTimesMs[1] - kFrameTimesMs[0]);
  Serial.print(", frames_per_cycle=");
  Serial.println(kFrameCountPerCycle);
  for (CompositionApproach approach : kApproaches) {
    for (OverlayApproach overlay_approach : kOverlayApproaches) {
      Serial.print("approach=");
      Serial.print(CompositionApproachName(approach));
      Serial.print(", overlay=");
      Serial.println(OverlayApproachName(overlay_approach));
      Serial.println("scale | frames | total_us | avg_frame_us | fps");
      Serial.println("------------------------------------------------");

      for (uint8_t scale_pct : kScales) {
        BenchmarkResult result =
            RunScaleCase(scale_pct, approach, overlay_approach);
        Serial.print(scale_pct);
        Serial.print("% | ");
        Serial.print(result.frames);
        Serial.print(" | ");
        Serial.print(result.total_us);
        Serial.print(" | ");
        Serial.print(AvgFrameUs(result), 2);
        Serial.print(" | ");
        Serial.println(FramesPerSecond(result), 2);
      }
      Serial.println();
    }
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(250);
#ifndef ROO_DISPLAY_BENCHMARK_OFFSCREEN
  initDisplay(kSurfaceColor);
#endif
}

void loop() {
  RunBenchmarks();
  delay(1000);
}