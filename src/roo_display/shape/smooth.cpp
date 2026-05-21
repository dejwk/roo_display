#include "roo_display/shape/impl/smooth_arc.h"
#include "roo_display/shape/impl/smooth_pixel.h"
#include "roo_display/shape/impl/smooth_round_rect.h"
#include "roo_display/shape/impl/smooth_triangle.h"
#include "roo_display/shape/impl/smooth_wedge.h"

namespace roo_display {

namespace {

class EmptyStream : public PixelStream {
 public:
  void Read(Color*, uint16_t) override {}

  void Skip(uint32_t) override {}
};

}  // namespace

SmoothShape::SmoothShape()
    : kind_(SmoothShape::EMPTY), extents_(0, 0, -1, -1) {}

SmoothShape::SmoothShape(Box extents, Wedge wedge)
    : kind_(SmoothShape::WEDGE),
      extents_(std::move(extents)),
      wedge_(std::move(wedge)) {}

SmoothShape::SmoothShape(Box extents, RoundRect round_rect)
    : kind_(SmoothShape::ROUND_RECT),
      extents_(std::move(extents)),
      round_rect_(std::move(round_rect)) {}

SmoothShape::SmoothShape(Box extents, RoundRectCorners round_rect_corners)
    : kind_(SmoothShape::ROUND_RECT_CORNERS),
      extents_(std::move(extents)),
      round_rect_corners_(std::move(round_rect_corners)) {}

SmoothShape::SmoothShape(Box extents, Arc arc)
    : kind_(SmoothShape::ARC),
      extents_(std::move(extents)),
      arc_(std::move(arc)) {}

SmoothShape::SmoothShape(Box extents, Triangle triangle)
    : kind_(SmoothShape::TRIANGLE),
      extents_(std::move(extents)),
      triangle_(std::move(triangle)) {}

SmoothShape::SmoothShape(int16_t x, int16_t y, Pixel pixel)
    : kind_(SmoothShape::PIXEL),
      extents_(x, y, x, y),
      pixel_(std::move(pixel)) {}

std::unique_ptr<PixelStream> SmoothShape::createStream() const {
  return createStream(extents());
}

std::unique_ptr<PixelStream> SmoothShape::createStream(
    const Box& clip_box) const {
  Box bounds = Box::Intersect(extents(), clip_box);
  if (bounds.empty()) {
    return std::unique_ptr<PixelStream>(new EmptyStream());
  }
  switch (kind_) {
    case ROUND_RECT:
      return internal::CreateRoundRectStream(round_rect_, bounds);
    default:
      return Rasterizable::createStream(bounds);
  }
}

void SmoothShape::drawTo(const Surface& s) const {
  Box box = Box::Intersect(extents_.translate(s.dx(), s.dy()), s.clip_box());
  if (box.empty()) {
    return;
  }
  switch (kind_) {
    case WEDGE: {
      internal::DrawWedge(wedge_, s, box);
      break;
    }
    case ROUND_RECT: {
      internal::DrawRoundRect(round_rect_, s, box);
      break;
    }
    case ROUND_RECT_CORNERS: {
      internal::DrawRoundRectCorners(round_rect_corners_, s, box);
      break;
    }
    case ARC: {
      internal::DrawArc(arc_, s, box);
      break;
    }
    case TRIANGLE: {
      internal::DrawTriangle(triangle_, s, box);
      break;
    }
    case PIXEL: {
      internal::DrawPixel(pixel_, s, box);
    }
    case EMPTY: {
      return;
    }
  }
}

void SmoothShape::readColors(const int16_t* x, const int16_t* y, uint32_t count,
                             Color* result) const {
  switch (kind_) {
    case WEDGE: {
      internal::ReadWedgeColors(wedge_, x, y, count, result);
      break;
    }
    case ROUND_RECT: {
      internal::ReadRoundRectColors(round_rect_, x, y, count, result);
      break;
    }
    case ROUND_RECT_CORNERS: {
      internal::ReadRoundRectCornersColors(round_rect_corners_, x, y, count,
                                           result);
      break;
    }
    case ARC: {
      internal::ReadArcColors(arc_, x, y, count, result);
      break;
    }
    case TRIANGLE: {
      internal::ReadTriangleColors(triangle_, x, y, count, result);
      break;
    }
    case PIXEL: {
      while (count-- > 0) {
        *result = pixel_.color;
      }
      break;
    }
    case EMPTY: {
      while (count-- > 0) {
        *result++ = color::Transparent;
      }
      break;
    }
  }
}

bool SmoothShape::readColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                                int16_t yMax, Color* result) const {
  switch (kind_) {
    case WEDGE: {
      return Rasterizable::readColorRect(xMin, yMin, xMax, yMax, result);
    }
    case ROUND_RECT: {
      return internal::ReadColorRectOfRoundRect(round_rect_, xMin, yMin, xMax,
                                                yMax, result);
    }
    case ROUND_RECT_CORNERS: {
      return internal::ReadColorRectOfRoundRectCorners(
          round_rect_corners_, xMin, yMin, xMax, yMax, result);
    }
    case ARC: {
      return internal::ReadColorRectOfArc(arc_, xMin, yMin, xMax, yMax, result);
    }
    case TRIANGLE: {
      return internal::ReadColorRectOfTriangle(triangle_, xMin, yMin, xMax,
                                               yMax, result);
    }
    case PIXEL: {
      *result = pixel_.color;
      return true;
    }
    case EMPTY: {
      *result = color::Transparent;
      return true;
    }
  }
  *result = color::Transparent;
  return true;
}

bool SmoothShape::readUniformColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                                       int16_t yMax, Color* result) const {
  switch (kind_) {
    case EMPTY: {
      *result = color::Transparent;
      return true;
    }
    case PIXEL: {
      *result = pixel_.color;
      return true;
    }
    case ROUND_RECT: {
      Box box(xMin, yMin, xMax, yMax);
      switch (internal::DetermineRectColorForRoundRect(round_rect_, box)) {
        case internal::round_rect::AreaType::kExterior:
          *result = color::Transparent;
          return true;
        case internal::round_rect::AreaType::kInterior:
          *result = round_rect_.interior_color;
          return true;
        case internal::round_rect::AreaType::kOutline:
          *result = round_rect_.outline_color;
          return true;
        default:
          return false;
      }
    }
    case ROUND_RECT_CORNERS: {
      Box box(xMin, yMin, xMax, yMax);
      switch (internal::DetermineRectColorForRoundRectCorners(
          round_rect_corners_, box)) {
        case internal::round_rect::AreaType::kExterior:
          *result = color::Transparent;
          return true;
        case internal::round_rect::AreaType::kInterior:
          *result = round_rect_corners_.interior_color;
          return true;
        case internal::round_rect::AreaType::kOutline:
          *result = round_rect_corners_.outline_color;
          return true;
        default:
          return false;
      }
    }
    case ARC: {
      Box box(xMin, yMin, xMax, yMax);
      switch (internal::DetermineRectColorForArc(arc_, box)) {
        case internal::arc::AreaType::kExterior:
          *result = color::Transparent;
          return true;
        case internal::arc::AreaType::kInterior:
          *result = arc_.interior_color;
          return true;
        case internal::arc::AreaType::kOutlineActive:
          *result = arc_.outline_active_color;
          return true;
        case internal::arc::AreaType::kOutlineInactive:
          *result = arc_.outline_inactive_color;
          return true;
        default:
          return false;
      }
    }
    case TRIANGLE: {
      Box box(xMin, yMin, xMax, yMax);
      switch (internal::DetermineRectColorForTriangle(triangle_, box)) {
        case internal::triangle::AreaType::kExterior:
          *result = color::Transparent;
          return true;
        case internal::triangle::AreaType::kInterior:
          *result = triangle_.color;
          return true;
        default:
          return false;
      }
    }
    default:
      return false;
  }
}

}  // namespace roo_display