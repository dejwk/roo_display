#include "roo_display/color/gradient.h"

#include "roo_display/color/interpolation.h"

namespace roo_display {

ColorGradient::ColorGradient(std::vector<Node> gradient, Boundary boundary)
    : gradient_(std::move(gradient)),
      boundary_(boundary),
      transparency_mode_(TransparencyMode::kNone),
      inv_period_(1.0f / (gradient_.back().value - gradient_.front().value)) {
  for (const Node& n : gradient_) {
    uint8_t a = n.color.a();
    if (a != 255) {
      if (a != 0) {
        transparency_mode_ = TransparencyMode::kFull;
        break;
      }
      transparency_mode_ = TransparencyMode::kCrude;
    }
  }
}

Color ColorGradient::getColor(float value) const {
  // Adjust the value, handling the boundary type.
  switch (boundary_) {
    case EXTENDED: {
      break;
    }
    case TRUNCATED: {
      if (value < gradient_.front().value) {
        float diff = gradient_.front().value - value;
        if (diff >= 1) return color::Transparent;
        return gradient_.front().color.withA(255 * diff);
      }
      if (value > gradient_.back().value) {
        float diff = value - gradient_.back().value;
        if (diff >= 1) return color::Transparent;
        return gradient_.back().color.withA(255 * diff);
      }
      break;
    }
    case PERIODIC: {
      if (value < gradient_.front().value || value > gradient_.back().value) {
        float adj = value - gradient_.front().value;
        float period = gradient_.back().value - gradient_.front().value;
        value =
            adj - period * floorf(adj * inv_period_) + gradient_.front().value;
      }
      break;
    }
  }
  // First, find the matching interval.
  uint16_t right_bound = 0;
  while (right_bound < gradient_.size() &&
         gradient_[right_bound].value < value) {
    right_bound++;
  }
  if (right_bound == 0) {
    return gradient_[0].color;
  } else if (right_bound >= gradient_.size()) {
    return gradient_.back().color;
  } else {
    // Interpolate.
    float left_temp = gradient_[right_bound - 1].value;
    float right_temp = gradient_[right_bound].value;
    float f_a = (value - left_temp) / (right_temp - left_temp);
    int16_t if_a = (int16_t)(256 * f_a);
    Color left = gradient_[right_bound - 1].color;
    Color right = gradient_[right_bound].color;
    return InterpolateColors(left, right, if_a);
    // uint32_t a =
    //     ((uint16_t)left.a() * (256 - if_a) + (uint16_t)right.a() * if_a) /
    //     256;
    // int16_t if_c;
    // if (left.a() == right.a()) {
    //   // Common case, e.g. both colors opaque.
    //   if_c = if_a;
    // } else if (left.a() == 0) {
    //   if_c = 256;
    // } else if (right.a() == 0) {
    //   if_c = 0;
    // } else {
    //   float f_c = f_a * right.a() / ((1 - f_a) * left.a() + f_a * right.a());
    //   if_c = (int16_t)(256 * f_c);
    // }
    // uint32_t r =
    //     ((uint16_t)left.r() * (256 - if_c) + (uint16_t)right.r() * if_c) /
    //     256;
    // uint32_t g =
    //     ((uint16_t)left.g() * (256 - if_c) + (uint16_t)right.g() * if_c) /
    //     256;
    // uint32_t b =
    //     ((uint16_t)left.b() * (256 - if_c) + (uint16_t)right.b() * if_c) /
    //     256;
    // return Color(a << 24 | r << 16 | g << 8 | b);
  }
}

RadialGradient::RadialGradient(FpPoint center, ColorGradient gradient,
                               Box extents)
    : cx_(center.x),
      cy_(center.y),
      gradient_(std::move(gradient)),
      extents_(extents) {}

void RadialGradient::readColors(const int16_t* x, const int16_t* y,
                                uint32_t count, Color* result) const {
  while (count-- > 0) {
    float dx = *x - cx_;
    float dy = *y - cy_;
    float r = sqrtf(dx * dx + dy * dy);
    Color c = gradient_.getColor(r);
    *result++ = c;
    ++x;
    ++y;
  }
}

RadialGradientSq::RadialGradientSq(Point center, ColorGradient gradient,
                                   Box extents)
    : cx_(center.x),
      cy_(center.y),
      gradient_(std::move(gradient)),
      extents_(extents) {}

void RadialGradientSq::readColors(const int16_t* x, const int16_t* y,
                                  uint32_t count, Color* result) const {
  while (count-- > 0) {
    int16_t dx = *x - cx_;
    int16_t dy = *y - cy_;
    uint32_t r = dx * dx + dy * dy;
    Color c = gradient_.getColor(r);
    *result++ = c;
    ++x;
    ++y;
  }
}

LinearGradient::LinearGradient(Point origin, float dx, float dy,
                               ColorGradient gradient, Box extents)
    : cx_(origin.x),
      cy_(origin.y),
      dx_(dx),
      dy_(dy),
      gradient_(std::move(gradient)),
      extents_(extents) {}

void LinearGradient::readColors(const int16_t* x, const int16_t* y,
                                uint32_t count, Color* result) const {
  if (dx_ == 0.0f) {
    if (dy_ == 1.0f) {
      while (count-- > 0) {
        *result++ = gradient_.getColor(*y++ - cy_);
      }
    } else {
      while (count-- > 0) {
        *result++ = gradient_.getColor((*y++ - cy_) * dy_);
      }
    }
  } else if (dy_ == 0.0f) {
    if (dx_ == 1.0f) {
      while (count-- > 0) {
        *result++ = gradient_.getColor(*x++ - cx_);
      }
    } else {
      while (count-- > 0) {
        *result++ = gradient_.getColor((*x++ - cx_) * dx_);
      }
    }
  } else {
    while (count-- > 0) {
      *result++ = gradient_.getColor((*x++ - cx_) * dx_ + (*y++ - cy_) * dy_);
    }
  }
}

bool LinearGradient::readColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                                   int16_t yMax, Color* result) const {
  int16_t width = xMax - xMin + 1;
  if (dx_ == 0.0f) {
    if (dy_ == 1.0f) {
      for (int16_t y = yMin; y <= yMax; ++y) {
        FillColor(result, width, gradient_.getColor(y - cy_));
        result += width;
      }
    } else {
      for (int16_t y = yMin; y <= yMax; ++y) {
        FillColor(result, width, gradient_.getColor((y - cy_) * dy_));
        result += width;
      }
    }
  } else if (dy_ == 0.0f) {
    if (dx_ == 1.0f) {
      const Color* start = result;
      for (int16_t x = xMin; x <= xMax; ++x) {
        *result++ = gradient_.getColor(x - cx_);
      }
      for (int16_t y = yMin + 1; y <= yMax; ++y) {
        memcpy(result, start, width * sizeof(Color));
        result += width;
      }
    } else {
      const Color* start = result;
      for (int16_t x = xMin; x <= xMax; ++x) {
        *result++ = gradient_.getColor((x - cx_) * dx_);
      }
      for (int16_t y = yMin + 1; y <= yMax; ++y) {
        memcpy(result, start, width * sizeof(Color));
        result += width;
      }
    }
  } else {
    for (int16_t y = yMin; y <= yMax; ++y) {
      for (int16_t x = xMin; x <= xMax; ++x) {
        *result++ = gradient_.getColor((x - cx_) * dx_ + (y - cy_) * dy_);
      }
    }
  }
  return false;
}

AngularGradient::AngularGradient(FpPoint center, ColorGradient gradient,
                                 Box extents)
    : cx_(center.x),
      cy_(center.y),
      gradient_(std::move(gradient)),
      extents_(extents) {}

void AngularGradient::readColors(const int16_t* x, const int16_t* y,
                                 uint32_t count, Color* result) const {
  while (count-- > 0) {
    *result++ = gradient_.getColor(atan2f(*x++ - cx_, cy_ - *y++));
  }
}

}  // namespace roo_display
