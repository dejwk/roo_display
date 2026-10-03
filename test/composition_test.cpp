#include <algorithm>
#include <memory>
#include <vector>

#include "roo_display/composition/rasterizable_stack.h"
#include "roo_display/composition/streamable_stack.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/filter/foreground.h"
#include "roo_display/shape/basic.h"
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

// Verifies group opacity is applied after overlap resolution using existing
// nesting, and the existing Offscreen constructor snapshots the same result.
TEST(Composition, NestedGroupOpacityAndOffscreenSnapshot) {
  Box bounds(0, 0, 31, 15);
  FilledRect base(bounds, color::Red);
  FilledRect detail(Box(8, 0, 23, 15), color::Blue);
  RasterizableStack content(bounds);
  content.addInput(&base);
  content.addInput(&detail);
  FilledRect opacity(bounds, Color(128, 0, 0, 0));
  RasterizableStack faded(bounds);
  faded.addInput(&content);
  faded.addInput(&opacity).withMode(BlendingMode::kDestinationIn);
  Offscreen<Argb8888> cached(faded);
  Color pixels[32];
  auto stream = faded.createStream();
  stream->read(pixels, 32);
  Color snapshot[32];
  cached.readColorRect(0, 0, 31, 0, snapshot);
  for (int x = 0; x < 32; ++x) {
    Color want = (x >= 8 && x <= 23 ? color::Blue : color::Red).withA(128);
    EXPECT_EQ(pixels[x], want);
    EXPECT_EQ(snapshot[x], want);
  }
  EXPECT_NE(pixels[8],
            AlphaBlend(color::Red.withA(128), color::Blue.withA(128)));
}

// Verifies clipped-out inputs cannot enlarge the input envelope, in either
// insertion order, while their composition-wide clearing effects remain live.
TEST(Composition, NaturalExtentsIgnoreEmptyInputs) {
  FilledRect source(Box(0, 0, 9, 9), color::Red);
  RasterizableStack raster(source.extents());
  StreamableStack stream(source.extents());
  for (bool empty_first : {false, true}) {
    raster.clearInputs();
    stream.clearInputs();
    for (int i = 0; i < 2; ++i) {
      Box clip =
          (i == 0) == empty_first ? Box(100, 100, 109, 109) : source.extents();
      raster.addInput(&source, clip, -3, -2);
      stream.addInput(&source, clip, -3, -2);
    }
    EXPECT_EQ(raster.naturalExtents(), Box(-3, -2, 6, 7));
    EXPECT_EQ(stream.naturalExtents(), raster.naturalExtents());
  }
  raster.clearInputs();
  stream.clearInputs();
  for (Box clip : {Box(100, 100, 109, 109), Box(-20, -20, -11, -11)}) {
    raster.addInput(&source, clip);
    stream.addInput(&source, clip);
  }
  EXPECT_EQ(raster.naturalExtents(), Box(0, 0, -1, -1));
  EXPECT_EQ(stream.naturalExtents(), raster.naturalExtents());

  raster.addInput(&source);
  stream.addInput(&source);
  raster.addInput(&source, Box(100, 100, 109, 109))
      .withMode(BlendingMode::kDestinationIn);
  stream.addInput(&source, Box(100, 100, 109, 109))
      .withMode(BlendingMode::kDestinationIn);
  EXPECT_EQ(raster.naturalExtents(), source.extents());
  EXPECT_EQ(stream.naturalExtents(), source.extents());
  Color pixel;
  raster.createStream()->read(&pixel, 1);
  EXPECT_EQ(pixel, color::Transparent);
  stream.createStream()->read(&pixel, 1);
  EXPECT_EQ(pixel, color::Transparent);
}

namespace {

// Records evaluation separately from metadata queries.
class EvaluationProbe : public FilledRect {
 public:
  EvaluationProbe(Box bounds, Color color) : FilledRect(bounds, color) {}

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override {
    ++evaluations;
    FilledRect::readColors(x, y, count, result);
  }

  bool readUniformColorRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                            Color* result) const override {
    ++evaluations;
    return FilledRect::readUniformColorRect(x0, y0, x1, y1, result);
  }

  std::unique_ptr<PixelStream> createStream(const Box& clip) const override {
    ++evaluations;
    return FilledRect::createStream(clip);
  }

  mutable int evaluations = 0;
};

}  // namespace

// Verifies conservative opacity claims against actual alpha across ordinary
// modes, partial/absent sources, and transparent samples including Background.
TEST(Composition, OpacityHintsAgreeWithPixels) {
  Box bounds(0, 0, 3, 2);
  for (Color bottom : {color::Red, Color(0x80654321), color::Transparent}) {
    EvaluationProbe base(bounds, bottom);
    for (Color top : {color::Blue, Color(0x80123456), color::Background,
                      color::Transparent}) {
      EvaluationProbe upper(bounds, top);
      for (Box clip : {bounds, Box(1, 1, 2, 2), Box(20, 20, 30, 30)}) {
        for (int m = 0; m < 12; ++m) {
          BlendingMode mode = static_cast<BlendingMode>(m);
          RasterizableStack raster(bounds);
          StreamableStack stream(bounds);
          raster.addInput(&base);
          stream.addInput(&base);
          raster.addInput(&upper, clip).withMode(mode);
          stream.addInput(&upper, clip).withMode(mode);
          int before = base.evaluations + upper.evaluations;
          TransparencyMode hint = raster.getTransparencyMode();
          ASSERT_EQ(stream.getTransparencyMode(), hint);
          EXPECT_EQ(base.evaluations + upper.evaluations, before);
          if (hint != TransparencyMode::kNone) continue;
          Color pixels[12];
          stream.createStream()->read(pixels, 12);
          for (Color pixel : pixels) EXPECT_TRUE(pixel.isOpaque());
        }
      }
    }
  }
}

// Verifies nested opaque groups suppress hidden reads and that live color,
// blend-mode, source-clip, translation, and stack-bound changes refresh hints.
TEST(Composition, NestedOpacityTracksCurrentSourcesAndGeometry) {
  Box bounds(0, 0, 31, 15);
  EvaluationProbe hidden(bounds, color::Green);
  FilledRect leaf(bounds, color::Red);
  RasterizableStack inner(bounds);
  inner.addInput(&leaf);
  RasterizableStack raster(bounds);
  StreamableStack stream(bounds);
  raster.addInput(&hidden);
  stream.addInput(&hidden);
  raster.addInput(&inner);
  stream.addInput(&inner);
  EXPECT_EQ(inner.getTransparencyMode(), TransparencyMode::kNone);
  EXPECT_EQ(raster.getTransparencyMode(), TransparencyMode::kNone);
  EXPECT_EQ(stream.getTransparencyMode(), TransparencyMode::kNone);
  Color pixel;
  raster.createStream()->read(&pixel, 1);
  stream.createStream()->read(&pixel, 1);
  Color tile[16];
  EXPECT_TRUE(raster.readColorRect(0, 0, 3, 3, tile));
  EXPECT_EQ(hidden.evaluations, 0);

  leaf = FilledRect(bounds, Color(0x80FF0000));
  EXPECT_EQ(inner.getTransparencyMode(), TransparencyMode::kFull);
  raster.createStream()->read(&pixel, 1);
  EXPECT_EQ(pixel, AlphaBlend(color::Green, leaf.color()));
  EXPECT_GT(hidden.evaluations, 0);
  leaf = FilledRect(bounds, color::Red);
  inner.setInput(0, &leaf).withMode(BlendingMode::kDestinationOut);
  EXPECT_EQ(inner.getTransparencyMode(), TransparencyMode::kFull);
  inner.setInput(0, &leaf, Box(1, 0, 31, 15));
  EXPECT_EQ(inner.getTransparencyMode(), TransparencyMode::kFull);
  inner.setInput(0, &leaf, 1, 0);
  EXPECT_EQ(inner.getTransparencyMode(), TransparencyMode::kFull);
  inner.setInput(0, &leaf);
  inner.setExtents(Box(0, 0, 32, 15));
  EXPECT_EQ(inner.getTransparencyMode(), TransparencyMode::kFull);
  inner.setExtents(bounds);
  EXPECT_EQ(inner.getTransparencyMode(), TransparencyMode::kNone);
  inner.addInput(&leaf, Box(100, 100, 101, 101))
      .withMode(BlendingMode::kDestinationIn);
  EXPECT_EQ(inner.getTransparencyMode(), TransparencyMode::kFull);
}

// Verifies known opaque results survive ordinary foreground/background
// overlays and full opaque masks, while partial masks revoke the guarantee.
TEST(Composition, OpacityPreservingOperations) {
  Box bounds(0, 0, 31, 15);
  FilledRect solid(bounds, color::Red);
  FilledRect translucent(bounds, Color(0x80123456));
  for (BlendingMode mode :
       {BlendingMode::kSourceOver, BlendingMode::kDestinationOver,
        BlendingMode::kSourceAtop, BlendingMode::kDestination}) {
    RasterizableStack raster(bounds);
    StreamableStack stream(bounds);
    raster.addInput(&solid);
    stream.addInput(&solid);
    raster.addInput(&translucent, Box(1, 2, 7, 9)).withMode(mode);
    stream.addInput(&translucent, Box(1, 2, 7, 9)).withMode(mode);
    EXPECT_EQ(raster.getTransparencyMode(), TransparencyMode::kNone);
    EXPECT_EQ(stream.getTransparencyMode(), TransparencyMode::kNone);
    for (BlendingMode mask :
         {BlendingMode::kSourceIn, BlendingMode::kDestinationIn,
          BlendingMode::kDestinationAtop}) {
      raster.setInput(1, &solid).withMode(mask);
      stream.setInput(1, &solid).withMode(mask);
      EXPECT_EQ(raster.getTransparencyMode(), TransparencyMode::kNone);
      EXPECT_EQ(stream.getTransparencyMode(), TransparencyMode::kNone);
      raster.setInput(1, &solid, Box(1, 2, 7, 9)).withMode(mask);
      stream.setInput(1, &solid, Box(1, 2, 7, 9)).withMode(mask);
      EXPECT_EQ(raster.getTransparencyMode(), TransparencyMode::kFull);
      EXPECT_EQ(stream.getTransparencyMode(), TransparencyMode::kFull);
    }
  }
}

}  // namespace roo_display
