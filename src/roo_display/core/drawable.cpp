#include "roo_display/core/drawable.h"

#include "roo_display/core/device.h"

namespace roo_display {

namespace {

const char* ToString(FillMode mode) {
  switch (mode) {
    case FillMode::kExtents:
      return "FillMode::kExtents";
    case FillMode::kVisible:
      return "FillMode::kVisible";
  }
  return "FillMode::(unknown)";
}

class EmptyDrawable : public Drawable {
 public:
  Box extents() const override { return Box(); }
};

}  // namespace

roo_logging::Stream& operator<<(roo_logging::Stream& os, FillMode mode) {
  return os << ToString(mode);
}

void Drawable::drawTo(const Surface& s) const {
  if (s.fill_mode() == FillMode::kExtents) {
    Box box = Box::Intersect(s.clip_box(), extents().translate(s.dx(), s.dy()));
    if (!box.empty()) {
      s.out().fillRect(BlendingMode::kSource, box, s.bgcolor());
    }
  }
  drawInteriorTo(s);
}

const Drawable* Drawable::Empty() {
  static EmptyDrawable empty;
  return &empty;
}

}  // namespace roo_display
