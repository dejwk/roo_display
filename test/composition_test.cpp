#include <algorithm>
#include <memory>
#include <vector>

#include "roo_display/composition/rasterizable_stack.h"
#include "roo_display/composition/streamable_stack.h"
#include "roo_display/filter/foreground.h"
#include "testing.h"

namespace roo_display {
namespace {

Color Sample(int seed, int16_t x, int16_t y) {
  const Color colors[] = {color::Transparent, color::Background,
                          Color(0x00123456),  Color(0x80654321),
                          color::Red,         color::Green,
                          color::Blue,        color::White};
  return colors[static_cast<uint32_t>(x + 17 * y + seed) % 8];
}

struct Layer {
  Box covered;
  int16_t dx;
  int16_t dy;
  int seed;
  BlendingMode mode;
};

// Scalar oracle intentionally independent of the compositor's mode classifier
// and geometry compiler. Absent samples have composition-wide mode semantics.
Color Evaluate(const std::vector<Layer>& layers, int16_t x, int16_t y) {
  Color result = color::Transparent;
  for (const Layer& layer : layers) {
    if (layer.covered.contains(x, y)) {
      result = ApplyBlending(layer.mode, result,
                             Sample(layer.seed, x - layer.dx, y - layer.dy));
    } else {
      switch (layer.mode) {
        case BlendingMode::kSource:
        case BlendingMode::kSourceIn:
        case BlendingMode::kSourceOut:
        case BlendingMode::kDestinationIn:
        case BlendingMode::kDestinationAtop:
        case BlendingMode::kClear:
          result = color::Transparent;
          break;
        default:
          break;
      }
    }
  }
  return result;
}

// Checks read sizes crossing rows and internal buffers, including the entire
// prefix promised by run metadata, which can extend beyond the current read.
void CheckStream(const Streamable& stack, Box clip,
                 const std::vector<Layer>& layers) {
  std::unique_ptr<PixelStream> stream = stack.createStream(clip);
  Color pixels[67];
  for (int offset = 0; offset < clip.area();) {
    int count = std::min(1 + offset % 67, clip.area() - offset);
    uint32_t run = 123;
    stream->read(pixels, count, run);
    for (int i = 0; i < count; ++i) {
      int index = offset + i;
      ASSERT_EQ(pixels[i], Evaluate(layers, clip.xMin() + index % clip.width(),
                                    clip.yMin() + index / clip.width()));
    }
    uint32_t prefix = std::min<uint32_t>(run, clip.area() - offset);
    for (uint32_t i = 0; i < prefix; ++i) {
      int index = offset + i;
      ASSERT_EQ(pixels[0], Evaluate(layers, clip.xMin() + index % clip.width(),
                                    clip.yMin() + index / clip.width()));
    }
    offset += count;
  }
  stream->read(nullptr, 0);
}

void CheckDrawing(const Drawable& stack, Box clip,
                  const std::vector<Layer>& layers, FillMode fill,
                  Color background, BlendingMode mode) {
  FakeOffscreen<Argb8888> output(clip.xMax() + 4, clip.yMax() + 4,
                                 color::Magenta);
  // Exercise surface translation separately from source translations.
  Surface surface(output, 1, 2, clip.translate(1, 2), false, background, fill,
                  mode);
  surface.drawObject(stack);
  for (int y = 0; y < output.raw_height(); ++y) {
    for (int x = 0; x < output.raw_width(); ++x) {
      Color want = color::Magenta;
      if (clip.contains(x - 1, y - 2)) {
        Color sample = Evaluate(layers, x - 1, y - 2);
        if (fill == FillMode::kExtents || sample.a() != 0 ||
            sample == color::Background) {
          Color resolved = sample == color::Background ? background
                           : background == color::Transparent
                               ? sample
                               : AlphaBlend(background, sample);
          want = ApplyBlending(mode, want, resolved);
        }
      }
      ASSERT_EQ(output.buffer()[y * output.raw_width() + x], want)
          << x << ", " << y;
    }
  }
}

// Checks both address-window streaming and rectangular filter access against
// the same scene, without confusing composition Background with surface fill.
void CheckFilter(const Rasterizable& stack, Box clip,
                 const std::vector<Layer>& layers) {
  for (bool window : {false, true}) {
    FakeOffscreen<Argb8888> output(clip.xMax() + 1, clip.yMax() + 1,
                                   color::Magenta);
    ForegroundFilter filter(output, &stack);
    if (window) {
      filter.setAddress(clip.xMin(), clip.yMin(), clip.xMax(), clip.yMax(),
                        BlendingMode::kSource);
      filter.fill(color::Blue, clip.area());
    } else {
      DisplayOutput& target = filter;
      target.fillRect(BlendingMode::kSource, clip, color::Blue);
    }
    for (int y = clip.yMin(); y <= clip.yMax(); ++y) {
      for (int x = clip.xMin(); x <= clip.xMax(); ++x) {
        ASSERT_EQ(output.buffer()[y * output.raw_width() + x],
                  AlphaBlend(color::Blue, Evaluate(layers, x, y)));
      }
    }
  }
}

}  // namespace

// Verifies the same scalar scene through points, rectangles, uniform probes,
// nested/compiled streams, translated drawing, and both filter access paths.
// Sizes straddle the 64/128-pixel thresholds; layers use signed offsets,
// source clips, absent sources, every ordinary mode, and alpha-zero RGB.
TEST(Composition, ConsumptionPathsMatchScalarOracle) {
  for (Box bounds : {Box(0, 0, 8, 6), Box(0, 0, 7, 7), Box(0, 0, 12, 4),
                     Box(0, 0, 15, 7), Box(0, 0, 42, 2), Box(0, 0, 16, 9)}) {
    for (int scene = 0; scene < 12; ++scene) {
      SCOPED_TRACE(bounds);
      SCOPED_TRACE(scene);
      std::vector<std::unique_ptr<Rasterizable>> sources;
      std::vector<Layer> layers;
      RasterizableStack raster(bounds);
      StreamableStack compiled(bounds);
      for (int i = 0; i < 4; ++i) {
        int seed = scene * 11 + i * 3;
        Box source_bounds(-3, -2, bounds.xMax() + 3, bounds.yMax() + 2);
        auto source = MakeRasterizable(
            source_bounds,
            [seed](int16_t x, int16_t y) { return Sample(seed, x, y); });
        sources.emplace_back(new decltype(source)(source));
        int16_t dx = (scene + i) % 5 - 2;
        int16_t dy = (scene + 2 * i) % 5 - 2;
        Box source_clip = i == 0
                              ? source_bounds
                              : Box(i - 1, 0, bounds.xMax() - i, bounds.yMax());
        if (i == 2 && scene % 3 == 0) source_clip = Box(100, 100, 101, 101);
        BlendingMode mode = static_cast<BlendingMode>((scene + i * 5) % 12);
        layers.push_back(
            {Box::Intersect(source_bounds, source_clip).translate(dx, dy), dx,
             dy, seed, mode});
        raster.addInput(sources.back().get(), source_clip, dx, dy)
            .withMode(mode);
        compiled.addInput(sources.back().get(), source_clip, dx, dy)
            .withMode(mode);
      }
      RasterizableStack nested(bounds);
      nested.addInput(&raster).withMode(BlendingMode::kSource);
      for (Box clip :
           {bounds, Box(1, 1, bounds.xMax() - 1, bounds.yMax() - 1)}) {
        std::vector<Color> pixels(clip.area());
        std::vector<int16_t> x(clip.area());
        std::vector<int16_t> y(clip.area());
        for (int i = 0; i < clip.area(); ++i) {
          x[i] = clip.xMin() + i % clip.width();
          y[i] = clip.yMin() + i / clip.width();
        }
        raster.readColors(x.data(), y.data(), pixels.size(), pixels.data());
        for (int i = 0; i < clip.area(); ++i)
          ASSERT_EQ(pixels[i], Evaluate(layers, x[i], y[i]));
        bool uniform = raster.readColorRect(
            clip.xMin(), clip.yMin(), clip.xMax(), clip.yMax(), pixels.data());
        for (int i = 0; i < clip.area(); ++i)
          ASSERT_EQ(pixels[uniform ? 0 : i], Evaluate(layers, x[i], y[i]));
        Color same;
        if (raster.readUniformColorRect(clip.xMin(), clip.yMin(), clip.xMax(),
                                        clip.yMax(), &same)) {
          for (int i = 0; i < clip.area(); ++i)
            ASSERT_EQ(same, Evaluate(layers, x[i], y[i]));
        }
        CheckStream(raster, clip, layers);
        CheckStream(compiled, clip, layers);
        CheckStream(nested, clip, layers);
        CheckFilter(raster, clip, layers);
        for (FillMode fill : {FillMode::kVisible, FillMode::kExtents}) {
          for (Color bg : {color::Transparent, Color(0x00112233),
                           Color(0x80445566), color::Blue}) {
            for (BlendingMode mode :
                 {BlendingMode::kSource, BlendingMode::kSourceOver}) {
              CheckDrawing(raster, clip, layers, fill, bg, mode);
              CheckDrawing(compiled, clip, layers, fill, bg, mode);
            }
          }
        }
      }
    }
  }
}

}  // namespace roo_display
