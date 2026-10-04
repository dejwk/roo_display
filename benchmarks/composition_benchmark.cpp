// Host benchmark: separate input setup, stream preparation, consumption, and
// drawing. Heap figures count requested C++ allocation bytes, excluding malloc
// bookkeeping, C allocations, fixture storage, and thread stacks.
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <string>
#include <vector>

#include "roo_display/composition/rasterizable_stack.h"
#include "roo_display/composition/streamable_stack.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/image/image.h"
#include "roo_display/shape/basic.h"
#include "test/composition_heap_tracking.h"

namespace {
using namespace roo_display;

struct Measurement {
  double us = 0;
  size_t allocations = 0;
  size_t peak = 0;
};

template <typename Fn>
void Measure(Measurement& total, Fn fn) {
  auto start = std::chrono::steady_clock::now();
  allocation::Stats stats = allocation::Measure(fn);
  auto end = std::chrono::steady_clock::now();
  total.us += std::chrono::duration<double, std::micro>(end - start).count();
  total.allocations += stats.count;
  total.peak = std::max(total.peak, stats.peak);
}

struct Input {
  const Streamable* stream;
  const Rasterizable* raster;
  Box clip;
  BlendingMode mode;
};

struct Scene {
  Box bounds;
  std::vector<std::unique_ptr<Rasterizable>> rasters;
  std::vector<uint8_t> encoded;
  std::unique_ptr<Streamable> image;
  std::vector<Input> inputs;
};

Scene MakeScene(const char* name, int width, int height, int count) {
  Scene scene;
  scene.bounds = Box(0, 0, width - 1, height - 1);
  bool compressed = std::string(name) == "rle";
  if (compressed) {
    for (int offset = 0; offset < width * height;) {
      int run = std::min(64, width * height - offset);
      scene.encoded.push_back(0x80 | (run - 1));
      scene.encoded.push_back(offset % 128 == 0 ? 64 : 192);
      offset += run;
    }
    scene.image.reset(new RleImage<Alpha8, ConstDramPtr>(
        width, height, scene.encoded.data(), Alpha8(color::Blue)));
  }
  for (int i = 0; i < count; ++i) {
    Box clip = scene.bounds;
    BlendingMode mode = BlendingMode::kSourceOver;
    std::string kind(name);
    if (kind == "sparse") {
      int x = (i % 4) * width / 4;
      int y = (i / 4) * height / 4;
      clip = Box(x, y, x + width / 4 - 1, y + height / 4 - 1);
    }
    if ((kind == "mask" || kind == "uniform_mask" || kind == "opaque_mask" ||
         kind == "alpha_mask") &&
        i == count - 1) {
      clip = Box(width / 4, height / 4, width * 3 / 4 - 1, height * 3 / 4 - 1);
      mode = BlendingMode::kDestinationIn;
    }
    if (compressed) {
      scene.inputs.push_back({scene.image.get(), nullptr, clip, mode});
      continue;
    }
    auto raster = MakeRasterizable(scene.bounds, [i](int16_t x, int16_t y) {
      return Color(96, (x * 7 + i * 13) % 256, (y * 11 + i) % 256, 128);
    });
    scene.rasters.emplace_back(new decltype(raster)(raster));
    const Rasterizable* source = scene.rasters.back().get();
    if ((kind == "opaque" && i == count - 1) || kind == "nested_opaque" ||
        kind == "uniform" || kind == "uniform_mask") {
      Color color = kind == "uniform" || kind == "uniform_mask"
                        ? Color(96, i * 13, 128, 192)
                        : color::Blue;
      scene.rasters.emplace_back(new FilledRect(scene.bounds, color));
      source = scene.rasters.back().get();
    }
    if ((kind == "opaque_mask" || kind == "alpha_mask") && i == count - 1) {
      scene.rasters.emplace_back(new FilledRect(
          scene.bounds, Color(kind == "opaque_mask" ? 255 : 128, 0, 0, 0)));
      source = scene.rasters.back().get();
    }
    if (kind == "nested" || kind == "nested_opaque") {
      std::unique_ptr<RasterizableStack> inner(
          new RasterizableStack(scene.bounds));
      inner->addInput(source);
      inner->addInput(source, width / 8, height / 8);
      source = inner.get();
      scene.rasters.push_back(std::move(inner));
    }
    scene.inputs.push_back({source, source, clip, mode});
  }
  return scene;
}

void Add(StreamableStack& stack, const Input& input) {
  stack.addInput(input.stream, input.clip).withMode(input.mode);
}

void Add(RasterizableStack& stack, const Input& input) {
  stack.addInput(input.raster, input.clip).withMode(input.mode);
}

volatile uint32_t checksum = 0;

// Consume exactly the requested pixels, allowing uniform metadata to skip long
// runs as real filtering consumers do. Resolve Background against the drawing
// surface's transparent background before comparing checksums.
void Consume(PixelStream& stream, int count) {
  Color pixels[64];
  uint32_t sum = 0;
  while (count > 0) {
    int batch = std::min(64, count);
    uint32_t run = 0;
    stream.read(pixels, batch, run);
    if (run >= static_cast<uint32_t>(batch)) {
      run = std::min<uint32_t>(run, count);
      sum += (pixels[0] == color::Background ? 0 : pixels[0].asArgb()) * run;
      stream.skip(run - batch);
      count -= run;
    } else {
      for (int i = 0; i < batch; ++i) {
        sum += pixels[i] == color::Background ? 0 : pixels[i].asArgb();
      }
      count -= batch;
    }
  }
  checksum = sum;
}

template <typename Stack>
void Run(const char* backend, const char* name, Scene& scene) {
  constexpr int kIterations = 10;
  Measurement setup;
  Measurement prepare;
  Measurement consume;
  Measurement draw;
  Measurement rebuild;
  Offscreen<Argb8888> output(scene.bounds);
  Stack reusable(scene.bounds);
  reusable.reserveInputs(scene.inputs.size());
  for (int iteration = 0; iteration < kIterations; ++iteration) {
    Stack stack(scene.bounds);
    Measure(setup, [&]() {
      for (const Input& input : scene.inputs) Add(stack, input);
    });
    Measure(rebuild, [&]() {
      reusable.clearInputs();
      for (const Input& input : scene.inputs) Add(reusable, input);
    });
    std::unique_ptr<PixelStream> stream;
    Measure(prepare, [&]() { stream = stack.createStream(); });
    Measure(consume, [&]() { Consume(*stream, scene.bounds.area()); });
    uint32_t streamed = checksum;
    Measure(draw, [&]() {
      Surface surface(output.output(), 0, 0, scene.bounds, false,
                      color::Transparent, FillMode::kExtents,
                      BlendingMode::kSource);
      surface.drawObject(stack);
    });
    std::unique_ptr<PixelStream> rendered = output.createStream();
    Consume(*rendered, scene.bounds.area());
    if (checksum != streamed) {
      std::fprintf(stderr, "Rendering mismatch: %s/%s\n", backend, name);
      std::abort();
    }
  }
  std::printf("%s,%s,%zu,%d", backend, name, scene.inputs.size(),
              scene.bounds.area());
  for (const Measurement* phase :
       {&setup, &rebuild, &prepare, &consume, &draw}) {
    std::printf(",%.2f,%.1f,%zu", phase->us / kIterations,
                static_cast<double>(phase->allocations) / kIterations,
                phase->peak);
  }
  std::printf("\n");
}

}  // namespace

int main() {
  std::printf("backend,scene,layers,pixels");
  for (const char* phase : {"setup", "rebuild", "prepare", "consume", "draw"}) {
    std::printf(",%s_us,%s_allocations,%s_peak_bytes", phase, phase, phase);
  }
  std::printf("\n");
  for (int width : {32, 160}) {
    for (int layers : {1, 4, 16}) {
      for (const char* kind :
           {"overlap", "opaque", "sparse", "mask", "nested", "rle", "uniform",
            "uniform_mask", "nested_opaque", "opaque_mask", "alpha_mask"}) {
        Scene scene = MakeScene(kind, width, width * 3 / 4, layers);
        Run<StreamableStack>("stream", kind, scene);
        if (std::string(kind) != "rle")
          Run<RasterizableStack>("raster", kind, scene);
      }
    }
  }
}
