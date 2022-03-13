#include "rasterizable.h"

namespace roo_display {

namespace {

class Stream : public PixelStream {
 public:
  Stream(const Rasterizable* data, Box bounds)
      : data_(data),
        bounds_(std::move(bounds)),
        x_(bounds_.xMin()),
        y_(bounds_.yMin()) {}

  void Read(Color* buf, uint16_t size) override {
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
    data_->ReadColors(x, y, size, buf);
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
  const Rasterizable* data_;
  Box bounds_;
  int16_t x_, y_;
};

static const int kMaxBufSize = 32;

}  // namespace

void Rasterizable::ReadColorsMaybeOutOfBounds(const int16_t* x,
                                              const int16_t* y, uint32_t count,
                                              Color* result) const {
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
    ReadColors(x, y, buf_size, result);
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
    ReadColors(newx, newy, buf_size, newresult);
    int buf_idx = 0;
    for (uint32_t i = start_offset; i < offset; ++i) {
      Color c = color::Transparent;
      if (buf_idx < buf_size && offsets[buf_idx] == i) {
        // Found point in the bounds for which we have the color.
        c = newresult[buf_idx];
        ++buf_idx;
      }
      result[i] = c;
    }
  }
}

bool Rasterizable::ReadColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                                 int16_t yMax, Color* result) const {
  uint32_t pixel_count = (xMax - xMin + 1) * (yMax - yMin + 1);
  int16_t x[pixel_count];
  int16_t y[pixel_count];
  int16_t* cx = x;
  int16_t* cy = y;
  for (int16_t y_cursor = yMin; y_cursor <= yMax; ++y_cursor) {
    for (int16_t x_cursor = xMin; x_cursor <= xMax; ++x_cursor) {
      *cx++ = x_cursor;
      *cy++ = y_cursor;
    }
  }
  ReadColors(x, y, pixel_count, result);
  Color c = result[0];
  for (uint32_t i = 1; i < pixel_count; i++) {
    if (result[i] != c) return false;
  }
  return true;
}

std::unique_ptr<PixelStream> Rasterizable::CreateStream() const {
  return std::unique_ptr<PixelStream>(new Stream(this, this->extents()));
}

std::unique_ptr<PixelStream> Rasterizable::CreateStream(
    const Box& bounds) const {
  return std::unique_ptr<PixelStream>(new Stream(this, bounds));
}

void Rasterizable::drawTo(const Surface& s) const {
  Box ext = extents();
  Box bounds = Box::intersect(s.clip_box(), ext.translate(s.dx(), s.dy()));
  if (bounds.empty()) return;
  Stream stream(this, bounds.translate(-s.dx(), -s.dy()));
  internal::FillRectFromStream(s.out(), bounds, &stream, s.bgcolor(),
                               s.fill_mode(), s.paint_mode(),
                               GetTransparencyMode());
}

}  // namespace roo_display
