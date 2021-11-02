#include "rasterizable.h"

namespace roo_display {

namespace {

class Stream : public PixelStream {
 public:
  Stream(const Rasterizable* data, Box extents)
      : data_(data),
        extents_(std::move(extents)),
        x_(extents_.xMin()),
        y_(extents_.yMin()) {}

  void Read(Color* buf, uint16_t size) override {
    int16_t x[size];
    int16_t y[size];
    for (int i = 0; i < size; ++i) {
      x[i] = x_;
      y[i] = y_;
      if (x_ < extents_.xMax()) {
        ++x_;
      } else {
        x_ = extents_.xMin();
        ++y_;
      }
    }
    data_->ReadColors(x, y, size, buf);
  }

  void Skip(uint32_t count) override {
    auto w = extents_.width();
    y_ += count / w;
    x_ += count % w;
    if (x_ > extents_.xMax()) {
      x_ -= w;
      ++y_;
    }
  }

 private:
  const Rasterizable* data_;
  Box extents_;
  int16_t x_, y_;
};

}  // namespace

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
