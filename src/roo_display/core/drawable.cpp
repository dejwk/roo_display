#include "roo_display/core/drawable.h"

#include "roo_display/core/device.h"

namespace roo_display {

void Drawable::drawTo(const Surface& s) const {
  if (s.fill_mode() == FILL_MODE_RECTANGLE) {
    Box box = Box::Intersect(s.clip_box(), extents().translate(s.dx(), s.dy()));
    if (!box.empty()) {
      s.out().fillRect(BLENDING_MODE_SOURCE, box, s.bgcolor());
    }
  }
  drawInteriorTo(s);
}

namespace {

class EmptyDrawable : public Drawable {
 public:
  Box extents() const override { return Box(); }
};

}  // namespace

const Drawable* Drawable::Empty() {
  static EmptyDrawable empty;
  return &empty;
}

}  // namespace roo_display
