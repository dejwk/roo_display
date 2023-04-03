#pragma once

#include "roo_display.h"
#include "roo_display/color/color.h"

namespace roo_display {

struct FpPoint {
  float x;
  float y;
};

// Anti-aliased wide line with different width at each end and round endings.
class SmoothWedgeShape : public Drawable {
 public:
  Box extents() const override { return extents_; }

 private:
  friend SmoothWedgeShape SmoothWedgedLine(FpPoint a, float width_a, FpPoint b,
                                           float width_b, Color color);

  friend SmoothWedgeShape SmoothThickLine(FpPoint a, FpPoint b, float width,
                                          Color color);

  SmoothWedgeShape(FpPoint a, float width_a, FpPoint b, float width_b,
                   Color color);

  void drawInteriorTo(const Surface& s) const override;

  float ax_;
  float ay_;
  float bx_;
  float by_;
  float aw_;
  float bw_;
  Color color_;
  Box extents_;
};

inline SmoothWedgeShape SmoothWedgedLine(FpPoint a, float width_a, FpPoint b,
                                         float width_b, Color color) {
  return SmoothWedgeShape(a, width_a, b, width_b, color);
}

inline SmoothWedgeShape SmoothThickLine(FpPoint a, FpPoint b, float width,
                                        Color color) {
  return SmoothWedgeShape(a, width, b, width, color);
}

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

  friend SmoothRoundRectShape SmoothFilledRoundRect(float x0, float y0,
                                                    float x1, float y1,
                                                    float radius, Color color);

  SmoothRoundRectShape(float x0, float y0, float x1, float y1, float radius,
                       Color color);

  void drawTo(const Surface& s) const override;

  inline uint8_t getAlpha(int16_t x, int16_t y, uint8_t max_alpha) const;

  float x0_;
  float y0_;
  float x1_;
  float y1_;
  float r_;
  Color color_;
  Box extents_;
  Box inner_wide_;
  Box inner_mid_;
  Box inner_tall_;

  // For exclusing the 4 triangles in the corners of the bounding rectangle.
  int16_t tmax_;
};

// Creates a circle with the specified center, radius, and color.
inline SmoothRoundRectShape SmoothFilledCircle(FpPoint center, float radius,
                                               Color color) {
  return SmoothRoundRectShape(center.x, center.y, center.x, center.y, radius,
                              color);
}

// Creates a round-rectangle with the specified bounds, corner radius, and color.
inline SmoothRoundRectShape SmoothFilledRoundRect(float x0, float y0, float x1,
                                                  float y1, float radius,
                                                  Color color) {
  if (x1 < x0) std::swap(x0, x1);
  if (y1 < y0) std::swap(y0, y1);
  float w = x1 - x0;
  float h = y1 - y0;
  float max_radius = ((w < h) ? w : h) / 2;  // 1/2 minor axis.
  if (radius > max_radius) radius = max_radius;
  return SmoothRoundRectShape(x0 + radius, y0 + radius, x1 - radius,
                              y1 - radius, radius, color);
}

}  // namespace roo_display
