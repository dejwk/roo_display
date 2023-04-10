#pragma once

#include "roo_display.h"
#include "roo_display/color/color.h"

namespace roo_display {

struct FpPoint {
  float x;
  float y;
};

enum EndingStyle {
  ENDING_ROUNDED = 0,
  ENDING_FLAT = 1,
};

// Represents one of the supported smooth shapes. We use a common class, because
// these shapes overlap; e.g. all of them can be reduced to a filled circle.
class SmoothShape;

// Creates a simple 1-pixel-wide anti-aliased line between two points.
SmoothShape SmoothLine(FpPoint a, FpPoint b, Color color);

// Creates a line from a to b, with the specified width, and a specified ending
// style.
SmoothShape SmoothThickLine(FpPoint a, FpPoint b, float width, Color color,
                            EndingStyle ending_style = ENDING_ROUNDED);

// Creates a 'wedged' line from a to b, with specified start and end widths,
// and a specified ending style.
SmoothShape SmoothWedgedLine(FpPoint a, float width_a, FpPoint b, float width_b,
                             Color color,
                             EndingStyle ending_style = ENDING_ROUNDED);

// Creates an outlined, round-cornered rectangle with the specified bounds,
// corner radius, outline thickness, outline color, and (optionally) interior
// color, defaulting to transparent.
SmoothShape SmoothOutlinedRoundRect(float x0, float y0, float x1, float y1,
                                    float radius, float outline_thickness,
                                    Color outline_color,
                                    Color interior_color = color::Transparent);

// Creates a round-rectangle with the specified bounds, corner radius, and
// color.
SmoothShape SmoothFilledRoundRect(float x0, float y0, float x1, float y1,
                                  float radius, Color color);

// Creates a circle with the specified center, radius, outline width, outline
// color, and optionally interior color (defaulting to transparent).
SmoothShape SmoothOutlinedCircle(FpPoint center, float radius,
                                 float outline_thickness, Color outline_color,
                                 Color interior_color = color::Transparent);

// Creates a circle with the specified center, radius, and color.
SmoothShape SmoothFilledCircle(FpPoint center, float radius, Color color);

// Creates a rotated filled rectangle with the specified center point, with,
// height, rotation angle, and color.
SmoothShape SmoothRotatedFilledRect(FpPoint center, float width, float height,
                                    float angle, Color color);

// Creates a 1-pixel-wide arc with the given center, radius, start and end
// angles, and color.
SmoothShape SmoothArc(FpPoint center, float radius, float angle_start,
                      float angle_end, Color color);

// Creates an arc with the given center, radius, and thickness, start and eng
// angles, color, and an optional ending style.
SmoothShape SmoothThickArc(FpPoint center, float radius, float thickness,
                           float angle_start, float angle_end, Color color,
                           EndingStyle ending_style = ENDING_ROUNDED);

// Creates a pie with the given center, radius, start and eng
// angles, and color.
SmoothShape SmoothPie(FpPoint center, float radius, float angle_start,
                      float angle_end, Color color);

// Creates an arc with the given center, radius, and thickness, start and eng
// angles, and color, and additionally, the color of the complement of the arc,
// the color of the circle inside the arc, and the ending style.
SmoothShape SmoothThickArcWithBackground(
    FpPoint center, float radius, float thickness, float angle_start,
    float angle_end, Color active_color, Color inactive_color,
    Color interior_color, EndingStyle ending_style = ENDING_ROUNDED);

// Implementation details follow.

class SmoothShape : public Rasterizable {
 public:
  struct Wedge {
    float ax;
    float ay;
    float bx;
    float by;
    float ar;
    float br;
    Color color;
    bool round_endings;
  };

  struct RoundRect {
    float x0;
    float y0;
    float x1;
    float y1;
    float ro;
    float ri;
    float ro_sq_adj;  // ro * ro + 0.25f
    float ri_sq_adj;  // ri * ri + 0.25f
    Color outline_color;
    Color interior_color;
    Box inner_mid;
    Box inner_wide;
    Box inner_tall;
  };

  struct Arc {
    float xc;
    float yc;
    float ro;
    float ri;
    float ro_sq_adj;  // ro * ro + 0.25f
    float ri_sq_adj;  // ri * ri + 0.25f
    float angle_start;
    float angle_end;
    Color outline_active_color;
    Color outline_inactive_color;
    Color interior_color;
    // Rectangle fully within the inner circle.
    Box inner_mid;
    // (x_ro - x_rc) / |ro - rc|, at angle start.
    float start_dxoc_norm;
    float start_dyoc_norm;
    // Endpoint of the cut line at the start angle, mid-band.
    float start_x_rc;
    float start_y_rc;
    // (x_ro - x_rc) / |ro - rc|, at angle end.
    float end_dxoc_norm;
    float end_dyoc_norm;
    // Endpoint of the cut line at the end angle, mid-band.
    float end_x_rc;
    float end_y_rc;
    // 1 / (width/2) = 1 - (ro - ri) / 2.
    int start_quadrant;
    int end_quadrant;
    bool round_endings;
  };

  Box extents() const override { return extents_; }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override;

  bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                     Color* result) const override;

 private:
  friend SmoothShape SmoothWedgedLine(FpPoint a, float width_a, FpPoint b,
                                      float width_b, Color color,
                                      EndingStyle ending_style);

  friend SmoothShape SmoothOutlinedRoundRect(float x0, float y0, float x1,
                                             float y1, float radius,
                                             float outline_thickness,
                                             Color outline_color,
                                             Color interior_color);

  friend SmoothShape SmoothRotatedFilledRect(FpPoint center, float width,
                                             float height, float angle,
                                             Color color);

  friend SmoothShape SmoothThickArcWithBackground(
      FpPoint center, float radius, float thickness, float angle_start,
      float angle_end, Color active_color, Color inactive_color,
      Color interior_color, EndingStyle ending_style);

  void drawTo(const Surface& s) const override;

  enum Kind { EMPTY = 0, WEDGE = 1, ROUND_RECT = 2, ARC = 3 };

  SmoothShape();
  SmoothShape(Box extents, Wedge wedge);
  SmoothShape(Box extents, RoundRect round_rect);
  SmoothShape(Box extents, Arc arc);

  Kind kind_;
  Box extents_;
  union {
    Wedge wedge_;
    RoundRect round_rect_;
    Arc arc_;
  };
};

}  // namespace roo_display
