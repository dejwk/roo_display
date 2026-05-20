#include "roo_display/shape/impl/smooth_wedge.h"

#include <math.h>

#include "roo_display/core/buffered_drawing.h"

namespace roo_display {

SmoothShape SmoothWedgedLine(FpPoint a, float width_a, FpPoint b, float width_b,
                             Color color, EndingStyle ending_style) {
  if (width_a < 0.0f) width_a = 0.0f;
  if (width_b < 0.0f) width_b = 0.0f;
  if (width_a == 0 && width_b == 0) {
    return SmoothShape();
  }
  float ar = width_a / 2.0f;
  float br = width_b / 2.0f;
  if (ending_style == ENDING_ROUNDED) {
    float dr = sqrtf((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    if (ar + dr <= br) {
      // 'a' fits in 'b'.
      return SmoothFilledCircle({b.x, b.y}, br, color);
    } else if (br + dr <= ar) {
      // 'b' fits in 'a'.
      return SmoothFilledCircle({a.x, a.y}, ar, color);
    }
  }
  if (a.x == b.x && a.y == b.y) {
    // Must be ENDING_FLAT. Nothing to draw.
    return SmoothShape();
  }
  // if (a.y == b.y && ar == br && ar >= 1.0f) {
  //   // Horizontal line.
  //   if (ending_style == ENDING_ROUNDED) {
  //     return SmoothFilledRoundRect(a.x - ar, a.y - ar, b.x + br, b.y + br,
  //     ar, color);
  //   } else {
  //     // return SmoothFilledRect(a.x, a.y - ar, b.x, b.y + br, color);
  //   }
  // }

  SmoothShape::Wedge wedge{a.x, a.y, b.x,   b.y,
                           ar,  br,  color, (ending_style == ENDING_ROUNDED)};
  ar -= 0.5f;
  br -= 0.5f;
  int16_t x0 = (int32_t)floorf(fminf(a.x - ar, b.x - br));
  int16_t y0 = (int32_t)floorf(fminf(a.y - ar, b.y - br));
  int16_t x1 = (int32_t)ceilf(fmaxf(a.x + ar, b.x + br));
  int16_t y1 = (int32_t)ceilf(fmaxf(a.y + ar, b.y + br));
  return SmoothShape(Box(x0, y0, x1, y1), std::move(wedge));
}

SmoothShape SmoothThickLine(FpPoint a, FpPoint b, float width, Color color,
                            EndingStyle ending_style) {
  return SmoothWedgedLine(a, width, b, width, color, ending_style);
}

SmoothShape SmoothLine(FpPoint a, FpPoint b, Color color) {
  return SmoothThickLine(a, b, 1.0f, color, ENDING_ROUNDED);
}

SmoothShape SmoothRotatedFilledRect(FpPoint center, float width, float height,
                                    float angle, Color color) {
  if (width <= 0.0f || height <= 0.0f) return SmoothShape();
  // We will use width as thickness.
  // Before rotation, and adjusted by the center point, the line endpoints have
  // coordinates (0, height/2) and (0, -height/2).
  float dx = sinf(angle) * (height * 0.5f);
  float dy = cosf(angle) * (height * 0.5f);
  return SmoothThickLine({center.x + dx, center.y + dy},
                         {center.x - dx, center.y - dy}, width, color,
                         ENDING_FLAT);
}

namespace {

struct WedgeDrawSpec {
  float r;
  float dr;
  float bax;
  float bay;
  float hd;
  float sqrt_hd;
  uint8_t max_alpha;
  bool round_endings;
};

inline uint8_t GetWedgeShapeAlpha(const WedgeDrawSpec& spec, float xpax,
                                  float ypay) {
  // hn (h-numerator) is zero at 'a' and equal to spec.hd at 'b'.
  float hn = (xpax * spec.bax + ypay * spec.bay);
  // h is hn/hd trimmed to [0, 1]. It represents the position of the point along
  // the line, with 0 at 'a' and 1 at 'b', that is closest to the point under
  // test. We will call that point 'P'.
  float h = hn < 0.0f ? 0.0f : hn > spec.hd ? 1.0f : hn / spec.hd;
  // (dx, dy) is a distance vector (from point under test to 'P').
  float dx = xpax - spec.bax * h;
  float dy = ypay - spec.bay * h;
  float l_sq = dx * dx + dy * dy;
  // adj_dist says how far the (widened) boundary of our shape is from 'P'.
  float adj_dist = spec.r - h * spec.dr;
  float adj_dist_sq = adj_dist * adj_dist;
  // Check if point under test is outside the (widened) boundary.
  if (adj_dist_sq < l_sq) return 0;
  if (!spec.round_endings) {
    // We now additionally require that the non-trimmed 'h' is not outside [0,
    // 1] by more than a pixel distance (and if it is within a pixel distance,
    // we anti-alias it).
    float d1 = (hn / spec.hd) * spec.sqrt_hd;
    float d2 = (1.0f - hn / spec.hd) * spec.sqrt_hd;

    if (d1 < .5f) {
      if (d1 < -0.5f) return 0;
      float d = adj_dist - sqrtf(l_sq);
      return uint8_t(std::min(d, (d1 + 0.5f)) * spec.max_alpha);
    }
    if (d2 < .5f) {
      if (d2 < -0.5f) return 0;
      float d = adj_dist - sqrtf(l_sq);
      return uint8_t(std::min(d, (d2 + 0.5f)) * spec.max_alpha);
    }
  }
  // Handle sub-pixel widths.
  if (adj_dist < 1.0f) {
    float l = sqrtf(l_sq);
    float d;
    if (l + adj_dist < 1.0f) {
      // Line fully within the pixel. Use line thickness as alpha.
      d = 2 * adj_dist - 1.0f;
    } else {
      // Line on the boundary of the pixel. Use the regular formula.
      d = adj_dist - l;
    }
    return (uint8_t)(d * spec.max_alpha);
  }
  // Equivalent to sqrt(l_sq) < adj_dist - 1 (the point is deep inside the
  // (widened) boundary), but without sqrt.
  if (adj_dist_sq - 2 * adj_dist + 1 > l_sq) return spec.max_alpha;
  // d is the distance of tested point, inside the (widened) boundary, from the
  // (widened) boundary. We now know it is in the [0-1] range.
  float l = sqrtf(l_sq);
  float d = adj_dist - l;
  return (uint8_t)(d * spec.max_alpha);
}

}  // namespace

namespace internal {

void DrawWedge(const SmoothShape::Wedge& wedge, const Surface& s,
               const Box& box) {
  int16_t dx = s.dx();
  int16_t dy = s.dy();
  float ax = wedge.ax + dx;
  float ay = wedge.ay + dy;
  float bx = wedge.bx + dx;
  float by = wedge.by + dy;
  uint8_t max_alpha = wedge.color.a();
  float bax = bx - ax;
  float bay = by - ay;
  float bay_dsq = bax * bax + bay * bay;
  WedgeDrawSpec spec{
      .r = wedge.ar + 0.5f,
      .dr = wedge.ar - wedge.br,
      .bax = wedge.bx - wedge.ax,
      .bay = wedge.by - wedge.ay,
      .hd = bay_dsq,
      .sqrt_hd = sqrtf(bay_dsq),
      .max_alpha = wedge.color.a(),
      .round_endings = wedge.round_endings,
  };

  // Establish x start and y start.
  int32_t ys = wedge.ay;
  if ((ax - wedge.ar) > (bx - wedge.br)) ys = wedge.by;

  uint8_t alpha = max_alpha;
  float xpax, ypay;
  bool can_minimize_scan =
      spec.round_endings && s.fill_mode() != FillMode::kExtents;

  ys += dy;
  if (ys < box.yMin()) ys = box.yMin();
  int32_t xs = box.xMin();
  Color preblended = AlphaBlend(s.bgcolor(), wedge.color);

  BufferedPixelWriter writer(s.out(), s.blending_mode());

  // Scan bounding box from ys down, calculate pixel intensity from distance
  // to line.
  for (int32_t yp = ys; yp <= box.yMax(); yp++) {
    bool endX = false;  // Flag to skip pixels
    ypay = yp - ay;
    for (int32_t xp = xs; xp <= box.xMax(); xp++) {
      if (endX && alpha == 0 && can_minimize_scan) break;  // Skip right side.
      xpax = xp - ax;
      // alpha = getAlpha(ar, xpax, ypay, bax, bay, rdt, max_alpha);
      alpha = GetWedgeShapeAlpha(spec, xpax, ypay);
      if (alpha == 0 && can_minimize_scan) continue;
      // Track the edge to minimize calculations.
      if (!endX) {
        endX = true;
        if (can_minimize_scan) {
          xs = xp;
        }
      }
      if (alpha != 0 || s.fill_mode() == FillMode::kExtents) {
        writer.writePixel(
            xp, yp,
            alpha == max_alpha
                ? preblended
                : AlphaBlend(s.bgcolor(), wedge.color.withA(alpha)));
      }
    }
  }

  if (ys > box.yMax()) ys = box.yMax();
  // Reset x start to left side of box.
  xs = box.xMin();
  // Scan bounding box from ys-1 up, calculate pixel intensity from distance
  // to line.
  for (int32_t yp = ys - 1; yp >= box.yMin(); yp--) {
    bool endX = false;  // Flag to skip pixels
    ypay = yp - ay;
    for (int32_t xp = xs; xp <= box.xMax(); xp++) {
      if (endX && alpha == 0 && can_minimize_scan)
        break;  // Skip right side of drawn line.
      xpax = xp - ax;
      // alpha = getAlpha(ar, xpax, ypay, bax, bay, rdt, max_alpha);
      alpha = GetWedgeShapeAlpha(spec, xpax, ypay);
      if (alpha == 0 && can_minimize_scan) continue;
      // Track line boundary.
      if (!endX) {
        endX = true;
        if (can_minimize_scan) {
          xs = xp;
        }
      }
      if (alpha != 0 || s.fill_mode() == FillMode::kExtents) {
        writer.writePixel(
            xp, yp,
            alpha == max_alpha
                ? preblended
                : AlphaBlend(s.bgcolor(), wedge.color.withA(alpha)));
      }
    }
  }
}

void ReadWedgeColors(const SmoothShape::Wedge& wedge, const int16_t* x,
                     const int16_t* y, uint32_t count, Color* result) {
  // This default rasterizable implementation seems to be ~50% slower than
  // drawTo (but it allows to use wedges as backgrounds or overlays, e.g.
  // indicator needles).
  float bax = wedge.bx - wedge.ax;
  float bay = wedge.by - wedge.ay;
  float bay_dsq = bax * bax + bay * bay;
  WedgeDrawSpec spec{
      // Widen the boundary to simplify calculations.
      .r = wedge.ar + 0.5f,
      .dr = wedge.ar - wedge.br,
      .bax = wedge.bx - wedge.ax,
      .bay = wedge.by - wedge.ay,
      .hd = bay_dsq,
      .sqrt_hd = sqrtf(bay_dsq),
      .max_alpha = wedge.color.a(),
      .round_endings = wedge.round_endings,
  };
  while (count-- > 0) {
    *result++ = wedge.color.withA(
        GetWedgeShapeAlpha(spec, *x++ - wedge.ax, *y++ - wedge.ay));
  }
}

}  // namespace internal

}  // namespace roo_display