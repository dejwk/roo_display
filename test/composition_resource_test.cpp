#include <array>
#include <memory>

#include "roo_display/composition/rasterizable_stack.h"
#include "roo_display/composition/streamable_stack.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/shape/basic.h"
#include "test/composition_heap_tracking.h"
#include "testing.h"

namespace roo_display {
namespace {

// Retains the prepared stream until after the measurement has captured live
// storage. Fixture allocations and destruction are outside the scope.
allocation::Stats Prepare(const Streamable& stack) {
  std::unique_ptr<PixelStream> stream;
  return allocation::Measure([&]() { stream = stack.createStream(); });
}

}  // namespace

// Verifies every ordinary allocation entry point has the same accounting and
// deletion layout, including the nothrow forms intercepted by sanitizers.
TEST(CompositionResources, HeapHarnessTracksNothrowAllocations) {
  allocation::Stats stats = allocation::Measure([]() {
    void* first = ::operator new(7);
    void* second = ::operator new[](11);
    void* third = ::operator new(13, std::nothrow);
    void* fourth = ::operator new[](17, std::nothrow);
    ::operator delete(first);
    ::operator delete[](second);
    ::operator delete(third);
    ::operator delete[](fourth);
  });
  EXPECT_EQ(stats.count, 4u);
  EXPECT_EQ(stats.peak, 48u);
  EXPECT_EQ(stats.live, 0u);
}

// Verifies eliminated inputs retain neither pixel buffers nor stream slots.
// Run with both test and production buffer sizes to catch size-dependent costs.
TEST(CompositionResources, HiddenInputsDoNotRetainStreamBuffers) {
  Box bounds(0, 0, 159, 119);
  FilledRect source(bounds, color::Blue);
  StreamableStack single(bounds);
  single.addInput(&source);
  StreamableStack hidden(bounds);
  for (int i = 0; i < 16; ++i) hidden.addInput(&source);
  allocation::Stats one = Prepare(single);
  allocation::Stats many = Prepare(hidden);
  EXPECT_GT(one.live, 0u);
  EXPECT_EQ(many.live, one.live);
}

// Verifies an absent mask preserves its clearing effect while retaining no
// source buffers, including for sources preceding the mask.
TEST(CompositionResources, AbsentMaskRetainsNoSourceBuffers) {
  Box bounds(0, 0, 159, 119);
  FilledRect source(bounds, color::Blue);
  StreamableStack empty(bounds);
  StreamableStack masked(bounds);
  for (int i = 0; i < 15; ++i) masked.addInput(&source);
  masked.addInput(&source, Box(200, 200, 201, 201))
      .withMode(BlendingMode::kDestinationIn);
  EXPECT_EQ(Prepare(masked).live, Prepare(empty).live);
}

// Verifies compiled storage does not grow with pixel area when the same
// coverage program fits its count operands; no framebuffer is introduced.
TEST(CompositionResources, PreparedStorageIsIndependentOfPixelArea) {
  FilledRect source(Box(0, 0, 159, 119), Color(0x80654321));
  StreamableStack small(Box(0, 0, 31, 23));
  StreamableStack large(source.extents());
  for (int i = 0; i < 4; ++i) {
    small.addInput(&source);
    large.addInput(&source);
  }
  EXPECT_EQ(Prepare(small).live, Prepare(large).live);
}

// Verifies descriptor rebuilding retains capacity and nested nonuniform raster
// drawing stays allocation-free after preparation, including partial masks.
TEST(CompositionResources, RebuildAndNestedRasterDrawingDoNotAllocate) {
  Box bounds(0, 0, 63, 31);
  auto source = MakeRasterizable(bounds, [](int16_t x, int16_t y) {
    return Color(128, x * 3, y * 5, 64);
  });
  FilledRect mask(Box(1, 1, 62, 30), Color(0x80000000));
  std::array<RasterizableStack, 4> groups = {
      RasterizableStack(bounds), RasterizableStack(bounds),
      RasterizableStack(bounds), RasterizableStack(bounds)};
  for (size_t i = 0; i < groups.size(); ++i) {
    groups[i].reserveInputs(2);
    groups[i].addInput(i == 0 ? static_cast<const Rasterizable*>(&source)
                              : &groups[i - 1]);
    groups[i].addInput(&mask).withMode(BlendingMode::kDestinationIn);
  }
  allocation::Stats rebuild = allocation::Measure([&]() {
    groups[0].clearInputs();
    groups[0].addInput(&source);
    groups[0].addInput(&mask).withMode(BlendingMode::kDestinationIn);
  });
  EXPECT_EQ(rebuild.count, 0u);
  Offscreen<Argb8888> output(bounds);
  Surface surface(output.output(), 0, 0, bounds, false, color::Transparent,
                  FillMode::kExtents, BlendingMode::kSource);
  surface.drawObject(groups.back());
  allocation::Stats draw =
      allocation::Measure([&]() { surface.drawObject(groups.back()); });
  EXPECT_EQ(draw.count, 0u);
  EXPECT_EQ(draw.peak, 0u);
}

}  // namespace roo_display
