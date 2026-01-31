#pragma once

#include "compactor.h"
#include "roo_display/color/blending.h"
#include "roo_display/color/traits.h"
#include "roo_display/core/device.h"
#include "roo_display/internal/byte_order.h"
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
//   void ramWrite(raw_color* data, size_t size);
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
    blending_mode_ = mode;
    target_.setAddrWindow(x0, y0, x1, y1);
  }

  void write(Color* color, uint32_t pixel_count) override {
    raw_color_type buffer[64];
    while (pixel_count > 64) {
      color = processColorSequence(blending_mode_, color, buffer, 64);
      target_.ramWrite(buffer, 64);
      pixel_count -= 64;
    }
    processColorSequence(blending_mode_, color, buffer, pixel_count);
    target_.ramWrite(buffer, pixel_count);
  }

  void writeRects(BlendingMode blending_mode, Color* color, int16_t* x0,
                  int16_t* y0, int16_t* x1, int16_t* y1,
                  uint16_t count) override {
    while (count-- > 0) {
      uint32_t pixel_count = (*x1 - *x0 + 1) * (*y1 - *y0 + 1);
      AddrWindowDevice::setAddress(*x0++, *y0++, *x1++, *y1++,
                                   BLENDING_MODE_SOURCE);
      Color mycolor = *color++;
      if (blending_mode == BLENDING_MODE_SOURCE_OVER) {
        mycolor = AlphaBlend(bgcolor_, mycolor);
      } else if (blending_mode == BLENDING_MODE_SOURCE_OVER_OPAQUE) {
        mycolor = AlphaBlendOverOpaque(bgcolor_, mycolor);
      }
      raw_color_type raw_color = to_raw_color(mycolor);
      target_.ramFill(raw_color, pixel_count);
    }
  }

  void fillRects(BlendingMode blending_mode, Color color, int16_t* x0,
                 int16_t* y0, int16_t* x1, int16_t* y1,
                 uint16_t count) override {
    if (blending_mode == BLENDING_MODE_SOURCE_OVER) {
      color = AlphaBlend(bgcolor_, color);
    } else if (blending_mode == BLENDING_MODE_SOURCE_OVER_OPAQUE) {
      color = AlphaBlendOverOpaque(bgcolor_, color);
    }
    raw_color_type raw_color = to_raw_color(color);

    while (count-- > 0) {
      uint32_t pixel_count = (*x1 - *x0 + 1) * (*y1 - *y0 + 1);
      AddrWindowDevice::setAddress(*x0++, *y0++, *x1++, *y1++,
                                   BLENDING_MODE_SOURCE);
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
          AddrWindowDevice::write(colors + offset, count);
        });
  }

  void fillPixels(BlendingMode blending_mode, Color color, int16_t* xs,
                  int16_t* ys, uint16_t pixel_count) override {
    if (blending_mode == BLENDING_MODE_SOURCE_OVER) {
      color = AlphaBlend(bgcolor_, color);
    } else if (blending_mode == BLENDING_MODE_SOURCE_OVER_OPAQUE) {
      color = AlphaBlendOverOpaque(bgcolor_, color);
    }
    raw_color_type raw_color = to_raw_color(color);
    compactor_.drawPixels(
        xs, ys, pixel_count,
        [this, raw_color](int16_t offset, int16_t x, int16_t y,
                          Compactor::WriteDirection direction, int16_t count) {
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
          target_.ramFill(raw_color, count);
        });
  }

  void orientationUpdated() override {
    if (!initialized_) {
      // Initialization will set the orientation.
      return;
    }
    target_.setOrientation(orientation());
  }

  static inline raw_color_type to_raw_color(Color color)
      __attribute__((always_inline)) {
    typename Target::ColorMode mode;
    return roo_io::hto<raw_color_type, Target::byte_order>(
        mode.fromArgbColor(color));
  }

 protected:
  Target target_;
  bool initialized_;

 private:
  Color* processColorSequence(BlendingMode blending_mode, Color* src,
                              raw_color_type* dest, uint32_t pixel_count) {
    ApplyBlendingOverBackground(blending_mode, bgcolor_, src, pixel_count);
    while (pixel_count-- > 0) {
      *dest++ = to_raw_color(*src++);
    }
    return src;
  }

  Color bgcolor_;
  uint16_t last_x0_, last_x1_, last_y0_, last_y1_;
  // Set by setAddress and used by write().
  BlendingMode blending_mode_;
  Compactor compactor_;
};

}  // namespace roo_display
