#pragma once

#include "roo_display/core/rasterizable.h"

namespace roo_display {

class RoundRectShadow : public Rasterizable {
 public:
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

  // Extents are given for the original area to which the shadow is to be
  // applied. Color is the color of the shadow in its darkest place.
  // The `blur_radius` signifies the object's elevation, i.e. how much larger
  // the shadow is compared to the object casting the shadow. A non-zero
  // 'corner_radius' indicates that corners of the object casting shadow are
  // themselves rounded.
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