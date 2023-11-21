#include "rasterizable.h"

namespace roo_display {

namespace {

class Stream : public PixelStream {
 public:
  Stream(const Rasterizable *data, Box bounds)
      : data_(data),
        bounds_(std::move(bounds)),
        x_(bounds_.xMin()),
        y_(bounds_.yMin()) {}

  void Read(Color *buf, uint16_t size) override {
    int16_t x[size];
    int16_t y[size];
    for (int i = 0; i < size; ++i) {
      x[i] = x_;
      y[i] = y_;
      if (x_ < bounds_.xMax()) {
        ++x_;
      } else {
        x_ = bounds_.xMin();
        ++y_;
      }
    }
    if (y[0] == y[size - 1]) {
      if (data_->readColorRect(x[0], y[0], x[size - 1], y[0], buf)) {
        for (int i = 1; i < size; ++i) {
          buf[i] = buf[0];
        }
      }
    } else {
      data_->readColors(x, y, size, buf);
    }
  }

  void Skip(uint32_t count) override {
    auto w = bounds_.width();
    y_ += count / w;
    x_ += count % w;
    if (x_ > bounds_.xMax()) {
      x_ -= w;
      ++y_;
    }
  }

 private:
  const Rasterizable *data_;
  Box bounds_;
  int16_t x_, y_;
};

static const int kMaxBufSize = 32;

}  // namespace

void Rasterizable::readColorsMaybeOutOfBounds(const int16_t *x,
                                              const int16_t *y, uint32_t count,
                                              Color *result,
                                              Color out_of_bounds_color) const {
  Box bounds = extents();
  // First process as much as we can without copying and extra memory.
  uint32_t offset = 0;
  bool fastpath = true;
  while (true) {
    uint32_t buf_size = 64;
    if (buf_size > count) buf_size = count;
    for (uint32_t i = 0; i < buf_size && fastpath; ++i) {
      fastpath &= bounds.contains(x[i], y[i]);
    }
    if (!fastpath) break;
    readColors(x, y, buf_size, result);
    count -= buf_size;
    if (count == 0) return;
    x += buf_size;
    y += buf_size;
    result += buf_size;
  }

  int16_t newx[kMaxBufSize];
  int16_t newy[kMaxBufSize];
  Color newresult[kMaxBufSize];
  uint32_t offsets[kMaxBufSize];
  offset = 0;
  while (offset < count) {
    int buf_size = 0;
    uint32_t start_offset = offset;
    do {
      if (bounds.contains(x[offset], y[offset])) {
        newx[buf_size] = x[offset];
        newy[buf_size] = y[offset];
        offsets[buf_size] = offset;
        ++buf_size;
      }
      offset++;
    } while (offset < count && buf_size < kMaxBufSize);
    readColors(newx, newy, buf_size, newresult);
    int buf_idx = 0;
    for (uint32_t i = start_offset; i < offset; ++i) {
      Color c = out_of_bounds_color;
      if (buf_idx < buf_size && offsets[buf_idx] == i) {
        // Found point in the bounds for which we have the color.
        c = newresult[buf_idx];
        ++buf_idx;
      }
      result[i] = c;
    }
  }
}

bool Rasterizable::readColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                                 int16_t yMax, Color *result) const {
  uint32_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
  int16_t x[pixel_count];
  int16_t y[pixel_count];
  int16_t *cx = x;
  int16_t *cy = y;
  for (int16_t y_cursor = yMin; y_cursor <= yMax; ++y_cursor) {
    for (int16_t x_cursor = xMin; x_cursor <= xMax; ++x_cursor) {
      *cx++ = x_cursor;
      *cy++ = y_cursor;
    }
  }
  readColors(x, y, pixel_count, result);
  Color c = result[0];
  for (uint32_t i = 1; i < pixel_count; i++) {
    if (result[i] != c) return false;
  }
  return true;
}

std::unique_ptr<PixelStream> Rasterizable::createStream() const {
  return std::unique_ptr<PixelStream>(new Stream(this, this->extents()));
}

std::unique_ptr<PixelStream> Rasterizable::createStream(
    const Box &bounds) const {
  return std::unique_ptr<PixelStream>(new Stream(this, bounds));
}

// void Rasterizable::drawTo(const Surface& s) const {
//   Box ext = extents();
//   Box bounds = Box::Intersect(s.clip_box(), ext.translate(s.dx(), s.dy()));
//   if (bounds.empty()) return;
//   Stream stream(this, bounds.translate(-s.dx(), -s.dy()));
//   internal::FillRectFromStream(s.out(), bounds, &stream, s.bgcolor(),
//                                s.fill_mode(), s.blending_mode(),
//                                getTransparencyMode());
// }

namespace {

inline void FillReplaceRect(DisplayOutput &output, const Box &extents,
                            int16_t dx, int16_t dy, const Rasterizable &object,
                            BlendingMode mode) {
  int32_t count = extents.area();
  Color buf[count];
  bool same =
      object.readColorRect(extents.xMin() - dx, extents.yMin() - dy,
                           extents.xMax() - dx, extents.yMax() - dy, buf);
  if (same) {
    output.fillRect(mode, extents, buf[0]);
  } else {
    output.setAddress(extents, mode);
    output.write(buf, count);
  }
}

inline void FillPaintRectOverOpaqueBg(DisplayOutput &output, const Box &extents,
                                      int16_t dx, int16_t dy, Color bgcolor,
                                      const Rasterizable &object,
                                      BlendingMode mode) {
  int32_t count = extents.area();
  Color buf[count];
  bool same =
      object.readColorRect(extents.xMin() - dx, extents.yMin() - dy,
                           extents.xMax() - dx, extents.yMax() - dy, buf);
  if (same) {
    output.fillRect(mode, extents, AlphaBlendOverOpaque(bgcolor, buf[0]));
  } else {
    for (int i = 0; i < count; i++) {
      buf[i] = AlphaBlendOverOpaque(bgcolor, buf[i]);
    }
    output.setAddress(extents, mode);
    output.write(buf, count);
  }
}

inline void FillPaintRectOverBg(DisplayOutput &output, const Box &extents,
                                int16_t dx, int16_t dy, Color bgcolor,
                                const Rasterizable &object, BlendingMode mode) {
  int32_t count = extents.area();
  Color buf[count];
  bool same =
      object.readColorRect(extents.xMin() - dx, extents.yMin() - dy,
                           extents.xMax() - dx, extents.yMax() - dy, buf);
  if (same) {
    output.fillRect(mode, extents, AlphaBlend(bgcolor, buf[0]));
  } else {
    for (int i = 0; i < count; i++) {
      buf[i] = AlphaBlend(bgcolor, buf[i]);
    }
    output.setAddress(extents, mode);
    output.write(buf, count);
  }
}

// Assumes no bgcolor.
inline void WriteRectVisible(DisplayOutput &output, const Box &extents,
                             int16_t dx, int16_t dy, const Rasterizable &object,
                             BlendingMode mode) {
  int32_t count = extents.area();
  Color buf[count];
  bool same =
      object.readColorRect(extents.xMin() - dx, extents.yMin() - dy,
                           extents.xMax() - dx, extents.yMax() - dy, buf);
  if (same) {
    if (buf[0] != color::Transparent) {
      output.fillRect(mode, extents, buf[0]);
    }
  } else {
    BufferedPixelWriter writer(output, mode);
    Color *ptr = buf;
    for (int16_t j = extents.yMin(); j <= extents.yMax(); ++j) {
      for (int16_t i = extents.xMin(); i <= extents.xMax(); ++i) {
        if (*ptr != color::Transparent) writer.writePixel(i, j, *ptr);
        ++ptr;
      }
    }
  }
}

inline void WriteRectVisibleOverOpaqueBg(DisplayOutput &output,
                                         const Box &extents, int16_t dx,
                                         int16_t dy, Color bgcolor,
                                         const Rasterizable &object,
                                         BlendingMode mode) {
  int32_t count = extents.area();
  Color buf[count];
  bool same =
      object.readColorRect(extents.xMin() - dx, extents.yMin() - dy,
                           extents.xMax() - dx, extents.yMax() - dy, buf);
  if (same) {
    if (buf[0] != color::Transparent) {
      output.fillRect(mode, extents, AlphaBlendOverOpaque(bgcolor, buf[0]));
    }
  } else {
    BufferedPixelWriter writer(output, mode);
    Color *ptr = buf;
    for (int16_t j = extents.yMin(); j <= extents.yMax(); ++j) {
      for (int16_t i = extents.xMin(); i <= extents.xMax(); ++i) {
        if (*ptr != color::Transparent)
          writer.writePixel(i, j, AlphaBlendOverOpaque(bgcolor, *ptr));
        ++ptr;
      }
    }
  }
}

inline void WriteRectVisibleOverBg(DisplayOutput &output, const Box &extents,
                                   int16_t dx, int16_t dy, Color bgcolor,
                                   const Rasterizable &object, BlendingMode mode) {
  int32_t count = extents.area();
  Color buf[count];
  bool same =
      object.readColorRect(extents.xMin() - dx, extents.yMin() - dy,
                           extents.xMax() - dx, extents.yMax() - dy, buf);
  if (same) {
    if (buf[0] != color::Transparent) {
      output.fillRect(mode, extents, AlphaBlend(bgcolor, buf[0]));
    }
  } else {
    BufferedPixelWriter writer(output, mode);
    Color *ptr = buf;
    for (int16_t j = extents.yMin(); j <= extents.yMax(); ++j) {
      for (int16_t i = extents.xMin(); i <= extents.xMax(); ++i) {
        if (*ptr != color::Transparent)
          writer.writePixel(i, j, AlphaBlend(bgcolor, *ptr));
        ++ptr;
      }
    }
  }
}

}  // namespace

void Rasterizable::drawTo(const Surface &s) const {
  Box box = s.clip_box();
  int16_t xMin = box.xMin();
  int16_t xMax = box.xMax();
  int16_t yMin = box.yMin();
  int16_t yMax = box.yMax();
  uint32_t pixel_count = box.area();
  const int16_t xMinOuter = (xMin / 8) * 8;
  const int16_t yMinOuter = (yMin / 8) * 8;
  const int16_t xMaxOuter = (xMax / 8) * 8 + 7;
  const int16_t yMaxOuter = (yMax / 8) * 8 + 7;
  FillMode fill_mode = s.fill_mode();
  TransparencyMode transparency = getTransparencyMode();
  Color bgcolor = s.bgcolor();
  DisplayOutput &output = s.out();
  BlendingMode mode = s.blending_mode();
  if (fill_mode == FILL_MODE_RECTANGLE || transparency == TRANSPARENCY_NONE) {
    if (pixel_count <= 64) {
      FillReplaceRect(output, box, s.dx(), s.dy(), *this, mode);
      return;
    }
    if (bgcolor.a() == 0 || transparency == TRANSPARENCY_NONE) {
      for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
        for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
          FillReplaceRect(output,
                          Box(std::max(x, box.xMin()), std::max(y, box.yMin()),
                              std::min((int16_t)(x + 7), box.xMax()),
                              std::min((int16_t)(y + 7), box.yMax())),
                          s.dx(), s.dy(), *this, mode);
        }
      }
    } else if (bgcolor.a() == 0xFF) {
      if (pixel_count <= 64) {
        FillPaintRectOverOpaqueBg(output, box, s.dx(), s.dy(), bgcolor, *this,
                                  mode);
        return;
      }
      for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
        for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
          FillPaintRectOverOpaqueBg(
              output,
              Box(std::max(x, box.xMin()), std::max(y, box.yMin()),
                  std::min((int16_t)(x + 7), box.xMax()),
                  std::min((int16_t)(y + 7), box.yMax())),
              s.dx(), s.dy(), bgcolor, *this, mode);
        }
      }
    } else {
      if (pixel_count <= 64) {
        FillPaintRectOverBg(output, box, s.dx(), s.dy(), bgcolor, *this, mode);
        return;
      }
      for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
        for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
          FillPaintRectOverBg(
              output,
              Box(std::max(x, box.xMin()), std::max(y, box.yMin()),
                  std::min((int16_t)(x + 7), box.xMax()),
                  std::min((int16_t)(y + 7), box.yMax())),
              s.dx(), s.dy(), bgcolor, *this, mode);
        }
      }
    }
  } else {
    if (bgcolor.a() == 0) {
      if (pixel_count <= 64) {
        WriteRectVisible(output, box, s.dx(), s.dy(), *this, mode);
        return;
      }
      for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
        for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
          WriteRectVisible(output,
                           Box(std::max(x, box.xMin()), std::max(y, box.yMin()),
                               std::min((int16_t)(x + 7), box.xMax()),
                               std::min((int16_t)(y + 7), box.yMax())),
                           s.dx(), s.dy(), *this, mode);
        }
      }
    } else if (bgcolor.a() == 0xFF) {
      if (pixel_count <= 64) {
        WriteRectVisibleOverOpaqueBg(output, box, s.dx(), s.dy(), bgcolor,
                                     *this, mode);
        return;
      }
      for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
        for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
          WriteRectVisibleOverOpaqueBg(
              output,
              Box(std::max(x, box.xMin()), std::max(y, box.yMin()),
                  std::min((int16_t)(x + 7), box.xMax()),
                  std::min((int16_t)(y + 7), box.yMax())),
              s.dx(), s.dy(), bgcolor, *this, mode);
        }
      }
    } else {
      if (pixel_count <= 64) {
        WriteRectVisibleOverBg(output, box, s.dx(), s.dy(), bgcolor, *this,
                               mode);
        return;
      }
      for (int16_t y = yMinOuter; y < yMaxOuter; y += 8) {
        for (int16_t x = xMinOuter; x < xMaxOuter; x += 8) {
          WriteRectVisibleOverBg(
              output,
              Box(std::max(x, box.xMin()), std::max(y, box.yMin()),
                  std::min((int16_t)(x + 7), box.xMax()),
                  std::min((int16_t)(y + 7), box.yMax())),
              s.dx(), s.dy(), bgcolor, *this, mode);
        }
      }
    }
  }
}

}  // namespace roo_display
