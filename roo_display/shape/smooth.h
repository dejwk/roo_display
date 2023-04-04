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

// Anti-aliased wide line with different width at each end and round endings.
class SmoothWedgeShape : public Drawable {
 public:
  Box extents() const override { return extents_; }

 private:
  friend SmoothWedgeShape SmoothWedgedLine(FpPoint a, float width_a, FpPoint b,
                                           float width_b, Color color,
                                           EndingStyle ending_style);

  friend SmoothWedgeShape SmoothThickLine(FpPoint a, FpPoint b, float width,
                                          Color color, EndingStyle ending_style);

  SmoothWedgeShape(FpPoint a, float width_a, FpPoint b, float width_b,
                   Color color, EndingStyle ending_style);

  void drawTo(const Surface& s) const override;

  float ax_;
  float ay_;
  float bx_;
  float by_;
  float aw_;
  float bw_;
  Color color_;
  bool round_endings_;
  Box extents_;
};

SmoothWedgeShape SmoothWedgedLine(FpPoint a, float width_a, FpPoint b,
                                  float width_b, Color color,
                                  EndingStyle ending_style = ENDING_ROUNDED);

SmoothWedgeShape SmoothThickLine(FpPoint a, FpPoint b, float width, Color color,
                                 EndingStyle ending_style = ENDING_ROUNDED);

class SmoothRoundRectShape : public Rasterizable {
 public:
  Box extents() const override { return extents_; }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override;

  bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                     Color* result) const override;

 private:
  friend SmoothRoundRectShape SmoothFilledCircle(FpPoint center, float radius,
                                                 Color color);

  friend SmoothRoundRectShape SmoothOutlinedCircle(FpPoint center, float radius,
                                                   float outline_thickness,
                                                   Color outline_color,
                                                   Color interior_color);

  friend SmoothRoundRectShape SmoothFilledRoundRect(float x0, float y0,
                                                    float x1, float y1,
                                                    float radius, Color color);

  friend SmoothRoundRectShape SmoothOutlinedRoundRect(
      float x0, float y0, float x1, float y1, float radius,
      float outline_thickness, Color outline_color, Color interior_color);

  SmoothRoundRectShape(float x0, float y0, float x1, float y1, float radius,
                       float interior_radius, Color outline_color,
                       Color interior_color);

  void drawTo(const Surface& s) const override;

  inline Color getColor(int16_t x, int16_t y) const;
  inline Color getColorPreChecked(int16_t x, int16_t y) const;
  inline float calcDistSq(int16_t, int16_t y) const;

  float x0_;
  float y0_;
  float x1_;
  float y1_;
  float r_;
  float ri_;
  Color outline_color_;
  Color interior_color_;
  Box extents_;
  Box inner_wide_;
  Box inner_mid_;
  Box inner_tall_;
};

inline SmoothRoundRectShape SmoothOutlinedRoundRect(
    float x0, float y0, float x1, float y1, float radius,
    float outline_thickness, Color outline_color,
    Color interior_color = color::Transparent) {
  if (x1 < x0) std::swap(x0, x1);
  if (y1 < y0) std::swap(y0, y1);
  float w = x1 - x0;
  float h = y1 - y0;
  float max_radius = ((w < h) ? w : h) / 2;  // 1/2 minor axis.
  if (radius > max_radius) radius = max_radius;
  if (radius < 0) radius = 0;
  if (outline_thickness < 0) outline_thickness = 0;
  float interior_radius = radius - outline_thickness;
  if (interior_radius < 0) interior_radius = 0;
  return SmoothRoundRectShape(x0 + radius, y0 + radius, x1 - radius,
                              y1 - radius, radius, interior_radius,
                              outline_color, interior_color);
}

// Creates a round-rectangle with the specified bounds, corner radius, and
// color.
inline SmoothRoundRectShape SmoothFilledRoundRect(float x0, float y0, float x1,
                                                  float y1, float radius,
                                                  Color color) {
  return SmoothOutlinedRoundRect(x0, y0, x1, y1, radius, 0, color, color);
}

// Creates a circle with the specified center, radius, outline width, outline
// color, and optionally interior color (defaulting to transparent).
inline SmoothRoundRectShape SmoothOutlinedCircle(
    FpPoint center, float radius, float outline_thickness, Color outline_color,
    Color interior_color = color::Transparent) {
  return SmoothOutlinedRoundRect(center.x - radius, center.y - radius,
                                 center.x + radius, center.y + radius, radius,
                                 outline_thickness, outline_color,
                                 interior_color);
}

// Creates a circle with the specified center, radius, and color.
inline SmoothRoundRectShape SmoothFilledCircle(FpPoint center, float radius,
                                               Color color) {
  return SmoothOutlinedCircle(center, radius, 0, color, color);
}

}  // namespace roo_display
