#pragma once

#include "compactor.h"
#include "roo_display/color/blending.h"
#include "roo_display/color/traits.h"
#include "roo_display/core/device.h"
#include "roo_display/internal/byte_order.h"
#include "roo_display/internal/color_format.h"
#include "roo_display/internal/color_io.h"
#include "roo_io/data/byte_order.h"

namespace roo_display {

// Convenience foundational driver for display devices that use the common
// concept of 'address window'. Virtually all SPI devices, including various ILI
// and ST devices, belong in this category. This class implements the entire
// contract of a display device, providing an optimized implementation for pixel
// write, using a Compactor class that detects writes to adjacent pixels and
// minimizes the number of address window commands.
//
// To implement a driver for a particular device on the basis of this class, you
// need to provide an implementation of the 'Target' class, with the following
// template contract:
//
// class Target {
//  public:
//   typedef ColorMode;
//   static constexpr ByteOrder byte_order;
//   Target();
//   Target(Target&&);
//   Target(int16_t width, int16_t height);
//   int16_t width() const;
//   int16_t height() const;
//   void init();
//   void begin();
//   void end();
//   void setOrientation(Orientation);
//   void setAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
//   void sync();
//   void ramWrite(const roo::byte* data, size_t pixel_count);
//   void ramFill(const roo::byte* data, size_t pixel_count);
//};
//
// See ili9341.h for a specific example.
//
// Many display devices rely on the same underying hardware contract, assumimg
// that the display is connected to some kind of shared bus (e.g. SPI), and that
// it has dedicated pins for 'chip select' (CS), 'data / command' (DC), and
// optionally reset (RST). If your device fits this description, you can use
// TransportBus (see transport_bus.h) to implement the Target.
template <typename Target>
class AddrWindowDevice : public DisplayDevice {
 public:
  using ColorMode = typename Target::ColorMode;
  static constexpr ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST;
  static constexpr ByteOrder byte_order = Target::byte_order;
  static constexpr int kBytesPerPixel =
      ColorTraits<typename Target::ColorMode>::bytes_per_pixel;
  using raw_color_type = roo::byte[kBytesPerPixel];

  AddrWindowDevice(Orientation orientation, uint16_t width, uint16_t height)
      : AddrWindowDevice(orientation, Target(width, height)) {}

  AddrWindowDevice(uint16_t width, uint16_t height)
      : AddrWindowDevice(Target(width, height)) {}

  AddrWindowDevice(Target target)
      : AddrWindowDevice(Orientation::Default(), std::move(target)) {}

  template <typename... Args>
  AddrWindowDevice(Args&&... args)
      : AddrWindowDevice(Orientation::Default(),
                         Target(std::forward<Args>(args)...)) {}

  template <typename... Args>
  AddrWindowDevice(Orientation orientation, Args&&... args)
      : AddrWindowDevice(orientation, Target(std::forward<Args>(args)...)) {}

  AddrWindowDevice(Orientation orientation, Target target)
      : DisplayDevice(orientation, target.width(), target.height()),
        target_(std::move(target)),
        initialized_(false),
        bgcolor_(0xFF7F7F7F),
        compactor_() {}

  ~AddrWindowDevice() override {}

  void init() override {
    target_.init();
    initialized_ = true;
    target_.begin();
    target_.setOrientation(orientation());
    target_.end();
  }

  void begin() override { target_.begin(); }

  void end() override { target_.end(); }

  void setBgColorHint(Color bgcolor) override { bgcolor_ = bgcolor; }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override {
    target_.setAddrWindow(x0, y0, x1, y1);
    blending_mode_ = mode;
  }

  void write(Color* color, uint32_t pixel_count) override {
    roo::byte buffer[64 * kBytesPerPixel];
    while (pixel_count > 64) {
      processColorSequence(blending_mode_, color, buffer, 64);
      target_.sync();
      target_.ramWrite(buffer, 64);
      color += 64;
      pixel_count -= 64;
    }
    processColorSequence(blending_mode_, color, buffer, pixel_count);
    target_.sync();
    target_.ramWrite(buffer, pixel_count);
  }

  void fill(Color color, uint32_t pixel_count) override {
    raw_color_type raw_color;
    processColor(blending_mode_, color, raw_color);
    target_.sync();
    target_.ramFill(raw_color, pixel_count);
  }

  void writeRects(BlendingMode blending_mode, Color* color, int16_t* x0,
                  int16_t* y0, int16_t* x1, int16_t* y1,
                  uint16_t count) override {
    raw_color_type raw_color;
    while (count-- > 0) {
      target_.setAddrWindow(*x0, *y0, *x1, *y1);
      uint32_t pixel_count = (*x1 - *x0 + 1) * (*y1 - *y0 + 1);
      x0++;
      y0++;
      x1++;
      y1++;
      Color mycolor = *color++;
      processColor(blending_mode, mycolor, raw_color);
      // No need to call sync after setAddrWindow().
      target_.ramFill(raw_color, pixel_count);
    }
  }

  void fillRects(BlendingMode blending_mode, Color color, int16_t* x0,
                 int16_t* y0, int16_t* x1, int16_t* y1,
                 uint16_t count) override {
    raw_color_type raw_color;
    processColor(blending_mode, color, raw_color);
    while (count-- > 0) {
      target_.setAddrWindow(*x0, *y0, *x1, *y1);
      uint32_t pixel_count = (*x1 - *x0 + 1) * (*y1 - *y0 + 1);
      x0++;
      y0++;
      x1++;
      y1++;
      // No need to call sync after setAddrWindow().
      target_.ramFill(raw_color, pixel_count);
    }
  }

  void writePixels(BlendingMode mode, Color* colors, int16_t* xs, int16_t* ys,
                   uint16_t pixel_count) override {
    blending_mode_ = mode;
    compactor_.drawPixels(
        xs, ys, pixel_count,
        [this, mode, colors](int16_t offset, int16_t x, int16_t y,
                             Compactor::WriteDirection direction,
                             int16_t count) {
          switch (direction) {
            case Compactor::RIGHT: {
              target_.setAddrWindow(x, y, x + count - 1, y);
              break;
            }
            case Compactor::DOWN: {
              target_.setAddrWindow(x, y, x, y + count - 1);
              break;
            }
            case Compactor::LEFT: {
              target_.setAddrWindow(x - count + 1, y, x, y);
              std::reverse(colors + offset, colors + offset + count);
              break;
            }
            case Compactor::UP: {
              target_.setAddrWindow(x, y - count + 1, x, y);
              std::reverse(colors + offset, colors + offset + count);
              break;
            }
          }
          // No need to call sync after setAddrWindow().
          AddrWindowDevice::write(colors + offset, count);
        });
  }

  void fillPixels(BlendingMode blending_mode, Color color, int16_t* xs,
                  int16_t* ys, uint16_t pixel_count) override {
    raw_color_type raw_color;
    processColor(blending_mode, color, raw_color);
    const roo::byte* raw_color_ptr = raw_color;
    compactor_.drawPixels(
        xs, ys, pixel_count,
        [this, raw_color_ptr](int16_t offset, int16_t x, int16_t y,
                              Compactor::WriteDirection direction,
                              int16_t count) {
          switch (direction) {
            case Compactor::RIGHT: {
              target_.setAddrWindow(x, y, x + count - 1, y);
              break;
            }
            case Compactor::DOWN: {
              target_.setAddrWindow(x, y, x, y + count - 1);
              break;
            }
            case Compactor::LEFT: {
              target_.setAddrWindow(x - count + 1, y, x, y);
              break;
            }
            case Compactor::UP: {
              target_.setAddrWindow(x, y - count + 1, x, y);
              break;
            }
          }
          // No need to call sync after setAddrWindow().
          target_.ramFill(raw_color_ptr, count);
        });
  }

  const ColorFormat& getColorFormat() const override {
    static const internal::ColorFormatImpl<typename Target::ColorMode,
                                           Target::byte_order,
                                           COLOR_PIXEL_ORDER_MSB_FIRST>
        format(color_mode());
    return format;
  }

  const ColorMode& color_mode() const {
    static const ColorMode mode;
    return mode;
  }

  void drawDirectRect(const roo::byte* data, size_t row_width_bytes,
                      int16_t src_x0, int16_t src_y0, int16_t src_x1,
                      int16_t src_y1, int16_t dst_x0, int16_t dst_y0) override {
    if (src_x1 < src_x0 || src_y1 < src_y0) return;
    if constexpr (ColorTraits<typename Target::ColorMode>::bytes_per_pixel <
                  1) {
      DisplayOutput::drawDirectRect(data, row_width_bytes, src_x0, src_y0,
                                    src_x1, src_y1, dst_x0, dst_y0);
      return;
    }

    int16_t width = src_x1 - src_x0 + 1;
    int16_t height = src_y1 - src_y0 + 1;
    target_.setAddrWindow(dst_x0, dst_y0, dst_x0 + width - 1,
                          dst_y0 + height - 1);

    const size_t width_bytes = static_cast<size_t>(width) * kBytesPerPixel;
    const roo::byte* row = data +
                           static_cast<size_t>(src_y0) * row_width_bytes +
                           static_cast<size_t>(src_x0) * kBytesPerPixel;
    if (row_width_bytes == width_bytes) {
      // No need to call sync just after setAddrWindow().
      target_.ramWrite(row, static_cast<size_t>(width) * height);
      return;
    }

    for (int16_t y = 0; y < height; ++y) {
      target_.sync();
      target_.ramWrite(row, width);
      row += row_width_bytes;
    }
  }

  void orientationUpdated() override {
    if (!initialized_) {
      // Initialization will set the orientation.
      return;
    }
    target_.setOrientation(orientation());
  }

 protected:
  Target target_;
  bool initialized_;

 private:
  void processColor(BlendingMode blending_mode, Color src, roo::byte* dest)
      __attribute__((always_inline)) {
    src = ApplyBlending(blending_mode, bgcolor_, src);
    ColorIo<typename Target::ColorMode, Target::byte_order> io;
    io.store(src, dest);
  }

  void processColorSequence(BlendingMode blending_mode, Color* src,
                            roo::byte* dest, uint32_t pixel_count)
      __attribute__((always_inline)) {
    ApplyBlendingOverBackground(blending_mode, bgcolor_, src, pixel_count);
    ColorIo<typename Target::ColorMode, Target::byte_order> io;
    while (pixel_count-- > 0) {
      io.store(*src++, dest);
      dest += kBytesPerPixel;
    }
  }

  Color bgcolor_;
  uint16_t last_x0_, last_x1_, last_y0_, last_y1_;
  // Set by setAddress and used by write().
  BlendingMode blending_mode_;
  Compactor compactor_;
};

}  // namespace roo_display
