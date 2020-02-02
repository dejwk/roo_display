#pragma once

#include "compactor.h"
#include "roo_display/core/byte_order.h"
#include "roo_display/core/device.h"
#include "roo_display/core/memfill.h"

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
//   void setXaddr(uint16_t x0, uint16_t x1);
//   void setYaddr(uint16_t y0, uint16_t y1);
//   void beginRamWrite();
//   void ramWrite(raw_color* data, size_t size);
//   void ramWrite(raw_color data);
//   void ramFill(raw_color data, size_t count);
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
  typedef typename ColorTraits<typename Target::ColorMode>::storage_type
      raw_color_type;

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
        bgcolor_(0xFF7F7F7F),
        last_x0_(-1),
        last_x1_(-1),
        last_y0_(-1),
        last_y1_(-1),
        compactor_() {}

  ~AddrWindowDevice() override {}

  void init() override {
    target_.init();
    target_.begin();
    target_.setOrientation(orientation());
    target_.end();
  }

  void begin() override { target_.begin(); }

  void end() override { target_.end(); }

  void setBgColorHint(Color bgcolor) override { bgcolor_ = bgcolor; }

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) override {
    if (last_x0_ != x0 || last_x1_ != x1) {
      target_.setXaddr(x0, x1);
      last_x0_ = x0;
      last_x1_ = x1;
    }

    if (last_y0_ != y0 || last_y1_ != y1) {
      target_.setYaddr(y0, y1);
      last_y0_ = y0;
      last_y1_ = y1;
    }

    target_.beginRamWrite();
  }

  void write(PaintMode mode, Color* color, uint32_t pixel_count) override {
    raw_color_type buffer[64];
    while (pixel_count > 64) {
      color = processColorSequence(mode, color, buffer, 64);
      target_.ramWrite(buffer, 64);
      pixel_count -= 64;
    }
    processColorSequence(mode, color, buffer, pixel_count);
    target_.ramWrite(buffer, pixel_count);
  }

  void writeRects(PaintMode mode, Color* color, int16_t* x0, int16_t* y0,
                  int16_t* x1, int16_t* y1, uint16_t count) override {
    while (count-- > 0) {
      uint32_t pixel_count = (*x1 - *x0 + 1) * (*y1 - *y0 + 1);
      AddrWindowDevice::setAddress(*x0++, *y0++, *x1++, *y1++);
      Color mycolor = *color++;
      if (mode == PAINT_MODE_BLEND) {
        mycolor = alphaBlend(bgcolor_, mycolor);
      }
      raw_color_type raw_color = to_raw_color(mycolor);
      target_.ramFill(raw_color, pixel_count);
    }
  }

  void fillRects(PaintMode mode, Color color, int16_t* x0, int16_t* y0,
                 int16_t* x1, int16_t* y1, uint16_t count) override {
    if (mode == PAINT_MODE_BLEND) {
      color = alphaBlend(bgcolor_, color);
    }
    raw_color_type raw_color = to_raw_color(color);

    while (count-- > 0) {
      uint32_t pixel_count = (*x1 - *x0 + 1) * (*y1 - *y0 + 1);
      AddrWindowDevice::setAddress(*x0++, *y0++, *x1++, *y1++);
      target_.ramFill(raw_color, pixel_count);
    }
  }

  void writePixels(PaintMode mode, Color* colors, int16_t* xs, int16_t* ys,
                   uint16_t pixel_count) override {
    compactor_.drawPixels(
        xs, ys, pixel_count,
        [this, mode, colors](int16_t offset, int16_t x, int16_t y,
                             Compactor::WriteDirection direction,
                             int16_t count) {
          switch (direction) {
            case Compactor::RIGHT: {
              AddrWindowDevice::setAddress(x, y, x + count - 1, y);
              break;
            }
            case Compactor::DOWN: {
              AddrWindowDevice::setAddress(x, y, x, y + count - 1);
              break;
            }
            case Compactor::LEFT: {
              AddrWindowDevice::setAddress(x - count + 1, y, x, y);
              std::reverse(colors + offset, colors + offset + count);
              break;
            }
            case Compactor::UP: {
              AddrWindowDevice::setAddress(x, y - count + 1, x, y);
              std::reverse(colors + offset, colors + offset + count);
              break;
            }
          }
          AddrWindowDevice::write(mode, colors + offset, count);
        });
  }

  void fillPixels(PaintMode mode, Color color, int16_t* xs, int16_t* ys,
                  uint16_t pixel_count) override {
    if (mode == PAINT_MODE_BLEND) {
      color = alphaBlend(bgcolor_, color);
    }
    raw_color_type raw_color = to_raw_color(color);
    compactor_.drawPixels(
        xs, ys, pixel_count,
        [this, raw_color](int16_t offset, int16_t x, int16_t y,
                          Compactor::WriteDirection direction, int16_t count) {
          switch (direction) {
            case Compactor::RIGHT: {
              AddrWindowDevice::setAddress(x, y, x + count - 1, y);
              break;
            }
            case Compactor::DOWN: {
              AddrWindowDevice::setAddress(x, y, x, y + count - 1);
              break;
            }
            case Compactor::LEFT: {
              AddrWindowDevice::setAddress(x - count + 1, y, x, y);
              break;
            }
            case Compactor::UP: {
              AddrWindowDevice::setAddress(x, y - count + 1, x, y);
              break;
            }
          }
          target_.ramFill(raw_color, count);
        });
  }

  void orientationUpdated() override { target_.setOrientation(orientation()); }

  static inline raw_color_type to_raw_color(Color color) {
    typename Target::ColorMode mode;
    return byte_order::hto<raw_color_type, Target::byte_order>(
        mode.fromArgbColor(color));
  }

 protected:
  Target target_;

 private:
  Color* processColorSequence(PaintMode mode, Color* src, raw_color_type* dest,
                              uint32_t pixel_count) {
    switch (mode) {
      case PAINT_MODE_REPLACE: {
        while (pixel_count-- > 0) {
          *dest++ = to_raw_color(*src++);
        }
        break;
      }
      case PAINT_MODE_BLEND: {
        while (pixel_count-- > 0) {
          *dest++ = to_raw_color(alphaBlend(bgcolor_, *src++));
        }
      }
      default:
        break;
    }
    return src;
  }

  Color bgcolor_;
  uint16_t last_x0_, last_x1_, last_y0_, last_y1_;
  Compactor compactor_;
};

}  // namespace roo_display
