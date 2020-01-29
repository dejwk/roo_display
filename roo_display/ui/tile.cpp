#include "tile.h"

#include "roo_display/core/device.h"

namespace roo_display {

namespace internal {

void TileBase::draw(const Surface& s, const Drawable& content) const {
  Box extents =
      Box::intersect(s.clip_box, border_.extents().translate(s.dx, s.dy));
  if (extents.empty()) return;
  Color bgcolor = alphaBlend(s.bgcolor, bgcolor_);
  Box interior =
      Box::intersect(s.clip_box, border_.interior().translate(s.dx, s.dy));
  if (bgcolor.a() != 0) {
    if (extents.yMin() < interior.yMin()) {
      // Draw the top bg bar.
      s.out->fillRect(extents.xMin(), extents.yMin(), extents.xMax(),
                      interior.yMin() - 1, bgcolor);
    }
    if (extents.xMin() < interior.xMin()) {
      // Draw the left bg bar.
      s.out->fillRect(extents.xMin(), interior.yMin(), interior.xMin() - 1,
                      interior.yMax(), bgcolor);
    }
  }
  Surface inner(s.out, s.dx + border_.x_offset(), s.dy + border_.y_offset(),
                extents, bgcolor);
  inner.drawObject(content);
  if (bgcolor.a() != 0) {
    if (extents.xMax() > interior.xMax()) {
      // Draw the right bg bar.
      s.out->fillRect(interior.xMax() + 1, interior.yMin(), extents.xMax(),
                      interior.yMax(), bgcolor);
    }
    if (extents.yMax() > interior.yMax()) {
      // Draw the bottom bg bar.
      s.out->fillRect(extents.xMin(), interior.yMax() + 1, extents.xMax(),
                      extents.yMax(), bgcolor);
    }
  }
}

}  // namespace internal

}  // namespace roo_display
