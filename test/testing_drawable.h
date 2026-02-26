
#include <string>

#include "roo_display.h"
#include "testing.h"

namespace roo_display {

// Helper class to reduce test boilerplate.
template <typename ColorMode>
class FakeScreen {
 public:
  FakeScreen(int16_t width, int16_t height, Color color = color::Transparent)
      : device_(width, height, color), display_(device_) {}

  FakeScreen(Box extents, Color color = color::Transparent)
      : device_(std::move(extents), color), display_(device_) {}

  void Draw(const Drawable& drawable, int16_t dx, int16_t dy,
            Color bgcolor = color::Transparent,
            FillMode fill_mode = FillMode::kVisible,
            BlendingMode blending_mode = BlendingMode::kSourceOver) {
    DrawingContext dc(display_);
    dc.setBackgroundColor(bgcolor);
    dc.setFillMode(fill_mode);
    dc.setBlendingMode(blending_mode);
    dc.draw(drawable, dx, dy);
  }

  void Draw(const Drawable& drawable, int16_t x, int16_t y, const Box& clip_box,
            Color bgcolor = color::Transparent,
            FillMode fill_mode = FillMode::kVisible,
            BlendingMode blending_mode = BlendingMode::kSourceOver) {
    DrawingContext dc(display_);
    dc.setClipBox(clip_box);
    dc.setBackgroundColor(bgcolor);
    dc.setFillMode(fill_mode);
    dc.setBlendingMode(blending_mode);
    dc.draw(drawable, x, y);
  }

  Box extents() const {
    return Box(0, 0, device_.effective_width() - 1,
               device_.effective_height() - 1);
  }

  const ColorMode& color_mode() const { return device_.color_mode(); }

  std::unique_ptr<TestColorStream> createRawStream() const {
    return device_.createRawStream();
  }

 private:
  FakeOffscreen<ColorMode> device_;
  Display display_;
};

}  // namespace roo_display