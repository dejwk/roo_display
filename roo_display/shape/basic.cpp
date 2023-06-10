
#include "roo_display/shape/basic.h"

namespace roo_display {

template <typename HLineFiller>
void drawNonSteepLine(HLineFiller *drawer, int16_t x0, int16_t y0, int16_t x1,
                      int16_t y1, bool flip_diag) {
  int16_t dx = x1 - x0, dy = y1 - y0;
  int16_t err = dx >> 1, xs = x0, dlen = 0;
  int16_t y = flip_diag ? y1 : y0;
  int16_t ystep = flip_diag ? -1 : 1;

  for (; x0 <= x1; x0++) {
    dlen++;
    err -= dy;
    if (err < 0) {
      err += dx;
      drawer->fillHLine(xs, y, x0);
      dlen = 0;
      y += ystep;
      xs = x0 + 1;
    }
  }
  if (dlen > 0) drawer->fillHLine(xs, y, x1);
}

template <typename VLineFiller>
void drawSteepLine(VLineFiller *drawer, int16_t x0, int16_t y0, int16_t x1,
                   int16_t y1, bool flip_diag) {
  int16_t dy = y1 - y0, dx = x1 - x0;
  int16_t err = dy >> 1, ys = y0, dlen = 0;
  int16_t x = flip_diag ? x1 : x0;
  int16_t xstep = flip_diag ? -1 : 1;

  for (; y0 <= y1; y0++) {
    dlen++;
    err -= dx;
    if (err < 0) {
      err += dy;
      drawer->fillVLine(x, ys, y0);
      dlen = 0;
      x += xstep;
      ys = y0 + 1;
    }
  }
  if (dlen > 0) drawer->fillVLine(x, ys, y1);
}

void drawHLine(DisplayOutput &device, int16_t x0, int16_t y0, int16_t x1,
               Color color, const Box &clip_box, BlendingMode mode) {
  if (x0 > clip_box.xMax() || x1 < clip_box.xMin() || y0 > clip_box.yMax() ||
      y0 < clip_box.yMin() || x1 < x0) {
    return;
  }

  if (x0 < clip_box.xMin()) x0 = clip_box.xMin();
  if (x1 > clip_box.xMax()) x1 = clip_box.xMax();

  device.fillRects(mode, color, &x0, &y0, &x1, &y0, 1);
}

void drawVLine(DisplayOutput &device, int16_t x0, int16_t y0, int16_t y1,
               Color color, const Box &clip_box, BlendingMode mode) {
  if (x0 > clip_box.xMax() || x0 < clip_box.xMin() || y0 > clip_box.yMax() ||
      y1 < clip_box.yMin() || y1 < y0) {
    return;
  }

  if (y0 < clip_box.yMin()) y0 = clip_box.yMin();
  if (y1 > clip_box.yMax()) y1 = clip_box.yMax();

  device.fillRects(mode, color, &x0, &y0, &x0, &y1, 1);
}

void Line::drawTo(const Surface &s) const {
  int16_t x0 = x0_ + s.dx(), y0 = y0_ + s.dy();
  int16_t x1 = x1_ + s.dx(), y1 = y1_ + s.dy();
  Color color = AlphaBlend(s.bgcolor(), this->color());
  if (x0 == x1) {
    drawVLine(s.out(), x0, y0, y1, color, s.clip_box(), s.blending_mode());
  } else if (y0_ == y1_) {
    drawHLine(s.out(), x0, y0, x1, color, s.clip_box(), s.blending_mode());
  } else {
    bool preclipped = s.clip_box().contains(Box(x0, y0, x1, y1));
    if (steep()) {
      if (preclipped) {
        BufferedVLineFiller drawer(s.out(), color, s.blending_mode());
        drawSteepLine(&drawer, x0, y0, x1, y1, diag_);
      } else {
        ClippingBufferedVLineFiller drawer(s.out(), color, s.clip_box(),
                                           s.blending_mode());
        drawSteepLine(&drawer, x0, y0, x1, y1, diag_);
      }
    } else {
      if (preclipped) {
        BufferedHLineFiller drawer(s.out(), color, s.blending_mode());
        drawNonSteepLine(&drawer, x0, y0, x1, y1, diag_);
      } else {
        ClippingBufferedHLineFiller drawer(s.out(), color, s.clip_box(),
                                           s.blending_mode());
        drawNonSteepLine(&drawer, x0, y0, x1, y1, diag_);
      }
    }
  }
}

void Rect::drawTo(const Surface &s) const {
  if (x1_ < x0_ || y1_ < y0_) return;
  int16_t x0 = x0_ + s.dx();
  int16_t y0 = y0_ + s.dy();
  int16_t x1 = x1_ + s.dx();
  int16_t y1 = y1_ + s.dy();
  Color color = AlphaBlend(s.bgcolor(), this->color());
  ClippingBufferedRectFiller filler(s.out(), color, s.clip_box(),
                                    s.blending_mode());
  filler.fillHLine(x0, y0, x1);
  filler.fillHLine(x0, y1, x1);
  filler.fillVLine(x0, y0 + 1, y1 - 1);
  filler.fillVLine(x1, y0 + 1, y1 - 1);
  if (s.fill_mode() == FILL_MODE_RECTANGLE && x1 - x0 >= 2 && y1 - y0 >= 2) {
    s.out().fillRect(BLENDING_MODE_SOURCE, Box(x0 + 1, y0 + 1, x1 - 1, y1 - 1),
                     s.bgcolor());
  }
}

void Border::drawTo(const Surface &s) const {
  if (x1_ < x0_ || y1_ < y0_) return;
  int16_t x0 = x0_ + s.dx();
  int16_t y0 = y0_ + s.dy();
  int16_t x1 = x1_ + s.dx();
  int16_t y1 = y1_ + s.dy();
  Color color = AlphaBlend(s.bgcolor(), this->color());
  ClippingBufferedRectFiller filler(s.out(), color, s.clip_box(),
                                    s.blending_mode());
  if (left_ > 0) {
    filler.fillRect(x0, y0, x0 + left_ - 1, y1);
    x0 += left_;
  }
  if (top_ > 0) {
    filler.fillRect(x0, y0, x1, y0 + top_ - 1);
    y0 += top_;
  }
  if (right_ > 0) {
    filler.fillRect(x1 - right_ + 1, y0, x1, y1);
    x1 -= right_;
  }
  if (bottom_ > 0) {
    filler.fillRect(x0, y1 - bottom_ + 1, x1, y1);
  }
}

void FilledRect::drawTo(const Surface &s) const {
  if (x1_ < x0_ || y1_ < y0_) return;
  int16_t x0 = x0_ + s.dx();
  int16_t y0 = y0_ + s.dy();
  int16_t x1 = x1_ + s.dx();
  int16_t y1 = y1_ + s.dy();
  Color color = AlphaBlend(s.bgcolor(), this->color());
  Box box(x0, y0, x1, y1);
  if (box.clip(s.clip_box())) {
    s.out().fillRect(s.blending_mode(), box, color);
  }
}

class FilledRectStream : public PixelStream {
 public:
  FilledRectStream(Color color) : color_(color) {}

  void Read(Color *buf, uint16_t count) override {
    FillColor(buf, count, color_);
  }

  void Skip(uint32_t count) override {}

 private:
  Color color_;
};

std::unique_ptr<PixelStream> FilledRect::createStream() const {
  return std::unique_ptr<PixelStream>(new FilledRectStream(color()));
}

std::unique_ptr<PixelStream> FilledRect::createStream(const Box &bounds) const {
  return std::unique_ptr<PixelStream>(new FilledRectStream(color()));
}

// Also used to draw regular circles.
template <typename RectFiller>
void drawRoundRectTmpl(RectFiller &filler, int16_t x0, int16_t y0, int x1,
                       int y1, int16_t r) {
  int32_t f = 2 - r;
  int32_t ddF_y = -2 * r;
  int32_t ddF_x = 1;
  int32_t xe = 0;

  while (f < 0) {
    ++xe;
    f += (ddF_x += 2);
  }
  f += (ddF_y += 2);
  filler.fillHLine(x0 - xe, y1 + r, x1 + xe);
  filler.fillHLine(x0 - xe, y0 - r, x1 + xe);
  filler.fillVLine(x1 + r, y0 - xe, y1 + xe);
  filler.fillVLine(x0 - r, y0 - xe, y1 + xe);

  while (xe < --r) {
    int32_t xs = xe + 1;
    while (f < 0) {
      ++xe;
      f += (ddF_x += 2);
    }
    f += (ddF_y += 2);

    filler.fillHLine(x0 - xe, y1 + r, x0 - xs);
    filler.fillHLine(x0 - xe, y0 - r, x0 - xs);
    filler.fillHLine(x1 + xs, y0 - r, x1 + xe);
    filler.fillHLine(x1 + xs, y1 + r, x1 + xe);

    filler.fillVLine(x1 + r, y1 + xs, y1 + xe);
    filler.fillVLine(x1 + r, y0 - xe, y0 - xs);
    filler.fillVLine(x0 - r, y0 - xe, y0 - xs);
    filler.fillVLine(x0 - r, y1 + xs, y1 + xe);
  }
}

template <typename HlineFiller>
void fillRoundRectCorners(HlineFiller *filler, int16_t x0, int16_t y0, int x1,
                          int y1, int16_t r) {
  // Optimized midpoint circle algorithm.
  int16_t x = 0;
  int16_t y = r;
  int16_t dx = 1;
  int16_t dy = r + r;
  int16_t p = -(r >> 1);

  while (x < y) {
    if (p >= 0) {
      dy -= 2;
      p -= dy;
      y--;
    }

    dx += 2;
    p += dx;

    x++;

    filler->fillHLine(x0 - y, y1 + x, x1 + y);
    filler->fillHLine(x0 - y, y0 - x, x1 + y);
    if (p >= 0 && x + 1 < y) {
      filler->fillHLine(x0 - x, y1 + y, x1 + x);
      filler->fillHLine(x0 - x, y0 - y, x1 + x);
    }
  }
}

template <typename HlineFiller>
void fillRoundRectOutsideCorners(HlineFiller *filler, int16_t x0, int16_t y0,
                                 int x1, int y1, int16_t r) {
  // Optimized midpoint circle algorithm.
  int16_t x = 0;
  int16_t y = r;
  int16_t dx = 1;
  int16_t dy = r + r;
  int16_t p = -(r >> 1);

  while (x < y) {
    if (p >= 0) {
      dy -= 2;
      p -= dy;
      y--;
    }

    dx += 2;
    p += dx;

    x++;
    if (y < r) {
      filler->fillHLine(x0 - r, y1 + x, x0 - y - 1);
      filler->fillHLine(x1 + y + 1, y1 + x, x1 + r);
      filler->fillHLine(x0 - r, y0 - x, x0 - y - 1);
      filler->fillHLine(x1 + y + 1, y0 - x, x1 + r);
    }
    if (p >= 0 && x + 1 < y) {
      filler->fillHLine(x0 - r, y1 + y, x0 - x - 1);
      filler->fillHLine(x1 + x + 1, y1 + y, x1 + r);
      filler->fillHLine(x0 - r, y0 - y, x0 - x - 1);
      filler->fillHLine(x1 + x + 1, y0 - y, x1 + r);
    }
  }
}

// Also used to draw circles, in a special case when radius is half of the box
// length.
void drawRoundRect(DisplayOutput &output, const Box &bbox, int16_t radius,
                   const Box &clip_box, Color color, BlendingMode mode) {
  if (Box::Intersect(clip_box, bbox).empty()) return;
  int16_t x0 = bbox.xMin() + radius;
  int16_t y0 = bbox.yMin() + radius;
  int16_t x1 = bbox.xMax() - radius;
  int16_t y1 = bbox.yMax() - radius;
  if (clip_box.contains(bbox)) {
    BufferedRectFiller filler(output, color, mode);
    drawRoundRectTmpl(filler, x0, y0, x1, y1, radius);
  } else {
    ClippingBufferedRectFiller filler(output, color, clip_box, mode);
    drawRoundRectTmpl(filler, x0, y0, x1, y1, radius);
  }
}

void RoundRect::drawInteriorTo(const Surface &s) const {
  Box extents(x0_ + s.dx(), y0_ + s.dy(), x1_ + s.dx(), y1_ + s.dy());
  drawRoundRect(s.out(), extents, radius_, s.clip_box(),
                AlphaBlend(s.bgcolor(), this->color()), s.blending_mode());
}

void Circle::drawInteriorTo(const Surface &s) const {
  Box extents(x0_ + s.dx(), y0_ + s.dy(), x0_ + diameter_ - 1 + s.dx(),
              y0_ + diameter_ - 1 + s.dy());
  drawRoundRect(s.out(), extents, diameter_ >> 1, s.clip_box(),
                AlphaBlend(s.bgcolor(), this->color()), s.blending_mode());
}

// Also used to draw circles, in a special case when radius is half of the box
// length.
void fillRoundRect(DisplayOutput &output, const Box &bbox, int16_t radius,
                   const Box &clip_box, Color color, BlendingMode mode) {
  if (Box::Intersect(clip_box, bbox).empty()) return;
  int16_t x0 = bbox.xMin() + radius;
  int16_t y0 = bbox.yMin() + radius;
  int16_t x1 = bbox.xMax() - radius;
  int16_t y1 = bbox.yMax() - radius;
  if (clip_box.contains(bbox)) {
    BufferedRectFiller filler(output, color, mode);
    if (y0 <= y1) filler.fillRect(bbox.xMin(), y0, bbox.xMax(), y1);
    fillRoundRectCorners(&filler, x0, y0, x1, y1, radius);
  } else {
    ClippingBufferedRectFiller filler(output, color, clip_box, mode);
    if (y0 <= y1) filler.fillRect(bbox.xMin(), y0, bbox.xMax(), y1);
    fillRoundRectCorners(&filler, x0, y0, x1, y1, radius);
  }
}

void fillRoundRectBg(DisplayOutput &output, const Box &bbox, int16_t radius,
                     const Box &clip_box, Color bgcolor, BlendingMode mode) {
  if (Box::Intersect(clip_box, bbox).empty()) return;
  int16_t x0 = bbox.xMin() + radius;
  int16_t y0 = bbox.yMin() + radius;
  int16_t x1 = bbox.xMax() - radius;
  int16_t y1 = bbox.yMax() - radius;
  if (clip_box.contains(bbox)) {
    BufferedRectFiller filler(output, bgcolor, mode);
    fillRoundRectOutsideCorners(&filler, x0, y0, x1, y1, radius);
  } else {
    ClippingBufferedRectFiller filler(output, bgcolor, clip_box, mode);
    fillRoundRectOutsideCorners(&filler, x0, y0, x1, y1, radius);
  }
}

void FilledRoundRect::drawTo(const Surface &s) const {
  Box extents(x0_ + s.dx(), y0_ + s.dy(), x1_ + s.dx(), y1_ + s.dy());
  fillRoundRect(s.out(), extents, radius_, s.clip_box(),
                AlphaBlend(s.bgcolor(), this->color()), s.blending_mode());
  if (s.fill_mode() == FILL_MODE_RECTANGLE) {
    fillRoundRectBg(s.out(), extents, radius_, s.clip_box(), s.bgcolor(),
                    s.blending_mode());
  }
}

void FilledCircle::drawTo(const Surface &s) const {
  Box extents(x0_ + s.dx(), y0_ + s.dy(), x0_ + diameter_ - 1 + s.dx(),
              y0_ + diameter_ - 1 + s.dy());
  fillRoundRect(s.out(), extents, diameter_ >> 1, s.clip_box(),
                AlphaBlend(s.bgcolor(), this->color()), s.blending_mode());
  if (s.fill_mode() == FILL_MODE_RECTANGLE) {
    fillRoundRectBg(s.out(), extents, diameter_ >> 1, s.clip_box(), s.bgcolor(),
                    s.blending_mode());
  }
}

template <typename PixelFiller>
void drawEllipse(PixelFiller *drawer, int16_t x0, int16_t y0, int16_t rx,
                 int16_t ry) {
  int32_t x, y;
  int32_t rx2 = rx * rx;
  int32_t ry2 = ry * ry;
  int32_t fx2 = 4 * rx2;
  int32_t fy2 = 4 * ry2;
  int32_t s;

  for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y;
       x++) {
    // These are ordered to minimise coordinate changes in x or y
    // drawPixel can then send fewer bounding box commands
    drawer->fillPixel(x0 + x, y0 + y);
    drawer->fillPixel(x0 - x, y0 + y);
    drawer->fillPixel(x0 - x, y0 - y);
    drawer->fillPixel(x0 + x, y0 - y);
    if (s >= 0) {
      s += fx2 * (1 - y);
      y--;
    }
    s += ry2 * ((4 * x) + 6);
  }

  for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x;
       y++) {
    // These are ordered to minimise coordinate changes in x or y
    // drawPixel can then send fewer bounding box commands
    drawer->fillPixel(x0 + x, y0 + y);
    drawer->fillPixel(x0 - x, y0 + y);
    drawer->fillPixel(x0 - x, y0 - y);
    drawer->fillPixel(x0 + x, y0 - y);
    if (s >= 0) {
      s += fy2 * (1 - x);
      x--;
    }
    s += rx2 * ((4 * y) + 6);
  }
}

// Coordinates must be sorted by Y order (y2 >= y1 >= y0)
template <typename HLineFiller>
void fillTriangle(HLineFiller *drawer, int16_t x0, int16_t y0, int16_t x1,
                  int16_t y1, int16_t x2, int16_t y2) {
  int16_t a, b, y, last;

  if (y0 == y2) {  // Handle awkward all-on-same-line case as its own thing
    a = b = x0;
    if (x1 < a) {
      a = x1;
    } else if (x1 > b) {
      b = x1;
    }
    if (x2 < a) {
      a = x2;
    } else if (x2 > b) {
      b = x2;
    }
    drawer->fillHLine(a, y0, b);
    return;
  }

  int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0,
          dx12 = x2 - x1, dy12 = y2 - y1;
  int32_t sa = 0, sb = 0;

  // For upper part of triangle, find scanline crossings for segments
  // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
  // is included here (and second loop will be skipped, avoiding a /0
  // error there), otherwise scanline y1 is skipped here and handled
  // in the second loop...which also avoids a /0 error here if y0=y1
  // (flat-topped triangle).
  if (y1 == y2) {
    last = y1;  // Include y1 scanline
  } else {
    last = y1 - 1;  // Skip it
  }

  for (y = y0; y <= last; y++) {
    a = x0 + sa / dy01;
    b = x0 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
    a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if (a > b) std::swap(a, b);
    drawer->fillHLine(a, y, b);
  }

  // For lower part of triangle, find scanline crossings for segments
  // 0-2 and 1-2.  This loop is skipped if y1=y2.
  sa = (int32_t)dx12 * (y - y1);
  sb = (int32_t)dx02 * (y - y0);
  for (; y <= y2; y++) {
    a = x1 + sa / dy12;
    b = x0 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
    a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
    b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
    */
    if (a > b) std::swap(a, b);
    drawer->fillHLine(a, y, b);
  }
}

void Triangle::drawInteriorTo(const Surface &s) const {
  Color color = AlphaBlend(s.bgcolor(), this->color());
  s.drawObject(Line(x0_, y0_, x1_, y1_, color));
  s.drawObject(Line(x1_, y1_, x2_, y2_, color));
  s.drawObject(Line(x0_, y0_, x2_, y2_, color));
}

void FilledTriangle::drawInteriorTo(const Surface &s) const {
  Box box = extents().translate(s.dx(), s.dy());
  Color color = AlphaBlend(s.bgcolor(), this->color());
  int16_t x0 = x0_ + s.dx();
  int16_t y0 = y0_ + s.dy();
  int16_t x1 = x1_ + s.dx();
  int16_t y1 = y1_ + s.dy();
  int16_t x2 = x2_ + s.dx();
  int16_t y2 = y2_ + s.dy();
  if (s.clip_box().contains(box)) {
    BufferedHLineFiller drawer(s.out(), color, s.blending_mode());
    fillTriangle(&drawer, x0, y0, x1, y1, x2, y2);
  } else {
    ClippingBufferedHLineFiller drawer(s.out(), color, s.clip_box(),
                                       s.blending_mode());
    fillTriangle(&drawer, x0, y0, x1, y1, x2, y2);
  }
}

}  // namespace roo_display