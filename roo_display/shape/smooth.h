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

SmoothShape SmoothArc(FpPoint center, float radius, float width,
                      float angle_start, float angle_end, Color active_color,
                      Color inactive_color, Color interior_color,
                      EndingStyle ending_style = ENDING_ROUNDED);

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
    float r;
    float ri;
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
    float angle_start;
    float angle_end;
    Color outline_active_color;
    Color outline_inactive_color;
    Color interior_color;
    // Endpoints of the cut line at the start angle.
    float start_x_ro;
    float start_y_ro;
    float start_x_rc;
    float start_y_rc;
    // Endpoints of the cut line at the end angle.
    float end_x_ro;
    float end_y_ro;
    float end_x_rc;
    float end_y_rc;
    // 1 / width = 1 - (ro - ri).
    float inv_width;
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

  friend SmoothShape SmoothArc(FpPoint center, float radius, float width,
                               float angle_start, float angle_end,
                               Color active_color, Color inactive_color,
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
