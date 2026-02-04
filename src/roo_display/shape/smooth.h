#pragma once

#include "roo_display.h"
#include "roo_display/color/color.h"
#include "roo_display/shape/point.h"

namespace roo_display {

/// Line ending style for smooth shapes.
enum EndingStyle {
  ENDING_ROUNDED = 0,
  ENDING_FLAT = 1,
};

/// Represents one of the supported smooth shapes.
///
/// A common representation is used because shapes overlap (e.g., many reduce
/// to a filled circle).
class SmoothShape;

/// Create a 1-pixel-wide anti-aliased line.
SmoothShape SmoothLine(FpPoint a, FpPoint b, Color color);

/// Create a line with width and ending style.
SmoothShape SmoothThickLine(FpPoint a, FpPoint b, float width, Color color,
                            EndingStyle ending_style = ENDING_ROUNDED);

/// Create a wedged line with different start/end widths.
SmoothShape SmoothWedgedLine(FpPoint a, float width_a, FpPoint b, float width_b,
                             Color color,
                             EndingStyle ending_style = ENDING_ROUNDED);

/// Create an outlined round-rect.
SmoothShape SmoothRoundRect(float x0, float y0, float x1, float y1,
                            float radius, Color color,
                            Color interior_color = color::Transparent);

/// Create an outlined round-rect with thickness.
SmoothShape SmoothThickRoundRect(float x0, float y0, float x1, float y1,
                                 float radius, float thickness, Color color,
                                 Color interior_color = color::Transparent);

/// Create a filled round-rect.
SmoothShape SmoothFilledRoundRect(float x0, float y0, float x1, float y1,
                                  float radius, Color color);

// SmoothShape SmoothFilledRect(float x0, float y0, float x1, float y1,
//                                   Color color);

/// Create a circle (optionally with interior color).
SmoothShape SmoothCircle(FpPoint center, float radius, Color color,
                         Color interior_color = color::Transparent);

/// Create a circle with thickness (ring).
SmoothShape SmoothThickCircle(FpPoint center, float radius, float thickness,
                              Color color,
                              Color interior_color = color::Transparent);

/// Create a filled circle.
SmoothShape SmoothFilledCircle(FpPoint center, float radius, Color color);

/// Create a rotated filled rectangle.
SmoothShape SmoothRotatedFilledRect(FpPoint center, float width, float height,
                                    float angle, Color color);

/// Create a 1-pixel-wide arc.
SmoothShape SmoothArc(FpPoint center, float radius, float angle_start,
                      float angle_end, Color color);

/// Create an arc with thickness.
SmoothShape SmoothThickArc(FpPoint center, float radius, float thickness,
                           float angle_start, float angle_end, Color color,
                           EndingStyle ending_style = ENDING_ROUNDED);

/// Create a pie slice.
SmoothShape SmoothPie(FpPoint center, float radius, float angle_start,
                      float angle_end, Color color);

/// Create an arc with background and interior colors.
SmoothShape SmoothThickArcWithBackground(
    FpPoint center, float radius, float thickness, float angle_start,
    float angle_end, Color active_color, Color inactive_color,
    Color interior_color, EndingStyle ending_style = ENDING_ROUNDED);

/// Create a filled triangle.
SmoothShape SmoothFilledTriangle(FpPoint a, FpPoint b, FpPoint c, Color color);

// Implementation details follow.

/// Smooth (anti-aliased) shape rasterizable.
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
    float ri;         // ro - width
    float rm;         // (ro - ri) / 2
    float ro_sq_adj;  // ro * ro + 0.25f
    float ri_sq_adj;  // ri * ri + 0.25f
    float rm_sq_adj;  // rm * rm + 0.25f
    float angle_start;
    float angle_end;
    Color outline_active_color;
    Color outline_inactive_color;
    Color interior_color;
    // Rectangle fully within the inner circle.
    Box inner_mid;
    // X-slope at angle start: (x_ro - x_rc) / |ro - rc|.
    float start_x_slope;
    // Y-slope at angle start: (y_ro - y_rc) / |ro - rc|.
    float start_y_slope;
    // Endpoint of the cut line at the start angle, mid-band, relative to the
    // circle center.
    float start_x_rc;
    float start_y_rc;
    // X-slope at angle end: (x_ro - x_rc) / |ro - rc|.
    float end_x_slope;
    // Y-slope at angle end: (y_ro - y_rc) / |ro - rc|.
    float end_y_slope;
    // Endpoint of the cut line at the end angle, mid-band, relative to the
    // circle center.
    float end_x_rc;
    float end_y_rc;
    float start_cutoff_x_slope;
    float start_cutoff_y_slope;
    float end_cutoff_x_slope;
    float end_cutoff_y_slope;
    bool round_endings;
    bool range_angle_sharp;
    bool nonempty_cutoff;
    bool cutoff_angle_sharp;

    // Lower 4 bits: when a bit is set, it means that the arc includes the
    // entire given quadrant.
    uint8_t quadrants_;
  };

  struct Triangle {
    float x1;
    float y1;
    float dx12;
    float dy12;
    float x2;
    float y2;
    float dx23;
    float dy23;
    float x3;
    float y3;
    float dx31;
    float dy31;
    Color color;
  };

  struct Pixel {
    Color color;
  };

  SmoothShape();

  Box extents() const override { return extents_; }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override;

  bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                     Color* result) const override;

 private:
  friend SmoothShape SmoothWedgedLine(FpPoint a, float width_a, FpPoint b,
                                      float width_b, Color color,
                                      EndingStyle ending_style);

  friend SmoothShape SmoothThickRoundRect(float x0, float y0, float x1,
                                          float y1, float radius,
                                          float thickness, Color color,
                                          Color interior_color);

  friend SmoothShape SmoothRotatedFilledRect(FpPoint center, float width,
                                             float height, float angle,
                                             Color color);

  friend SmoothShape SmoothThickArcImpl(
      FpPoint center, float radius, float thickness, float angle_start,
      float angle_end, Color active_color, Color inactive_color,
      Color interior_color, EndingStyle ending_style, bool trim_to_active);

  friend SmoothShape SmoothFilledTriangle(FpPoint a, FpPoint b, FpPoint c,
                                          Color color);

  void drawTo(const Surface& s) const override;

  enum Kind {
    EMPTY = 0,
    WEDGE = 1,
    ROUND_RECT = 2,
    ARC = 3,
    TRIANGLE = 4,
    PIXEL = 5
  };

  SmoothShape(Box extents, Wedge wedge);
  SmoothShape(Box extents, RoundRect round_rect);
  SmoothShape(Box extents, Arc arc);
  SmoothShape(Box extents, Triangle triangle);
  SmoothShape(int16_t x, int16_t y, Pixel pixel);

  Kind kind_;
  Box extents_;
  union {
    Wedge wedge_;
    RoundRect round_rect_;
    Arc arc_;
    Triangle triangle_;
    Pixel pixel_;
  };
};

}  // namespace roo_display
