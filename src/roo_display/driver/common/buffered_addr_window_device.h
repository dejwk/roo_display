#pragma once

#include <memory>

#include "roo_backport.h"
#include "roo_backport/byte.h"
#include "roo_display/core/offscreen.h"
#include "roo_display/driver/common/compactor.h"

namespace roo_display {

template <typename Target>
class BufferedAddrWindowDevice : public DisplayDevice {
 public:
  typedef ColorStorageType<typename Target::ColorMode> raw_color_type;

  BufferedAddrWindowDevice(Orientation orientation = Orientation::Default(),
                           Target target = Target())
      : DisplayDevice(orientation, target.width(), target.height()),
        target_(std::move(target)),
        buffer_(new roo::byte[(Target::ColorMode::bits_per_pixel *
                                   target.width() * target.height() +
                               7) /
                              8]),
        buffer_dev_(target_.width(), target_.height(), buffer_.get(),
                    typename Target::ColorMode()),
        buffer_raster_(buffer_dev_.raster()),
        compactor_() {}

  ~BufferedAddrWindowDevice() override {}

  void init() override { target_.init(); }

  void begin() override { target_.begin(); }

  void end() override {
    flushRectCache();
    target_.end();
  }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    flushRectCache();
    buffer_dev_.setAddress(x0, y0, x1, y1, mode);
    rect_cache_.setWindow(x0, y0, x1, y1);
  }

  void write(Color* color, uint32_t pixel_count) override {
    buffer_dev_.write(color, pixel_count);
    rect_cache_.pixelsWritten(pixel_count);
  }

  void writeRects(BlendingMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    flushRectCache();
    buffer_dev_.writeRects(mode, color, x0, y0, x1, y1, count);
    while (count-- > 0) {
      target_.flushRect(buffer_raster_, *x0++, *y0++, *x1++, *y1++);
    }
  }

  void fillRects(BlendingMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    flushRectCache();
    buffer_dev_.fillRects(mode, color, x0, y0, x1, y1, count);
    while (count-- > 0) {
      target_.flushRect(buffer_raster_, *x0++, *y0++, *x1++, *y1++);
    }
  }

  void writePixels(BlendingMode mode, Color* colors, int16_t* xs, int16_t* ys,
                   uint16_t pixel_count) override {
    compactor_.drawPixels(
        xs, ys, pixel_count,
        [this, mode, colors](int16_t offset, int16_t x, int16_t y,
                             Compactor::WriteDirection direction,
                             int16_t count) {
          switch (direction) {
            case Compactor::RIGHT: {
              buffer_dev_.setAddress(x, y, x + count - 1, y, mode);
              buffer_dev_.write(colors + offset, count);
              target_.flushRect(buffer_raster_, x, y, x + count - 1, y);
              break;
            }
            case Compactor::DOWN: {
              buffer_dev_.setAddress(x, y, x, y + count - 1, mode);
              buffer_dev_.write(colors + offset, count);
              target_.flushRect(buffer_raster_, x, y, x, y + count - 1);
              break;
            }
            case Compactor::LEFT: {
              buffer_dev_.setAddress(x - count + 1, y, x, y, mode);
              std::reverse(colors + offset, colors + offset + count);
              buffer_dev_.write(colors + offset, count);
              target_.flushRect(buffer_raster_, x - count + 1, y, x, y);
              break;
            }
            case Compactor::UP: {
              buffer_dev_.setAddress(x, y - count + 1, x, y, mode);
              std::reverse(colors + offset, colors + offset + count);
              buffer_dev_.write(colors + offset, count);
              target_.flushRect(buffer_raster_, x, y - count + 1, x, y);
              break;
            }
          }
        });
  }

  void fillPixels(BlendingMode mode, Color color, int16_t* xs, int16_t* ys,
                  uint16_t pixel_count) override {
    compactor_.drawPixels(
        xs, ys, pixel_count,
        [this, mode, color](int16_t offset, int16_t x, int16_t y,
                            Compactor::WriteDirection direction,
                            int16_t count) {
          switch (direction) {
            case Compactor::RIGHT: {
              buffer_dev_.fillRect(mode, Box(x, y, x + count - 1, y), color);
              target_.flushRect(buffer_raster_, x, y, x + count - 1, y);
              break;
            }
            case Compactor::DOWN: {
              buffer_dev_.fillRect(mode, Box(x, y, x, y + count - 1), color);
              target_.flushRect(buffer_raster_, x, y, x, y + count - 1);
              break;
            }
            case Compactor::LEFT: {
              buffer_dev_.fillRect(mode, Box(x - count + 1, y, x, y), color);
              target_.flushRect(buffer_raster_, x - count + 1, y, x, y);
              break;
            }
            case Compactor::UP: {
              buffer_dev_.fillRect(mode, Box(x, y - count + 1, x, y), color);
              target_.flushRect(buffer_raster_, x, y - count + 1, x, y);
              break;
            }
          }
        });
  }

  void interpretRect(const roo::byte* data, size_t row_width_bytes, int16_t x0,
                     int16_t y0, int16_t x1, int16_t y1,
                     Color* output) override {
    buffer_dev_.interpretRect(data, row_width_bytes, x0, y0, x1, y1, output);
  }

  void orientationUpdated() override { target_.setOrientation(orientation()); }

  static inline raw_color_type to_raw_color(Color color) {
    typename Target::ColorMode mode;
    return mode.fromArgbColor(color);
  }

 private:
  class RectCache {
   public:
    void setWindow(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
        __attribute__((always_inline)) {
      window_ = Box(x0, y0, x1, y1);
      begin_ = end_ = 0;
    }

    void pixelsWritten(uint16_t count) { end_ += count; }

    bool isDirty() const { return begin_ != end_; }

    uint32_t dirtyPixelCount() const {
      return static_cast<uint32_t>(end_ - begin_);
    }

    // Returns a next rectangle from the dirty segment that needs to be flushed,
    // and adjusts the begin_ offset accordingly.
    // Returns an empty rect when the dirty segment has already been entirely
    // consumed.
    Box consume() {
      if (!isDirty()) return Box(0, 0, -1, -1);
      if (begin_ != 0) {
        // First line starts at offset; we need to return a dedicated horizontal
        // rectangle to represent that.
        uint32_t remaining = static_cast<uint32_t>(window_.width()) - begin_;
        bool unfinished_line = dirtyPixelCount() < remaining;
        if (unfinished_line) {
          // There may come more pixels in this line later; we can't compact the
          // window.
          Box result(window_.xMin() + begin_, window_.yMin(),
                     window_.xMin() + end_ - 1, window_.yMin());
          begin_ = end_;
          return result;
        } else {
          // We can remove this line from the window.
          Box result(window_.xMin() + begin_, window_.yMin(), window_.xMax(),
                     window_.yMin());
          window_ = Box(window_.xMin(), window_.yMin() + 1, window_.xMax(),
                        window_.yMax());
          begin_ = 0;
          end_ -= window_.width();
          return result;
        }
      } else {
        int16_t full_lines = dirtyPixelCount() / window_.width();
        if (full_lines > 0) {
          Box result(window_.xMin(), window_.yMin(), window_.xMax(),
                     window_.yMin() + full_lines - 1);
          // We leave at most one (the last, incomplete) line in the window.
          window_ = Box(window_.xMin(), window_.yMin() + full_lines - 1,
                        window_.xMax(), window_.yMax());
          end_ -= full_lines * window_.width();
          return result;
        } else {
          // Last, incomplete line. The window must be one-line-tall at this
          // point.
          Box result(window_.xMin(), window_.yMin(), window_.xMin() + end_ - 1,
                     window_.yMin());
          window_ = Box(window_.xMin(), window_.yMin() + full_lines,
                        window_.xMax(), window_.yMax());
          begin_ = end_;
          return result;
        }
      }
    }

   private:
    Box window_;
    int32_t begin_;
    int32_t end_;
  };

  void flushRectCache() {
    while (true) {
      Box box = rect_cache_.consume();
      if (box.empty()) return;
      target_.flushRect(buffer_raster_, box.xMin(), box.yMin(), box.xMax(),
                        box.yMax());
    }
  }

  Target target_;
  std::unique_ptr<roo::byte[]> buffer_;
  OffscreenDevice<typename Target::ColorMode> buffer_dev_;
  ConstDramRaster<typename Target::ColorMode> buffer_raster_;
  RectCache rect_cache_;
  Compactor compactor_;
};

}  // namespace roo_display
