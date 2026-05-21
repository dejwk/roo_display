#include "roo_display/shape/impl/smooth_round_rect.h"

#include <math.h>

#include "roo_display/core/buffered_drawing.h"
#include "roo_display/shape/impl/smooth_internal.h"

namespace roo_display {

namespace internal {

NormalizedSingleRadiusRoundRect NormalizeSingleRadiusRoundRect(
    float x0, float y0, float x1, float y1, float radius, float thickness) {
  if (radius < 0.0f) radius = 0.0f;
  if (thickness < 0.0f) thickness = 0.0f;
  if (x1 < x0) std::swap(x0, x1);
  if (y1 < y0) std::swap(y0, y1);

  float width = x1 - x0;
  float height = y1 - y0;
  float max_radius = ((width < height) ? width : height) * 0.5f;
  if (radius > max_radius) radius = max_radius;

  float delta = thickness * 0.5f;
  float inner_x0 = x0 + delta;
  float inner_y0 = y0 + delta;
  float inner_x1 = x1 - delta;
  float inner_y1 = y1 - delta;
  float inner_radius = radius - delta;
  if (inner_radius < 0.0f) inner_radius = 0.0f;

  NormalizedRoundRectKind kind =
      (inner_x0 >= inner_x1 || inner_y0 >= inner_y1)
          ? NormalizedRoundRectKind::kFilled
          : (inner_radius > 0.0f ? NormalizedRoundRectKind::kRoundInner
                                 : NormalizedRoundRectKind::kRectInner);

  return {kind,       x0 - delta,     y0 - delta,  x1 + delta,
          y1 + delta, radius + delta, inner_x0,    inner_y0,
          inner_x1,   inner_y1,       inner_radius};
}

}  // namespace internal

SmoothShape SmoothThickRoundRect(float x0, float y0, float x1, float y1,
                                 float radius, float thickness, Color color,
                                 Color interior_color) {
  if (thickness < 0) thickness = 0;
  const internal::NormalizedSingleRadiusRoundRect normalized =
      internal::NormalizeSingleRadiusRoundRect(x0, y0, x1, y1, radius,
                                               thickness);
  const bool rectangular_inner =
      normalized.kind == internal::NormalizedRoundRectKind::kRectInner;
  if (normalized.kind == internal::NormalizedRoundRectKind::kFilled) {
    thickness = 0.0f;
    interior_color = color;
  }
  if (thickness == 0.0f) {
    color = interior_color;
  }

  x0 = normalized.outer_x0;
  y0 = normalized.outer_y0;
  x1 = normalized.outer_x1;
  y1 = normalized.outer_y1;
  radius = normalized.outer_radius;
  float width = x1 - x0;
  float height = y1 - y0;
  float outer_radius = radius;
  if (outer_radius < 0) outer_radius = 0;
  float inner_radius = outer_radius - thickness;
  if (inner_radius < 0) inner_radius = 0;
  const float inner_total_width =
      std::max(0.0f, normalized.inner_x1 - normalized.inner_x0);
  const float inner_total_height =
      std::max(0.0f, normalized.inner_y1 - normalized.inner_y0);
  Box extents((int16_t)floorf(x0 + 0.5f), (int16_t)floorf(y0 + 0.5f),
              (int16_t)ceilf(x1 - 0.5f), (int16_t)ceilf(y1 - 0.5f));
  if (extents.xMin() == extents.xMax() && extents.yMin() == extents.yMax()) {
    // Special case: the rect fits into a single pixel. Let's just calculate the
    // color and get it over with.
    float outer_area = std::min<float>(
        1.0f,
        std::max<float>(0.0f, width * height -
                                  (4.0f - M_PI) * outer_radius * outer_radius));
    float inner_area = std::min<float>(
        1.0f,
        std::max<float>(0.0f, inner_total_width * inner_total_height -
                                  (4.0f - M_PI) * inner_radius * inner_radius));
    color = color.withA(color.a() * outer_area);
    interior_color = interior_color.withA(interior_color.a() * inner_area);
    Color pixel_color = AlphaBlend(color, interior_color);
    if (pixel_color.a() == 0) return SmoothShape();
    return SmoothShape(extents.xMin(), extents.yMin(),
                       SmoothShape::Pixel{color});
  }

  x0 += outer_radius;
  y0 += outer_radius;
  x1 -= outer_radius;
  y1 -= outer_radius;
  const SmoothShape::RoundRect::InnerBoundaryMode inner_boundary_mode =
      rectangular_inner ? SmoothShape::RoundRect::InnerBoundaryMode::kRect
                        : SmoothShape::RoundRect::InnerBoundaryMode::kRound;
  Box inner_mid;
  Box inner_wide;
  Box inner_tall;
  if (rectangular_inner) {
    const Box inner_rect_interior((int16_t)ceilf(normalized.inner_x0 + 0.5f),
                                  (int16_t)ceilf(normalized.inner_y0 + 0.5f),
                                  (int16_t)floorf(normalized.inner_x1 - 0.5f),
                                  (int16_t)floorf(normalized.inner_y1 - 0.5f));
    inner_mid = inner_rect_interior;
    inner_wide = inner_rect_interior;
    inner_tall = inner_rect_interior;
  } else {
    float diagonal_offset = sqrtf(0.5f) * inner_radius;
    inner_mid = Box((int16_t)ceilf(x0 - diagonal_offset + 0.5f),
                    (int16_t)ceilf(y0 - diagonal_offset + 0.5f),
                    (int16_t)floorf(x1 + diagonal_offset - 0.5f),
                    (int16_t)floorf(y1 + diagonal_offset - 0.5f));
    inner_wide = Box(
        (int16_t)ceilf(x0 - inner_radius + 0.5f), (int16_t)ceilf(y0 + 0.5f),
        (int16_t)floorf(x1 + inner_radius - 0.5f), (int16_t)floorf(y1 - 0.5f));
    inner_tall = Box(
        (int16_t)ceilf(x0 + 0.5f), (int16_t)ceilf(y0 - inner_radius + 0.5f),
        (int16_t)floorf(x1 - 0.5f), (int16_t)floorf(y1 + inner_radius - 0.5f));
  }
  SmoothShape::RoundRect spec{x0,
                              y0,
                              x1,
                              y1,
                              outer_radius,
                              inner_radius,
                              outer_radius * outer_radius + 0.25f,
                              inner_radius * inner_radius + 0.25f,
                              normalized.inner_x0,
                              normalized.inner_y0,
                              normalized.inner_x1,
                              normalized.inner_y1,
                              color,
                              interior_color,
                              std::move(inner_mid),
                              std::move(inner_wide),
                              std::move(inner_tall),
                              inner_boundary_mode};

  return SmoothShape(std::move(extents), std::move(spec));
}

SmoothShape SmoothRoundRect(float x0, float y0, float x1, float y1,
                            float radius, Color color, Color interior_color) {
  return SmoothThickRoundRect(x0, y0, x1, y1, radius, 1, color, interior_color);
}

SmoothShape SmoothFilledRoundRect(float x0, float y0, float x1, float y1,
                                  float radius, Color color) {
  return SmoothThickRoundRect(x0, y0, x1, y1, radius, 0, color, color);
}

// SmoothShape SmoothFilledRect(float x0, float y0, float x1, float y1,
//                              Color color) {
//   return SmoothFilledRoundRect(x0, y0, x1, y1, 0.5, color);
// }

SmoothShape SmoothThickCircle(FpPoint center, float radius, float thickness,
                              Color color, Color interior_color) {
  return SmoothThickRoundRect(center.x - radius, center.y - radius,
                              center.x + radius, center.y + radius, radius,
                              thickness, color, interior_color);
}

SmoothShape SmoothCircle(FpPoint center, float radius, Color color,
                         Color interior_color) {
  return SmoothThickCircle(center, radius, 1, color, interior_color);
}

SmoothShape SmoothFilledCircle(FpPoint center, float radius, Color color) {
  return SmoothThickCircle(center, radius, 0, color, color);
}

namespace {

inline bool UsesRectInnerBoundary(const SmoothShape::RoundRect& rect) {
  return rect.inner_boundary_mode ==
         SmoothShape::RoundRect::InnerBoundaryMode::kRect;
}

inline bool PointInsideRoundRectInteriorHelper(
    const SmoothShape::RoundRect& rect, int16_t x, int16_t y) {
  return rect.inner_mid.contains(x, y) || rect.inner_wide.contains(x, y) ||
         rect.inner_tall.contains(x, y);
}

inline bool BoxInsideRoundRectInteriorHelper(const SmoothShape::RoundRect& rect,
                                             const Box& box) {
  return rect.inner_mid.contains(box) || rect.inner_wide.contains(box) ||
         rect.inner_tall.contains(box);
}

inline float CalcDistSqRectInnerBoundary(const SmoothShape::RoundRect& rect,
                                         float xt, float yt);

inline float CalcDistSqRoundRectInnerBoundary(
    const SmoothShape::RoundRect& rect, float outer_d_squared, int16_t x,
    int16_t y) {
  return UsesRectInnerBoundary(rect) ? CalcDistSqRectInnerBoundary(rect, x, y)
                                     : outer_d_squared;
}

inline Color GetSmoothRoundRectPixelColor(const SmoothShape::RoundRect& rect,
                                          int16_t x, int16_t y) {
  const bool uses_rect_inner = UsesRectInnerBoundary(rect);
  if (rect.ri < 0.5) {
    // Note: checking this unconditionally is correct, but since it tends to
    // apply to few pixels, it is not a net win. But for ri < 0.5, it is needed
    // for correctness, due to the math below.
    if (PointInsideRoundRectInteriorHelper(rect, x, y)) {
      return rect.interior_color;
    }
  }
  float ref_x = std::min(std::max((float)x, rect.x0), rect.x1);
  float ref_y = std::min(std::max((float)y, rect.y0), rect.y1);
  float dx = x - ref_x;
  float dy = y - ref_y;

  Color interior = rect.interior_color;
  // // This only applies to a handful of pixels and seems to slow things down.
  // if (abs(dx) + abs(dy) < spec.ri - 0.5) {
  //   return spec.interior;
  // }
  float d_squared = dx * dx + dy * dy;
  float ro = rect.ro;
  float ri = rect.ri;
  Color outline = rect.outline_color;

  if (!uses_rect_inner && d_squared <= rect.ri_sq_adj - ri && ri >= 0.5f) {
    // Point fully within the interior.
    return interior;
  }
  if (d_squared >= rect.ro_sq_adj + ro) {
    // Point fully outside the round rectangle.
    return color::Transparent;
  }
  bool fully_within_outer = d_squared <= rect.ro_sq_adj - ro;
  float inner_d_squared =
      CalcDistSqRoundRectInnerBoundary(rect, d_squared, x, y);
  bool fully_outside_inner = ro == ri || inner_d_squared >= rect.ri_sq_adj + ri;
  if (fully_within_outer && fully_outside_inner) {
    // Point fully within the outline band.
    return outline;
  }
  // if (dx == 0 && dy == 0 && rect.inner_mid.contains(x, y)) {
  //   return interior;
  // }
  // Point is on either of the boundaries; need anti-aliasing.
  // Note: replacing with integer sqrt (iterative, loop-unrolled, 24-bit) slows
  // things down. At 64-bit not loop-unrolled, does so dramatically.
  float outer_d = sqrtf(d_squared);
  if (fully_outside_inner) {
    // On the outer boundary.
    return outline.withA((uint8_t)(outline.a() * (ro - outer_d + 0.5f)));
  }
  float inner_d = uses_rect_inner ? sqrtf(inner_d_squared) : outer_d;
  if (fully_within_outer) {
    // On the inner boundary.
    float outline_coverage = 1.0f - (ri - inner_d + 0.5f);
    if (!uses_rect_inner && ri < 0.5f &&
        (roundf(rect.x0 - ri) == roundf(rect.x1 + ri) ||
         roundf(rect.y0 - ri) == roundf(rect.y1 + ri))) {
      // Pesky corner case: the interior is hair-thin. Approximate.
      outline_coverage = std::min(1.0f, outline_coverage * 2.0f);
    }
    const uint16_t outline_fraction = outline_coverage * 256;
    return InterpolateColors(interior, outline, outline_fraction);
  }
  // On both bounderies (e.g. the band is very thin).
  const uint16_t outer_fraction = (ro - outer_d + 0.5f) * 256;
  const uint16_t interior_fraction = (ri - inner_d + 0.5f) * 256;
  const uint16_t outline_fraction = outer_fraction - interior_fraction;
  return MixColors(interior, interior_fraction, outline, outline_fraction);
}

inline float CalcDistSqRect(float x0, float y0, float x1, float y1, float xt,
                            float yt) {
  float dx = (xt <= x0 ? xt - x0 : xt >= x1 ? xt - x1 : 0.0f);
  float dy = (yt <= y0 ? yt - y0 : yt >= y1 ? yt - y1 : 0.0f);
  return dx * dx + dy * dy;
}

inline float CalcDistSqRectInnerBoundary(const SmoothShape::RoundRect& rect,
                                         float xt, float yt) {
  return CalcDistSqRect(rect.inner_x0, rect.inner_y0, rect.inner_x1,
                        rect.inner_y1, xt, yt);
}

inline bool BoxFullyOutsideRectInnerBoundary(const SmoothShape::RoundRect& rect,
                                             float x_min, float y_min,
                                             float x_max, float y_max) {
  return x_max <= rect.inner_x0 - 0.5f || x_min >= rect.inner_x1 + 0.5f ||
         y_max <= rect.inner_y0 - 0.5f || y_min >= rect.inner_y1 + 0.5f;
}

// Called for rectangles with area <= 64 pixels.
internal::round_rect::AreaType DetermineRectColorForRoundRectImpl(
    const SmoothShape::RoundRect& rect, const Box& box) {
  if (BoxInsideRoundRectInteriorHelper(rect, box)) {
    return internal::round_rect::AreaType::kInterior;
  }
  const bool uses_rect_inner = UsesRectInnerBoundary(rect);
  float xMin = box.xMin() - 0.5f;
  float yMin = box.yMin() - 0.5f;
  float xMax = box.xMax() + 0.5f;
  float yMax = box.yMax() + 0.5f;
  float dtl = CalcDistSqRect(rect.x0, rect.y0, rect.x1, rect.y1, xMin, yMin);
  float dtr = CalcDistSqRect(rect.x0, rect.y0, rect.x1, rect.y1, xMax, yMin);
  float dbl = CalcDistSqRect(rect.x0, rect.y0, rect.x1, rect.y1, xMin, yMax);
  float dbr = CalcDistSqRect(rect.x0, rect.y0, rect.x1, rect.y1, xMax, yMax);

  float r_min_sq = rect.ri_sq_adj - rect.ri;
  // Check if the rect falls entirely inside the interior boundary.
  if (!uses_rect_inner && dtl < r_min_sq && dtr < r_min_sq && dbl < r_min_sq &&
      dbr < r_min_sq) {
    return internal::round_rect::AreaType::kInterior;
  }

  float r_max_sq = rect.ro_sq_adj + rect.ro;
  // Check if the rect falls entirely outside the boundary (in one of the 4
  // corners).
  if (xMax < rect.x0) {
    if (yMax < rect.y0) {
      if (dbr >= r_max_sq) {
        return internal::round_rect::AreaType::kExterior;
      }
    } else if (yMin > rect.y1) {
      if (dtr >= r_max_sq) {
        return internal::round_rect::AreaType::kExterior;
      }
    }
  } else if (xMin > rect.x1) {
    if (yMax < rect.y0) {
      if (dbl >= r_max_sq) {
        return internal::round_rect::AreaType::kExterior;
      }
    } else if (yMin > rect.y1) {
      if (dtl >= r_max_sq) {
        return internal::round_rect::AreaType::kExterior;
      }
    }
  }
  if (uses_rect_inner) {
    if (!BoxFullyOutsideRectInnerBoundary(rect, xMin, yMin, xMax, yMax)) {
      return internal::round_rect::AreaType::kMixed;
    }
    if (xMin >= rect.x0 && xMax <= rect.x1 && yMin >= rect.y0 &&
        yMax <= rect.y1) {
      return internal::round_rect::AreaType::kOutline;
    }

    float outer_full_sq = rect.ro_sq_adj - rect.ro;
    if (xMax <= rect.x1) {
      if (yMax <= rect.y1) {
        if (dtl < outer_full_sq && dbr < outer_full_sq) {
          return internal::round_rect::AreaType::kOutline;
        }
      } else if (yMin >= rect.y0) {
        if (dtr < outer_full_sq && dbl < outer_full_sq) {
          return internal::round_rect::AreaType::kOutline;
        }
      }
    } else if (xMin >= rect.x0) {
      if (yMax <= rect.y1) {
        if (dtr < outer_full_sq && dbl < outer_full_sq) {
          return internal::round_rect::AreaType::kOutline;
        }
      } else if (yMin >= rect.y0) {
        if (dtl < outer_full_sq && dbr < outer_full_sq) {
          return internal::round_rect::AreaType::kOutline;
        }
      }
    }
    return internal::round_rect::AreaType::kMixed;
  }
  // If all corners are in the same quadrant, and all corners are within the
  // ring, then the rect is also within the ring.
  if (xMax <= rect.x1) {
    if (yMax <= rect.y1) {
      float r_ring_max_sq = rect.ro_sq_adj - rect.ro;
      float r_ring_min_sq = rect.ri_sq_adj + rect.ri;
      if (dtl < r_ring_max_sq && dtl > r_ring_min_sq && dbr < r_ring_max_sq &&
          dbr > r_ring_min_sq) {
        return internal::round_rect::AreaType::kOutline;
      }
    } else if (yMin >= rect.y0) {
      float r_ring_max_sq = rect.ro_sq_adj - rect.ro;
      float r_ring_min_sq = rect.ri_sq_adj + rect.ri;
      if (dtr < r_ring_max_sq && dtr > r_ring_min_sq && dbl < r_ring_max_sq &&
          dbl > r_ring_min_sq) {
        return internal::round_rect::AreaType::kOutline;
      }
    }
  } else if (xMin >= rect.x0) {
    if (yMax <= rect.y1) {
      float r_ring_max_sq = rect.ro_sq_adj - rect.ro;
      float r_ring_min_sq = rect.ri_sq_adj + rect.ri;
      if (dtr < r_ring_max_sq && dtr > r_ring_min_sq && dbl < r_ring_max_sq &&
          dbl > r_ring_min_sq) {
        return internal::round_rect::AreaType::kOutline;
      }
    } else if (yMin >= rect.y0) {
      float r_ring_max_sq = rect.ro_sq_adj - rect.ro;
      float r_ring_min_sq = rect.ri_sq_adj + rect.ri;
      if (dtl < r_ring_max_sq && dtl > r_ring_min_sq && dbr < r_ring_max_sq &&
          dbr > r_ring_min_sq) {
        return internal::round_rect::AreaType::kOutline;
      }
    }
  }
  // Slow case; evaluate every pixel from the rectangle.
  return internal::round_rect::AreaType::kMixed;
}

  inline uint64_t Square(int32_t value) {
    return (uint64_t)value * (uint64_t)value;
  }

  // Tracks the largest doubled-x still inside a fixed squared-radius threshold
  // as successive scanlines move across the rounded cap.
  class MaxCircleDxTracker {
   public:
    MaxCircleDxTracker(uint64_t limit_sq4, int32_t initial_dx)
        : limit_sq4_(limit_sq4), dx_(initial_dx) {}

    void Update(uint64_t dy_sq4) {
      if (dy_sq4 > limit_sq4_) {
        dx_ = -1;
        return;
      }
      if (dx_ < 0) dx_ = 0;
      while (dx_ > 0 && Square(dx_) + dy_sq4 > limit_sq4_) {
        --dx_;
      }
      while (Square(dx_ + 1) + dy_sq4 <= limit_sq4_) {
        ++dx_;
      }
    }

    int32_t dx() const { return dx_; }

   private:
    const uint64_t limit_sq4_;
    int32_t dx_;
  };

  // Tracks the smallest doubled-x already outside a fixed squared-radius
  // threshold as successive scanlines move across the rounded cap.
  class MinCircleDxTracker {
   public:
    MinCircleDxTracker(uint64_t limit_sq4, int32_t initial_dx)
        : limit_sq4_(limit_sq4), dx_(initial_dx) {}

    void Update(uint64_t dy_sq4) {
      if (dy_sq4 >= limit_sq4_) {
        dx_ = 0;
        return;
      }
      if (dx_ < 0) dx_ = 0;
      while (dx_ > 0 && Square(dx_ - 1) + dy_sq4 >= limit_sq4_) {
        --dx_;
      }
      while (Square(dx_) + dy_sq4 < limit_sq4_) {
        ++dx_;
      }
    }

    int32_t dx() const { return dx_; }

   private:
    const uint64_t limit_sq4_;
    int32_t dx_;
  };

class RoundRectStream : public PixelStream {
 public:
  // Emits a round-rect in row-major order without buffering the whole row.
  // Each scanline is decomposed into a small number of uniform segments:
  // transparent, outline, interior, plus narrow 'slow' edge segments that
  // still need per-pixel anti-aliased evaluation.
  RoundRectStream(const SmoothShape::RoundRect& rect, Box bounds)
      : rect_(rect),
        bounds_(std::move(bounds)),
        x_(bounds_.xMin()),
        y_(bounds_.yMin()),
        segment_count_(0),
        segment_index_(0),
        row_ready_(false),
        x0x2_(lroundf(2 * rect.x0)),
        y0x2_(lroundf(2 * rect.y0)),
        x1x2_(lroundf(2 * rect.x1)),
        y1x2_(lroundf(2 * rect.y1)),
        rox2_(lroundf(2 * rect.ro)),
        rix2_(lroundf(2 * rect.ri)),
        outer_full_sq4_(Square(std::max<int32_t>(0, rox2_ - 1))),
        outer_trans_sq4_(Square(rox2_ + 1)),
        has_inner_full_(rix2_ > 0),
        inner_full_sq4_(has_inner_full_ ? Square(rix2_ - 1) : 0),
        inner_out_sq4_(Square(rix2_ + 1)),
        outer_full_tracker_(outer_full_sq4_, std::max<int32_t>(-1, rox2_ - 1)),
        outer_trans_tracker_(outer_trans_sq4_, rox2_ + 1),
        inner_full_tracker_(
          inner_full_sq4_,
          has_inner_full_ ? std::max<int32_t>(-1, rix2_ - 1) : -1),
        inner_out_tracker_(inner_out_sq4_, rix2_ + 1) {}

  void Read(Color* buf, uint16_t count) override {
    while (count > 0) {
      if (!row_ready_) PrepareRow();
      if (segment_index_ >= segment_count_) return;
      const Segment& segment = segments_[segment_index_];
      uint16_t batch = segment.end_x - x_ + 1;
      if (batch > count) batch = count;
      switch (segment.kind) {
        case SegmentKind::kTransparent:
          FillColor(buf, batch, color::Transparent);
          break;
        case SegmentKind::kInterior:
          FillColor(buf, batch, rect_.interior_color);
          break;
        case SegmentKind::kOutline:
          FillColor(buf, batch, rect_.outline_color);
          break;
        case SegmentKind::kSlow:
          for (uint16_t i = 0; i < batch; ++i) {
            buf[i] = GetSmoothRoundRectPixelColor(rect_, x_ + i, y_);
          }
          break;
      }
      buf += batch;
      count -= batch;
      x_ += batch;
      if (x_ > segment.end_x) {
        ++segment_index_;
      }
      if (x_ > bounds_.xMax()) {
        x_ = bounds_.xMin();
        ++y_;
        row_ready_ = false;
      }
    }
  }

  void Skip(uint32_t count) override {
    uint16_t width = bounds_.width();
    y_ += count / width;
    x_ += count % width;
    if (x_ > bounds_.xMax()) {
      x_ -= width;
      ++y_;
    }
    row_ready_ = false;
    segment_count_ = 0;
    segment_index_ = 0;
  }

 private:
  enum class SegmentKind : uint8_t {
    kTransparent,
    kInterior,
    kOutline,
    kSlow,
  };

  struct Segment {
    int16_t start_x;
    int16_t end_x;
    SegmentKind kind;
  };

  inline int16_t FloorDiv2(int32_t value) {
    return value >= 0 ? value / 2 : -(((-value) + 1) / 2);
  }

  inline int16_t CeilDiv2(int32_t value) {
    return value >= 0 ? (value + 1) / 2 : -((-value) / 2);
  }

  void UpdateThresholds(uint64_t dy_sq4) {
    outer_full_tracker_.Update(dy_sq4);
    outer_trans_tracker_.Update(dy_sq4);
    if (has_inner_full_) {
      inner_full_tracker_.Update(dy_sq4);
    }
    inner_out_tracker_.Update(dy_sq4);
  }

  void AddSegment(int16_t start_x, int16_t end_x, SegmentKind kind) {
    if (start_x > end_x) return;
    if (segment_count_ > 0) {
      Segment& prev = segments_[segment_count_ - 1];
      if (prev.kind == kind && prev.end_x + 1 == start_x) {
        prev.end_x = end_x;
        return;
      }
    }
    segments_[segment_count_++] = Segment{start_x, end_x, kind};
  }

  SegmentKind ClassifyZeroDx(uint64_t dy_sq4) const {
    if (has_inner_full_ && dy_sq4 <= inner_full_sq4_) {
      return SegmentKind::kInterior;
    }
    if (dy_sq4 >= outer_trans_sq4_) {
      return SegmentKind::kTransparent;
    }
    if (dy_sq4 <= outer_full_sq4_ &&
        (rox2_ == rix2_ || dy_sq4 >= inner_out_sq4_)) {
      return SegmentKind::kOutline;
    }
    return SegmentKind::kSlow;
  }

  void PrepareLeftSide(int16_t start_x, int16_t end_x, int32_t trans_dx_min,
                       int32_t outer_dx_max, int32_t inner_out_dx_min,
                       int32_t inner_dx_max) {
    if (start_x > end_x) return;
    int16_t transparent_end = FloorDiv2(x0x2_ - trans_dx_min);
    int16_t outer_start = CeilDiv2(x0x2_ - outer_dx_max);
    int16_t outline_end = FloorDiv2(x0x2_ - inner_out_dx_min);
    int16_t interior_start = CeilDiv2(x0x2_ - inner_dx_max);

    AddSegment(start_x, std::min(end_x, transparent_end),
               SegmentKind::kTransparent);
    AddSegment(std::max(start_x, (int16_t)(transparent_end + 1)),
               std::min(end_x, (int16_t)(outer_start - 1)), SegmentKind::kSlow);
    AddSegment(std::max(start_x, outer_start), std::min(end_x, outline_end),
               SegmentKind::kOutline);
    AddSegment(std::max(start_x, (int16_t)(outline_end + 1)),
               std::min(end_x, (int16_t)(interior_start - 1)),
               SegmentKind::kSlow);
    AddSegment(std::max(start_x, interior_start), end_x,
               SegmentKind::kInterior);
  }

  void PrepareRightSide(int16_t start_x, int16_t end_x, int32_t trans_dx_min,
                        int32_t outer_dx_max, int32_t inner_out_dx_min,
                        int32_t inner_dx_max) {
    if (start_x > end_x) return;
    int16_t interior_end = FloorDiv2(x1x2_ + inner_dx_max);
    int16_t outline_start = CeilDiv2(x1x2_ + inner_out_dx_min);
    int16_t outer_end = FloorDiv2(x1x2_ + outer_dx_max);
    int16_t transparent_start = CeilDiv2(x1x2_ + trans_dx_min);

    AddSegment(start_x, std::min(end_x, interior_end), SegmentKind::kInterior);
    AddSegment(std::max(start_x, (int16_t)(interior_end + 1)),
               std::min(end_x, (int16_t)(outline_start - 1)),
               SegmentKind::kSlow);
    AddSegment(std::max(start_x, outline_start), std::min(end_x, outer_end),
               SegmentKind::kOutline);
    AddSegment(std::max(start_x, (int16_t)(outer_end + 1)),
               std::min(end_x, (int16_t)(transparent_start - 1)),
               SegmentKind::kSlow);
    AddSegment(std::max(start_x, transparent_start), end_x,
               SegmentKind::kTransparent);
  }

  void PrepareRow() {
    segment_count_ = 0;
    segment_index_ = 0;
    row_ready_ = true;
    if (y_ > bounds_.yMax()) return;

    // Distances are tracked in doubled coordinates so the +/-0.5 pixel margins
    // used by the anti-aliased round-rect tests become exact integer
    // thresholds. For the current row we reduce the shape to three zones:
    // left cap, center slab, right cap.
    int32_t yx2 = 2 * y_;
    int32_t ref_yx2 = std::min(std::max(yx2, y0x2_), y1x2_);
    uint64_t dyx2 = (uint64_t)std::abs(yx2 - ref_yx2);
    uint64_t dy_sq4 = dyx2 * dyx2;

    UpdateThresholds(dy_sq4);

    int16_t zero_start = CeilDiv2(x0x2_);
    int16_t zero_end = FloorDiv2(x1x2_);

    PrepareLeftSide(
        bounds_.xMin(), std::min(bounds_.xMax(), (int16_t)(zero_start - 1)),
        outer_trans_tracker_.dx(), outer_full_tracker_.dx(),
        inner_out_tracker_.dx(), inner_full_tracker_.dx());

    int16_t center_start = std::max(bounds_.xMin(), zero_start);
    int16_t center_end = std::min(bounds_.xMax(), zero_end);
    if (center_start <= center_end) {
      AddSegment(center_start, center_end, ClassifyZeroDx(dy_sq4));
    }

    PrepareRightSide(std::max(bounds_.xMin(), (int16_t)(zero_end + 1)),
                     bounds_.xMax(), outer_trans_tracker_.dx(),
                     outer_full_tracker_.dx(), inner_out_tracker_.dx(),
                     inner_full_tracker_.dx());
  }

  const SmoothShape::RoundRect& rect_;
  Box bounds_;
  int16_t x_;
  int16_t y_;
  // Worst case is five left-cap spans, one center span, and five right-cap
  // spans before adjacent equal-color spans are merged.
  Segment segments_[11];
  uint8_t segment_count_;
  uint8_t segment_index_;
  bool row_ready_;

  // Stored in doubled coordinates so half-pixel AA thresholds stay integral.
  const int32_t x0x2_;
  const int32_t y0x2_;
  const int32_t x1x2_;
  const int32_t y1x2_;
  const int32_t rox2_;
  const int32_t rix2_;

  // Squared doubled-radius thresholds used to classify pixels without floating
  // point math while scanning. 'full' means definitely inside a region;
  // 'trans/out' means definitely outside it.
  const uint64_t outer_full_sq4_;
  const uint64_t outer_trans_sq4_;
  const bool has_inner_full_;
  const uint64_t inner_full_sq4_;
  const uint64_t inner_out_sq4_;

  // Current scanline state for the incremental circle trackers.
  MaxCircleDxTracker outer_full_tracker_;
  MinCircleDxTracker outer_trans_tracker_;
  MaxCircleDxTracker inner_full_tracker_;
  MinCircleDxTracker inner_out_tracker_;
};

// Rectangular-inner round rects share the same rounded outer boundary but keep
// the inner geometry axis-aligned, so the stream can recover long uniform runs
// without touching the rounded-inner hot path.
class RectInnerRoundRectStream : public PixelStream {
 public:
  RectInnerRoundRectStream(const SmoothShape::RoundRect& rect, Box bounds)
      : rect_(rect),
        bounds_(std::move(bounds)),
        x_(bounds_.xMin()),
        y_(bounds_.yMin()),
        segment_count_(0),
        segment_index_(0),
        row_ready_(false),
        x0x2_(lroundf(2 * rect.x0)),
        y0x2_(lroundf(2 * rect.y0)),
        x1x2_(lroundf(2 * rect.x1)),
        y1x2_(lroundf(2 * rect.y1)),
        rox2_(lroundf(2 * rect.ro)),
        outer_full_sq4_(Square(std::max<int32_t>(0, rox2_ - 1))),
        outer_trans_sq4_(Square(rox2_ + 1)),
        outer_full_tracker_(outer_full_sq4_, std::max<int32_t>(-1, rox2_ - 1)),
        outer_trans_tracker_(outer_trans_sq4_, rox2_ + 1),
        inner_inside_start_((int16_t)ceilf(rect.inner_x0)),
        inner_inside_end_((int16_t)floorf(rect.inner_x1)),
        inner_outside_left_end_((int16_t)floorf(rect.inner_x0 - 0.5f)),
        inner_outside_right_start_((int16_t)ceilf(rect.inner_x1 + 0.5f)) {}

  void Read(Color* buf, uint16_t count) override {
    while (count > 0) {
      if (!row_ready_) PrepareRow();
      if (segment_index_ >= segment_count_) return;
      const Segment& segment = segments_[segment_index_];
      uint16_t batch = segment.end_x - x_ + 1;
      if (batch > count) batch = count;
      switch (segment.kind) {
        case SegmentKind::kSolid:
          FillColor(buf, batch, segment.color);
          break;
        case SegmentKind::kSlow:
          for (uint16_t i = 0; i < batch; ++i) {
            buf[i] = GetSmoothRoundRectPixelColor(rect_, x_ + i, y_);
          }
          break;
      }
      buf += batch;
      count -= batch;
      x_ += batch;
      if (x_ > segment.end_x) {
        ++segment_index_;
      }
      if (x_ > bounds_.xMax()) {
        x_ = bounds_.xMin();
        ++y_;
        row_ready_ = false;
      }
    }
  }

  void Skip(uint32_t count) override {
    uint16_t width = bounds_.width();
    y_ += count / width;
    x_ += count % width;
    if (x_ > bounds_.xMax()) {
      x_ -= width;
      ++y_;
    }
    row_ready_ = false;
    segment_count_ = 0;
    segment_index_ = 0;
  }

 private:
  enum class SegmentKind : uint8_t {
    kSolid,
    kSlow,
  };

  struct Segment {
    int16_t start_x;
    int16_t end_x;
    SegmentKind kind;
    Color color;
  };

  inline int16_t FloorDiv2(int32_t value) {
    return value >= 0 ? value / 2 : -(((-value) + 1) / 2);
  }

  inline int16_t CeilDiv2(int32_t value) {
    return value >= 0 ? (value + 1) / 2 : -((-value) / 2);
  }

  void AddSolidSegment(int16_t start_x, int16_t end_x, Color color) {
    if (start_x > end_x) return;
    if (segment_count_ > 0) {
      Segment& prev = segments_[segment_count_ - 1];
      if (prev.kind == SegmentKind::kSolid && prev.color == color &&
          prev.end_x + 1 == start_x) {
        prev.end_x = end_x;
        return;
      }
    }
    segments_[segment_count_++] =
        Segment{start_x, end_x, SegmentKind::kSolid, color};
  }

  void AddSlowSegment(int16_t start_x, int16_t end_x) {
    if (start_x > end_x) return;
    if (segment_count_ > 0) {
      Segment& prev = segments_[segment_count_ - 1];
      if (prev.kind == SegmentKind::kSlow && prev.end_x + 1 == start_x) {
        prev.end_x = end_x;
        return;
      }
    }
    segments_[segment_count_++] =
        Segment{start_x, end_x, SegmentKind::kSlow, color::Transparent};
  }

  void AddSampledSolidSegment(int16_t start_x, int16_t end_x) {
    if (start_x > end_x) return;
    AddSolidSegment(start_x, end_x,
                    GetSmoothRoundRectPixelColor(rect_, start_x, y_));
  }

  void AddFullOuterSegments(int16_t start_x, int16_t end_x) {
    if (start_x > end_x) return;

    const float row_y = y_;
    if (row_y <= rect_.inner_y0 - 0.5f || row_y >= rect_.inner_y1 + 0.5f) {
      AddSolidSegment(start_x, end_x, rect_.outline_color);
      return;
    }

    AddSolidSegment(start_x, std::min(end_x, inner_outside_left_end_),
                    rect_.outline_color);
    AddSlowSegment(std::max(start_x, (int16_t)(inner_outside_left_end_ + 1)),
                   std::min(end_x, (int16_t)(inner_inside_start_ - 1)));

    const int16_t inside_start = std::max(start_x, inner_inside_start_);
    const int16_t inside_end = std::min(end_x, inner_inside_end_);
    if (inside_start <= inside_end) {
      const bool has_full_interior = !rect_.inner_mid.empty() &&
                                     y_ >= rect_.inner_mid.yMin() &&
                                     y_ <= rect_.inner_mid.yMax();
      if (has_full_interior) {
        AddSampledSolidSegment(
            inside_start,
            std::min(inside_end, (int16_t)(rect_.inner_mid.xMin() - 1)));
        AddSolidSegment(std::max(inside_start, rect_.inner_mid.xMin()),
                        std::min(inside_end, rect_.inner_mid.xMax()),
                        rect_.interior_color);
        AddSampledSolidSegment(
            std::max(inside_start, (int16_t)(rect_.inner_mid.xMax() + 1)),
            inside_end);
      } else {
        AddSampledSolidSegment(inside_start, inside_end);
      }
    }

    AddSlowSegment(std::max(start_x, (int16_t)(inner_inside_end_ + 1)),
                   std::min(end_x, (int16_t)(inner_outside_right_start_ - 1)));
    AddSolidSegment(std::max(start_x, inner_outside_right_start_), end_x,
                    rect_.outline_color);
  }

  void PrepareRow() {
    segment_count_ = 0;
    segment_index_ = 0;
    row_ready_ = true;
    if (y_ > bounds_.yMax()) return;

    int32_t yx2 = 2 * y_;
    int32_t ref_yx2 = std::min(std::max(yx2, y0x2_), y1x2_);
    uint64_t dyx2 = (uint64_t)std::abs(yx2 - ref_yx2);
    uint64_t dy_sq4 = dyx2 * dyx2;

    outer_full_tracker_.Update(dy_sq4);
    outer_trans_tracker_.Update(dy_sq4);

    if (outer_trans_tracker_.dx() == 0) {
      AddSolidSegment(bounds_.xMin(), bounds_.xMax(), color::Transparent);
      return;
    }

    int16_t transparent_end = FloorDiv2(x0x2_ - outer_trans_tracker_.dx());
    int16_t transparent_start = CeilDiv2(x1x2_ + outer_trans_tracker_.dx());

    AddSolidSegment(bounds_.xMin(), std::min(bounds_.xMax(), transparent_end),
                    color::Transparent);

    if (outer_full_tracker_.dx() < 0) {
      AddSlowSegment(
          std::max(bounds_.xMin(), (int16_t)(transparent_end + 1)),
          std::min(bounds_.xMax(), (int16_t)(transparent_start - 1)));
    } else {
      int16_t outer_full_start = CeilDiv2(x0x2_ - outer_full_tracker_.dx());
      int16_t outer_full_end = FloorDiv2(x1x2_ + outer_full_tracker_.dx());
      AddSlowSegment(std::max(bounds_.xMin(), (int16_t)(transparent_end + 1)),
                     std::min(bounds_.xMax(), (int16_t)(outer_full_start - 1)));
      AddFullOuterSegments(std::max(bounds_.xMin(), outer_full_start),
                           std::min(bounds_.xMax(), outer_full_end));
      AddSlowSegment(
          std::max(bounds_.xMin(), (int16_t)(outer_full_end + 1)),
          std::min(bounds_.xMax(), (int16_t)(transparent_start - 1)));
    }

    AddSolidSegment(std::max(bounds_.xMin(), transparent_start), bounds_.xMax(),
                    color::Transparent);
  }

  const SmoothShape::RoundRect& rect_;
  Box bounds_;
  int16_t x_;
  int16_t y_;
  Segment segments_[11];
  uint8_t segment_count_;
  uint8_t segment_index_;
  bool row_ready_;

  const int32_t x0x2_;
  const int32_t y0x2_;
  const int32_t x1x2_;
  const int32_t y1x2_;
  const int32_t rox2_;

  const uint64_t outer_full_sq4_;
  const uint64_t outer_trans_sq4_;
  MaxCircleDxTracker outer_full_tracker_;
  MinCircleDxTracker outer_trans_tracker_;

  const int16_t inner_inside_start_;
  const int16_t inner_inside_end_;
  const int16_t inner_outside_left_end_;
  const int16_t inner_outside_right_start_;
};

struct RoundRectDrawSpec {
  DisplayOutput* out;
  FillMode fill_mode;
  BlendingMode blending_mode;
  Color bgcolor;
  Color pre_blended_outline;
  Color pre_blended_interior;
};

// Called for rectangles with area <= 64 pixels.
inline void FillSubrectOfRoundRect(const SmoothShape::RoundRect& rect,
                                   const RoundRectDrawSpec& spec,
                                   const Box& box) {
  Color interior = rect.interior_color;
  Color outline = rect.outline_color;
  switch (DetermineRectColorForRoundRectImpl(rect, box)) {
    case internal::round_rect::AreaType::kExterior: {
      if (spec.fill_mode == FillMode::kExtents) {
        spec.out->fillRect(spec.blending_mode, box, spec.bgcolor);
      }
      return;
    }
    case internal::round_rect::AreaType::kInterior: {
      if (spec.fill_mode == FillMode::kExtents ||
          interior != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box, spec.pre_blended_interior);
      }
      return;
    }
    case internal::round_rect::AreaType::kOutline: {
      if (spec.fill_mode == FillMode::kExtents ||
          outline != color::Transparent) {
        spec.out->fillRect(spec.blending_mode, box, spec.pre_blended_outline);
        return;
      }
    }
    default:
      break;
  }

  // Slow case; evaluate every pixel from the rectangle.
  if (spec.fill_mode == FillMode::kVisible) {
    BufferedPixelWriter writer(*spec.out, spec.blending_mode);
    for (int16_t y = box.yMin(); y <= box.yMax(); ++y) {
      for (int16_t x = box.xMin(); x <= box.xMax(); ++x) {
        Color c = GetSmoothRoundRectPixelColor(rect, x, y);
        if (c == color::Transparent) continue;
        writer.writePixel(x, y,
                          c == interior  ? spec.pre_blended_interior
                          : c == outline ? spec.pre_blended_outline
                                         : AlphaBlend(spec.bgcolor, c));
      }
    }
  } else {
    Color color[64];
    int cnt = 0;
    for (int16_t y = box.yMin(); y <= box.yMax(); ++y) {
      for (int16_t x = box.xMin(); x <= box.xMax(); ++x) {
        Color c = GetSmoothRoundRectPixelColor(rect, x, y);
        color[cnt++] = c.a() == 0      ? spec.bgcolor
                       : c == interior ? spec.pre_blended_interior
                       : c == outline  ? spec.pre_blended_outline
                                       : AlphaBlend(spec.bgcolor, c);
      }
    }
    spec.out->setAddress(box, spec.blending_mode);
    spec.out->write(color, cnt);
  }
}

void FillSubrectangle(const SmoothShape::RoundRect& rect,
                      const RoundRectDrawSpec& spec, const Box& box) {
  const int16_t xMinOuter = (box.xMin() / 8) * 8;
  const int16_t yMinOuter = (box.yMin() / 8) * 8;
  const int16_t xMaxOuter = (box.xMax() / 8) * 8 + 7;
  const int16_t yMaxOuter = (box.yMax() / 8) * 8 + 7;
  for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
    for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
      FillSubrectOfRoundRect(
          rect, spec,
          Box(std::max(x, box.xMin()), std::max(y, box.yMin()),
              std::min((int16_t)(x + 7), box.xMax()),
              std::min((int16_t)(y + 7), box.yMax())));
    }
  }
}

void DrawRoundRectImpl(SmoothShape::RoundRect rect, const Surface& s,
                       const Box& box) {
  RoundRectDrawSpec spec{
      .out = &s.out(),
      .fill_mode = s.fill_mode(),
      .blending_mode = s.blending_mode(),
      .bgcolor = s.bgcolor(),
      .pre_blended_outline = AlphaBlend(s.bgcolor(), rect.outline_color),
      .pre_blended_interior = AlphaBlend(s.bgcolor(), rect.interior_color),
  };
  if (s.dx() != 0 || s.dy() != 0) {
    rect.x0 += s.dx();
    rect.y0 += s.dy();
    rect.x1 += s.dx();
    rect.y1 += s.dy();
    rect.inner_x0 += s.dx();
    rect.inner_y0 += s.dy();
    rect.inner_x1 += s.dx();
    rect.inner_y1 += s.dy();
    rect.inner_wide = rect.inner_wide.translate(s.dx(), s.dy());
    rect.inner_mid = rect.inner_mid.translate(s.dx(), s.dy());
    rect.inner_tall = rect.inner_tall.translate(s.dx(), s.dy());
  }
  {
    uint32_t pixel_count = box.area();
    if (pixel_count <= 64) {
      FillSubrectOfRoundRect(rect, spec, box);
      return;
    }
  }
  if (rect.inner_mid.width() <= 16) {
    FillSubrectangle(rect, spec, box);
  } else {
    const Box& inner = rect.inner_mid;
    const Box top = Box::Intersect(
        Box(box.xMin(), box.yMin(), box.xMax(), inner.yMin() - 1), box);
    const Box left = Box::Intersect(
        Box(box.xMin(), inner.yMin(), inner.xMin() - 1, inner.yMax()), box);
    const Box clipped_inner = Box::Intersect(inner, box);
    const Box right = Box::Intersect(
        Box(inner.xMax() + 1, inner.yMin(), box.xMax(), inner.yMax()), box);
    const Box bottom = Box::Intersect(
        Box(box.xMin(), inner.yMax() + 1, box.xMax(), box.yMax()), box);
    if (!top.empty()) {
      FillSubrectangle(rect, spec, top);
    }
    if (!left.empty()) {
      FillSubrectangle(rect, spec, left);
    }
    if (!clipped_inner.empty() && (s.fill_mode() == FillMode::kExtents ||
                                   rect.interior_color != color::Transparent)) {
      s.out().fillRect(s.blending_mode(), clipped_inner,
                       spec.pre_blended_interior);
    }
    if (!right.empty()) {
      FillSubrectangle(rect, spec, right);
    }
    if (!bottom.empty()) {
      FillSubrectangle(rect, spec, bottom);
    }
  }
}

}  // namespace

namespace internal {

round_rect::AreaType DetermineRectColorForRoundRect(
    const SmoothShape::RoundRect& rect, const Box& box) {
  return DetermineRectColorForRoundRectImpl(rect, box);
}

bool ReadColorRectOfRoundRect(const SmoothShape::RoundRect& rect, int16_t xMin,
                              int16_t yMin, int16_t xMax, int16_t yMax,
                              Color* result) {
  Box box(xMin, yMin, xMax, yMax);
  switch (DetermineRectColorForRoundRectImpl(rect, box)) {
    case round_rect::AreaType::kExterior: {
      *result = color::Transparent;
      return true;
    }
    case round_rect::AreaType::kInterior: {
      *result = rect.interior_color;
      return true;
    }
    case round_rect::AreaType::kOutline: {
      *result = rect.outline_color;
      return true;
    }
    default:
      break;
  }
  Color* out = result;
  for (int16_t y = yMin; y <= yMax; ++y) {
    for (int16_t x = xMin; x <= xMax; ++x) {
      *out++ = GetSmoothRoundRectPixelColor(rect, x, y);
    }
  }
  // This is now very unlikely to be true, or we would have caught it above.
  // uint32_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
  // Color c = result[0];
  // for (uint32_t i = 1; i < pixel_count; i++) {
  //   if (result[i] != c) return false;
  // }
  // return true;
  return false;
}

void ReadRoundRectColors(const SmoothShape::RoundRect& rect, const int16_t* x,
                         const int16_t* y, uint32_t count, Color* result) {
  while (count-- > 0) {
    if (PointInsideRoundRectInteriorHelper(rect, *x, *y)) {
      *result = rect.interior_color;
    } else {
      *result = GetSmoothRoundRectPixelColor(rect, *x, *y);
    }
    ++x;
    ++y;
    ++result;
  }
}

std::unique_ptr<PixelStream> CreateRoundRectStream(
    const SmoothShape::RoundRect& rect, const Box& bounds) {
  if (UsesRectInnerBoundary(rect)) {
    return std::unique_ptr<PixelStream>(
        new RectInnerRoundRectStream(rect, bounds));
  }
  return std::unique_ptr<PixelStream>(new RoundRectStream(rect, bounds));
}

void DrawRoundRect(SmoothShape::RoundRect rect, const Surface& s,
                   const Box& box) {
  DrawRoundRectImpl(rect, s, box);
}

}  // namespace internal

}  // namespace roo_display