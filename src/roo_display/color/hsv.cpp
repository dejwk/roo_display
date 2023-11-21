#include "roo_display/color/hsv.h"

#include <stdlib.h>

namespace roo_display {

Color HsvToRgb(float h, float s, float v) {
  // See https://en.wikipedia.org/wiki/HSL_and_HSV.
  float c = v * s;
  float hp = h / 60.0;
  int ihp = (int)hp;
  float x = c * (1 - abs((hp - 2 * (ihp / 2)) - 1));
  float m = v - c;
  int ix = (int)(255 * x);
  int ic = (int)(255 * c);
  int im = (int)(255 * m);
  int ixm = ix + im;
  int icm = ic + im;
  switch (ihp % 6) {
    case 0:
      return Color(icm, ixm, im);
    case 1:
      return Color(ixm, icm, im);
    case 2:
      return Color(im, icm, ixm);
    case 3:
      return Color(im, ixm, icm);
    case 4:
      return Color(ixm, im, icm);
    case 5:
      return Color(icm, im, ixm);
    default:
      return Color(0);
  }
}

}  // namespace roo_display