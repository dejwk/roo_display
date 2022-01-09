#include "tile.h"

#include "roo_display/core/device.h"
#include "roo_display/filter/background.h"
#include "roo_display/filter/transformed.h"

namespace roo_display {

namespace internal {

void TileBase::draw(const Surface& s, const Drawable& content) const {
  if (background_ != nullptr) {
    BackgroundFilter filter(s.out(), background_, s.dx(), s.dy());
    Surface news(filter, s.dx(), s.dy(), s.clip_box(), color::Transparent,
                 s.fill_mode(), s.paint_mode());
    drawInternal(news, content);
  } else {
    drawInternal(s, content);
  }
}

void TileBase::drawInternal(const Surface& s, const Drawable& content) const {
  FillMode fill_mode = s.fill_mode();
  if (fill_mode_ == FILL_MODE_RECTANGLE) fill_mode = FILL_MODE_RECTANGLE;
  Box extents =
      Box::intersect(s.clip_box(), border_.extents().translate(s.dx(), s.dy()));
  if (extents.empty()) return;
  Color bgcolor = alphaBlend(s.bgcolor(), bgcolor_);
  Box interior = Box::intersect(s.clip_box(),
                                border_.interior().translate(s.dx(), s.dy()));
  if (interior.empty()) {
    if (fill_mode == FILL_MODE_RECTANGLE) {
      s.out().fillRect(
          s.paint_mode(),
          Box(extents.xMin(), extents.yMin(), extents.xMax(), extents.yMax()),
          bgcolor);
    }
    return;
  }
  if (fill_mode == FILL_MODE_RECTANGLE) {
    if (extents.yMin() < interior.yMin()) {
      // Draw the top bg bar.
      s.out().fillRect(s.paint_mode(),
                       Box(extents.xMin(), extents.yMin(), extents.xMax(),
                           interior.yMin() - 1),
                       bgcolor);
    }
    if (extents.xMin() < interior.xMin()) {
      // Draw the left bg bar.
      s.out().fillRect(s.paint_mode(),
                       Box(extents.xMin(), interior.yMin(), interior.xMin() - 1,
                           interior.yMax()),
                       bgcolor);
    }
  }
  Surface inner(s.out(), s.dx() + border_.x_offset(),
                s.dy() + border_.y_offset(), extents, bgcolor, fill_mode,
                s.paint_mode());
  inner.drawObject(content);
  if (fill_mode == FILL_MODE_RECTANGLE) {
    if (extents.xMax() > interior.xMax()) {
      // Draw the right bg bar.
      s.out().fillRect(s.paint_mode(),
                       Box(interior.xMax() + 1, interior.yMin(), extents.xMax(),
                           interior.yMax()),
                       bgcolor);
    }
    if (extents.yMax() > interior.yMax()) {
      // Draw the bottom bg bar.
      s.out().fillRect(s.paint_mode(),
                       Box(extents.xMin(), interior.yMax() + 1, extents.xMax(),
                           extents.yMax()),
                       bgcolor);
    }
  }
}

}  // namespace internal

}  // namespace roo_display
