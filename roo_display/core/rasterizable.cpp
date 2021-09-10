#include "rasterizable.h"

namespace roo_display {

namespace {

class Stream : public PixelStream {
 public:
  Stream(const Rasterizable* data)
      : data_(data),
        extents_(data->extents()),
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

std::unique_ptr<PixelStream> Rasterizable::CreateStream() const {
  return std::unique_ptr<PixelStream>(new Stream(this));
}

} // namespace roo_display
