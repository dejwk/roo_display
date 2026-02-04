#pragma once

#include "roo_display/core/rasterizable.h"

namespace roo_display {

/// Rasterizable drop shadow for rounded rectangles.
class RoundRectShadow : public Rasterizable {
 public:
  /// Shadow specification parameters.
  struct Spec {
    // Extents of the shadow.
    int16_t x, y, w, h;

    // External radius of the shadow at full extents.
    uint8_t radius;

    // 'Internal' shadow radius, within which the shadow is not diffused.
    // Generally the same as the corner radius of the widget casting the shadow.
    uint8_t border;

    // The alpha at a point where the shadow is not diffuset.
    uint8_t alpha_start;

    // 256 * how much the alpha decreases per each pixel of distance.
    uint16_t alpha_step;
  };

  /// Construct a rounded-rect shadow.
  ///
  /// `extents` refer to the original object area. `blur_radius` approximates
  /// elevation (shadow growth). `corner_radius` indicates rounded corners of
  /// the casting object.
  RoundRectShadow(roo_display::Box extents, Color color, uint8_t blur_radius,
                  uint8_t dx, uint8_t dy, uint8_t corner_radius);

  Box extents() const override { return shadow_extents_; }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override;

  bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                     roo_display::Color* result) const override;

 private:
  Spec spec_;
  Box object_extents_;
  Box shadow_extents_;
  uint8_t corner_radius_;
  Color color_;
};

}  // namespace roo_display