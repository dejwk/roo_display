#pragma once

// Support for drawing to in-memory buffers, using various color modes.

#include "roo_display/color/color.h"
#include "roo_display/core/raster.h"
#include "roo_display/internal/byte_order.h"
#include "roo_display/internal/memfill.h"

namespace roo_display {

namespace internal {

// Helper class used by the Offscreen to transform point or rect coordinates
// received in the write and fill requests, in order to apply the offscreen's
// orientation.
// The orientation applies to how content is _written_ to the buffer. Reading
// the buffer is following its 'native' orientation. This allows the buffer
// to be used as a streamable, and have its content drawn to another canvas
// in the standardized order (left-to-right, top-to-bottom), regardless of
// the orientation.
class Orienter {
 public:
  Orienter(int16_t raster_width, int16_t raster_height,
           Orientation orientation = Orientation::Default())
      : orientation_(orientation),
        xMax_(raster_width - 1),
        yMax_(raster_height - 1) {}

  Orientation orientation() const { return orientation_; }

  void setOrientation(Orientation orientation) { orientation_ = orientation; }

  void OrientPixels(int16_t *&x, int16_t *&y, int16_t count);
  void OrientRects(int16_t *&x0, int16_t *&y0, int16_t *&x1, int16_t *&y1,
                   int16_t count);

 private:
  inline void revertX(int16_t *x, uint16_t count);
  inline void revertY(int16_t *y, uint16_t count);

  Orientation orientation_;
  int16_t xMax_;
  int16_t yMax_;
};

// Helper class used by Offscreen to implement setAddress() and then
// write()/fill() under different orientations. Internally, the class
// stores two (signed) integer offsets, specifying the amount of pixel
// index increment (or decrement, if negative) along the x and y axis,
// respectively. This representation yields a very efficient and simple
// implementation of advance(), regardless of the orientation.
class AddressWindow {
 public:
  AddressWindow();

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  int16_t raw_width, int16_t raw_height,
                  Orientation orientation);

  int16_t x() const { return cursor_x_; }
  int16_t y() const { return cursor_y_; }

  const uint32_t offset() const { return offset_; }
  Orientation orientation() const { return orientation_; }
  int16_t advance_x() const { return advance_x_; }
  int16_t advance_y() const { return advance_y_; }

  void advance();
  void advance(uint32_t count);

  bool eof() const { return cursor_y_ > y1_; }
  uint16_t remaining_in_row() const { return x1_ - cursor_x_ + 1; }
  uint16_t remaining_rows() const { return y1_ - cursor_y_ + 1; }
  int16_t width() const { return x1_ - x0_ + 1; }
  int16_t height() const { return y1_ - y0_ + 1; }

 private:
  Orientation orientation_;
  uint32_t offset_;
  uint16_t x0_, x1_, y0_, y1_;
  int16_t advance_x_, advance_y_;
  uint16_t cursor_x_, cursor_y_;
};

}  // namespace internal

// It implements both Drawable and Streamable contracts,
// which allows to be easily drawn to other display devices.
// For example, to draw a sub-rectangle of the Offscreen, simply use
// CreateClippedStream(offscreen, rect).

// If you draw to/from the offscreen using its public methods only, the choice
// of these two parameters does not really matter; use defaults. But, if you're
// importing or exporting raw data from/to some existing format or device, you
// may need to set these parameters to match that format. For example, XBitmap
// uses LSB_FIRST.
//

// Bounding rectangle, specified during construction, determines how the
// Offscreen behaves when it is drawn to another display device. As any other
// Drawable, Offscreen can have a bounding rectangle that does not start at
// (0,0).

//
// Note: to use the content of the offscreen as Stremable, e.g. to super-impose
// the content over / under another streamable, use raster().

// The OffscreenDevice class is an in-memory implementation of DisplayDevice. It
// can be used just like a regular display device, to buffer arbitrary images
// and display operations.
//
// OffscreenDevice supports arbitrary color modes, byte orders, and pixel
// orders, which can be specified as template parameters:
// * pixel_order applies to sub-byte color modes, and specifies if, within
//   a single byte, the left-most pixel is mapped to the most or the least
//   significant bit(s).
// * byte_order applies to multi-byte color modes, and specifies if the bytes
//   representing a single pixel are stored in big or little endian.
//
// Offscreen supports device orientation. When created, the Offscreen has the
// default orientation (Right-Down). You can modify the orientation at any time,
// using setOrientation(). New orientation will not affect existing content, but
// it will affect all future write operations. For example, setting the
// orientation to 'Orientation::Default().rotateLeft()' allows to render content
// rotated counter-clockwise by 90 degrees.
template <typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN,
          int8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
class OffscreenDevice : public DisplayDevice {
 public:
  // Creates an offscreen device with specified geometry, using the designated
  // buffer. The buffer must have sufficient capacity, determined as (width *
  // height * ColorMode::bits_per_pixel + 7) / 8. The buffer is not modified; it
  // can contain pre-existing content.
  OffscreenDevice(int16_t width, int16_t height, uint8_t *buffer,
                  ColorMode color_mode)
      : DisplayDevice(width, height),
        color_mode_(color_mode),
        buffer_(buffer),
        orienter_(width, height, Orientation::Default()) {}

  OffscreenDevice(OffscreenDevice &&other) = delete;

  void orientationUpdated();

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                  BlendingMode mode) override;

  void write(Color *color, uint32_t pixel_count) override;

  void writePixels(BlendingMode mode, Color *color, int16_t *x, int16_t *y,
                   uint16_t pixel_count) override;

  void fillPixels(BlendingMode mode, Color color, int16_t *x, int16_t *y,
                  uint16_t pixel_count) override;

  void writeRects(BlendingMode mode, Color *color, int16_t *x0, int16_t *y0,
                  int16_t *x1, int16_t *y1, uint16_t count) override;

  void fillRects(BlendingMode mode, Color color, int16_t *x0, int16_t *y0,
                 int16_t *x1, int16_t *y1, uint16_t count) override;

  ColorMode &color_mode() { return color_mode_; }
  const ColorMode &color_mode() const { return color_mode_; }

  // const Raster<const uint8_t *, ColorMode, pixel_order, byte_order> &raster()
  //     const {
  //   return raster_;
  // }

  const Raster<const uint8_t *, ColorMode, pixel_order, byte_order> raster()
      const {
    return raster(0, 0);
  }

  const Raster<const uint8_t *, ColorMode, pixel_order, byte_order> raster(
      int16_t dx, int16_t dy) const {
    return Raster<const uint8_t *, ColorMode, pixel_order, byte_order>(
        Box(dx, dy, dx + raw_width() - 1, dy + raw_height() - 1), buffer_,
        color_mode_);
  }

  // std::unique_ptr<PixelStream> createStream() const override {
  //   return raster().createStream();
  // }

  // TransparencyMode getTransparencyMode() const override {
  //   return color_mode().transparency();
  // }

  // virtual void readColors(const int16_t *x, const int16_t *y, uint32_t count,
  //                         Color *result) const override {
  //   raster().readColors(x, y, count, result);
  // }

  // Allows direct access to the underlying buffer.
  uint8_t *buffer() { return buffer_; }
  const uint8_t *buffer() const { return buffer_; }

  int16_t window_x() const { return window_.x(); }
  int16_t window_y() const { return window_.y(); }

 private:
  template <typename, typename, ColorPixelOrder, ByteOrder>
  friend struct WriteOp;

  template <typename Writer>
  void writeToWindow(Writer &write, uint32_t count) {
    while (count-- > 0) {
      write(buffer_, window_.offset());
      window_.advance();
    }
  }

  void fillRectsAbsolute(BlendingMode mode, Color color, int16_t *x0,
                         int16_t *y0, int16_t *x1, int16_t *y1, uint16_t count);

  void fillHlinesAbsolute(BlendingMode mode, Color color, int16_t *x0,
                          int16_t *y0, int16_t *x1, uint16_t count);

  void fillVlinesAbsolute(BlendingMode mode, Color color, int16_t *x0,
                          int16_t *y0, int16_t *y1, uint16_t count);

  ColorMode color_mode_;

  uint8_t *buffer_;

  // bool owns_buffer_;
  internal::Orienter orienter_;
  // // Streaming read acess.
  // Raster<const uint8_t *, ColorMode, pixel_order, byte_order> raster_;
  internal::AddressWindow window_;
  BlendingMode blending_mode_;
};

// Offscreen<Rgb565> offscreen;
// DrawingContext dc(offscreen);
// dc.draw(...);
template <typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN,
          int8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
class Offscreen : public Rasterizable {
 public:
  // Creates an offscreen with specified geometry, using the designated buffer.
  // The buffer must have sufficient capacity, determined as (width * height *
  // ColorMode::bits_per_pixel + 7) / 8. The buffer is not modified; it can
  // contain pre-existing content.
  Offscreen(int16_t width, int16_t height, uint8_t *buffer,
            ColorMode color_mode = ColorMode())
      : Offscreen(Box(0, 0, width - 1, height - 1), buffer, color_mode) {}

  // Creates an offscreen device with specified geometry, using the
  // designated buffer. The buffer must have sufficient capacity, determined as
  // (width * height * ColorMode::bits_per_pixel + 7) / 8. The buffer is not
  // modified; it can contain pre-existing content.
  Offscreen(Box extents, uint8_t *buffer, ColorMode color_mode = ColorMode())
      : device_(extents.width(), extents.height(), buffer, color_mode),
        raster_(device_.raster(extents.xMin(), extents.yMin())),
        extents_(extents),
        anchor_extents_(extents),
        owns_buffer_(false) {}

  // Creates an offscreen with specified geometry, using an internally
  // allocated buffer. The buffer is not pre-initialized; it contains random
  // bytes.
  Offscreen(int16_t width, int16_t height, ColorMode color_mode = ColorMode())
      : Offscreen(Box(0, 0, width - 1, height - 1), color_mode) {}

  // Creates an offscreen with specified geometry, using an internally allocated
  // buffer. The buffer is not pre-initialized; it contains random bytes.
  Offscreen(Box extents, ColorMode color_mode = ColorMode())
      : device_(
            extents.width(), extents.height(),
            new uint8_t[(ColorMode::bits_per_pixel * extents.area() + 7) / 8],
            color_mode),
        raster_(device_.raster(extents.xMin(), extents.yMin())),
        extents_(extents),
        anchor_extents_(extents),
        owns_buffer_(true) {}

  // Creates an offscreen with specified geometry, using an internally allocated
  // buffer. The buffer is pre-filled using the specified  color.
  Offscreen(int16_t width, int16_t height, Color fillColor,
            ColorMode color_mode = ColorMode())
      : Offscreen(Box(0, 0, width - 1, height - 1), fillColor, color_mode) {}

  // Creates an offscreen with specified geometry, using an internally allocated
  // buffer. The buffer is pre-filled using the specified  color.
  Offscreen(Box extents, Color fillColor, ColorMode color_mode = ColorMode())
      : Offscreen(extents, color_mode) {
    device_.fillRect(0, 0, extents.width() - 1, extents.height() - 1,
                     fillColor);
  }

  // Convenience constructor that makes a RAM copy of the specified drawable.
  Offscreen(const Drawable &d, ColorMode color_mode = ColorMode())
      : Offscreen(d.extents(), color_mode) {
    setAnchorExtents(d.anchorExtents());
    Box extents = d.extents();
    if (color_mode.transparency() != TRANSPARENCY_NONE) {
      device_.fillRect(0, 0, extents.width() - 1, extents.height() - 1,
                       color::Transparent);
    }
    Surface s(device_, -extents.xMin(), -extents.yMin(),
              Box(0, 0, extents.width(), extents.height()), false);
    s.drawObject(d);
  }

  virtual ~Offscreen() {
    if (owns_buffer_) delete[] output().buffer();
  }

  const Raster<const uint8_t *, ColorMode, pixel_order, byte_order> &raster()
      const {
    return raster_;
  }

  Box extents() const override { return extents_; }
  Box anchorExtents() const override { return anchor_extents_; }

  void setAnchorExtents(Box anchor_extents) {
    anchor_extents_ = anchor_extents;
  }

  TransparencyMode getTransparencyMode() const override {
    return raster().getTransparencyMode();
  }

  void readColors(const int16_t *x, const int16_t *y, uint32_t count,
                  Color *result) const override {
    return raster().readColors(x, y, count, result);
  }

  const OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                        storage_type> &
  output() const {
    return device_;
  }

  OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                  storage_type> &
  output() {
    return device_;
  }

  uint8_t *buffer() { return output().buffer(); }
  const uint8_t *buffer() const { return output().buffer(); }

 protected:
  // Sets the default (maximum) clip box. Usually the same as raster extents,
  // but may be smaller, e.g. if the underlying raster is byte-aligned and the
  // extents aren't.
  void set_extents(const Box &extents) { extents_ = extents; }

 private:
  friend class DrawingContext;

  // Implements Drawable.
  void drawTo(const Surface &s) const override { s.drawObject(raster()); }

  // For DrawingContext.
  void nest() const {}
  void unnest() const {}
  Color getBackgroundColor() const { return color::Transparent; }
  const Rasterizable *getRasterizableBackground() const { return nullptr; }
  int16_t dx() const { return -raster_.extents().xMin(); }
  int16_t dy() const { return -raster_.extents().yMin(); }
  bool is_write_once() const { return false; }

  OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                  storage_type>
      device_;

  const Raster<const uint8_t *, ColorMode, pixel_order, byte_order> raster_;

  Box extents_;
  Box anchor_extents_;

  bool owns_buffer_;
};

// Convenience specialization for constructing bit maps, e.g. to set them
// as bit masks. Uses Monochrome with transparent background. Writing anything
// but full transparency causes the bit to be set. The data is aligned row-wise
// to full bytes; that is, each row of pixels starts on a new byte.
class BitMaskOffscreen : public Offscreen<Monochrome> {
 public:
  // Creates a bit map offscreen with the specified geometry, using the
  // designated buffer. The buffer must have sufficient capacity, determined as
  // ((width + 7) / 8) * height. The buffer is not modified; it can contain
  // pre-existing content.
  BitMaskOffscreen(int16_t width, int16_t height, uint8_t *buffer)
      : BitMaskOffscreen(Box(0, 0, width - 1, height - 1), buffer) {}

  // Creates a bit map offscreen with the specified geometry, using the
  // designated buffer. The buffer must have sufficient capacity, determined as
  // ((width + 7) / 8) * height. The buffer is not modified; it can contain
  // pre-existing content.
  BitMaskOffscreen(Box extents, uint8_t *buffer);

  // Creates an offscreen with specified geometry, using an internally
  // allocated buffer. The buffer is not pre-initialized; it contains random
  // bytes.
  BitMaskOffscreen(int16_t width, int16_t height)
      : BitMaskOffscreen(Box(0, 0, width - 1, height - 1)) {}

  // Creates an offscreen with specified geometry, using an internally allocated
  // buffer. The buffer is not pre-initialized; it contains random bytes.
  BitMaskOffscreen(Box extents);

  // Creates an offscreen with specified geometry, using an internally allocated
  // buffer. The buffer is pre-filled using the specified  color.
  BitMaskOffscreen(int16_t width, int16_t height, Color fillColor)
      : BitMaskOffscreen(Box(0, 0, width - 1, height - 1), fillColor) {}

  // Creates an offscreen with specified geometry, using an internally allocated
  // buffer. The buffer is pre-filled using the specified  color.
  BitMaskOffscreen(Box extents, Color fillColor);
};

// Implementation details follow.

// Writer template contract specifies how to write pixel, or a sequence of
// pixels, using a specified using a specified sequence of colors, into a buffer
// with a given 'start' address, and a given pixel_offset. The writer does not
// need to know the geometry (i.e. width and height) of the display; it treats
// the buffer as a contiguous sequence of pixels, starting at the top-left
// corner, and going left-to-right, and then top-to-bottom.

namespace internal {

// Reader / writer for multi-byte color modes, supports reading and writing raw
// byte content from and to DRAM. Used by Writers and Fillers, below.
template <int bits_per_pixel, ByteOrder byte_order>
class RawIterator;

template <typename RawType, ByteOrder byte_order>
class TrivialIterator {
 public:
  TrivialIterator(uint8_t *ptr, uint32_t offset)
      : ptr_(((RawType *)ptr) + offset) {}
  RawType read() const { return byte_order::toh<RawType, byte_order>(*ptr_); }
  void write(RawType value) const {
    *ptr_ = byte_order::hto<RawType, byte_order>(value);
  }
  void operator++() { ptr_++; }

 private:
  RawType *ptr_;
};

template <ByteOrder byte_order>
class RawIterator<8, byte_order> : public TrivialIterator<uint8_t, byte_order> {
 public:
  RawIterator(uint8_t *ptr, uint32_t offset)
      : TrivialIterator<uint8_t, byte_order>(ptr, offset) {}
};

template <ByteOrder byte_order>
class RawIterator<16, byte_order>
    : public TrivialIterator<uint16_t, byte_order> {
 public:
  RawIterator(uint8_t *ptr, uint32_t offset)
      : TrivialIterator<uint16_t, byte_order>(ptr, offset) {}
};

template <ByteOrder byte_order>
class RawIterator<32, byte_order>
    : public TrivialIterator<uint32_t, byte_order> {
 public:
  RawIterator(uint8_t *ptr, uint32_t offset)
      : TrivialIterator<uint32_t, byte_order>(ptr, offset) {}
};

template <>
class RawIterator<24, BYTE_ORDER_BIG_ENDIAN> {
 public:
  RawIterator(uint8_t *ptr, uint32_t offset) : ptr_(ptr + 3 * offset) {}
  uint32_t read() const { return ptr_[0] << 16 | ptr_[1] << 8 | ptr_[2]; }
  void write(uint32_t value) const {
    ptr_[0] = value >> 16;
    ptr_[1] = value >> 8;
    ptr_[2] = value;
  }
  void operator++() { ptr_ += 3; }

 private:
  uint8_t *ptr_;
};

template <>
class RawIterator<24, BYTE_ORDER_LITTLE_ENDIAN> {
 public:
  RawIterator(uint8_t *ptr, uint32_t offset) : ptr_(ptr + 3 * offset) {}
  uint32_t read() const { return ptr_[2] << 16 | ptr_[1] << 8 | ptr_[0]; }
  void write(uint32_t value) const {
    ptr_[0] = value;
    ptr_[1] = value >> 8;
    ptr_[2] = value >> 16;
  }
  void operator++() { ptr_ += 3; }

 private:
  uint8_t *ptr_;
};

// For sub-byte color modes.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          BlendingMode blending_mode,
          uint8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
class BlendingWriterOperator {
 public:
  BlendingWriterOperator(ColorMode &color_mode, const Color *color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    RawBlender<ColorMode, blending_mode> blender;
    auto color = blender(subpixel.ReadSubPixelColor(*target, pixel_index),
                         *color_++, color_mode_);
    subpixel.applySubPixelColor(color, target, pixel_index);
  }

  void operator()(storage_type *p, uint32_t offset, uint32_t count) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    while (count-- > 0) {
      RawBlender<ColorMode, blending_mode> blender;
      auto color = blender(subpixel.ReadSubPixelColor(*target, pixel_index),
                           *color_++, color_mode_);
      subpixel.applySubPixelColor(color, target, pixel_index);
      if (++pixel_index == pixels_per_byte) {
        pixel_index = 0;
        target++;
      }
    }
  }

 private:
  ColorMode &color_mode_;
  const Color *color_;
};

// For color modes in which a pixel takes up at least 1 byte.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          BlendingMode blending_mode, typename storage_type>
class BlendingWriterOperator<ColorMode, pixel_order, byte_order, blending_mode,
                             1, storage_type> {
 public:
  BlendingWriterOperator(const ColorMode &color_mode, const Color *color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    RawBlender<ColorMode, blending_mode> blender;
    itr.write(blender(itr.read(), *color_++, color_mode_));
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    RawBlender<ColorMode, blending_mode> blender;
    while (count-- > 0) {
      itr.write(blender(itr.read(), *color_++, color_mode_));
      ++itr;
    }
  }

 private:
  const ColorMode &color_mode_;
  const Color *color_;
};

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order>
struct BlendingWriter {
  template <BlendingMode blending_mode>
  using Operator =
      BlendingWriterOperator<ColorMode, pixel_order, byte_order, blending_mode>;
};

// BLENDING_MODE_SOURCE specialization of the writer.

// // For sub-byte color modes.
// template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder
// byte_order,
//           uint8_t pixels_per_byte, typename storage_type>
// class BlendingWriter<ColorMode, pixel_order, byte_order,
// BLENDING_MODE_SOURCE,
//                     pixels_per_byte, storage_type> {
//  public:
//   BlendingWriter(ColorMode &color_mode, const Color *color)
//       : color_mode_(color_mode), color_(color) {}

//   void operator()(uint8_t *p, uint32_t offset) {
//     SubPixelColorHelper<ColorMode, pixel_order> subpixel;
//     int pixel_index = offset % pixels_per_byte;
//     uint8_t *target = p + offset / pixels_per_byte;
//     subpixel.applySubPixelColor(color_mode_.fromArgbColor(*color_++), target,
//                                 pixel_index);
//   }

//   void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
//     SubPixelColorHelper<ColorMode, pixel_order> subpixel;
//     int pixel_index = offset % pixels_per_byte;
//     uint8_t *target = p + offset / pixels_per_byte;
//     while (count-- > 0) {
//       subpixel.applySubPixelColor(color_mode_.fromArgbColor(*color_++),
//       target,
//                                   pixel_index);
//       if (++pixel_index == pixels_per_byte) {
//         pixel_index = 0;
//         target++;
//       }
//     }
//   }

//  private:
//   ColorMode &color_mode_;
//   const Color *color_;
// };

// // For color modes in which a pixel takes up at least 1 byte.
// template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder
// byte_order,
//           typename storage_type>
// class BlendingWriter<ColorMode, pixel_order, byte_order,
// BLENDING_MODE_SOURCE, 1,
//                     storage_type> {
//  public:
//   BlendingWriter(const ColorMode &color_mode, const Color *color)
//       : color_mode_(color_mode), color_(color) {}

//   void operator()(uint8_t *p, uint32_t offset) {
//     internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p,
//     offset); itr.write(color_mode_.fromArgbColor(*color_++));
//   }

//   void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
//     internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p,
//     offset); while (count-- > 0) {
//       itr.write(color_mode_.fromArgbColor(*color_++));
//       ++itr;
//     }
//   }

//  private:
//   const ColorMode &color_mode_;
//   const Color *color_;
// };

// GenericWriter is similar to BlendingWriter, but it resolves the blender at
// run time. It might therefore be a bit slower, but it does not waste program
// space.

// For sub-byte color modes.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          uint8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
class GenericWriter {
 public:
  GenericWriter(ColorMode &color_mode, BlendingMode blending_mode, Color *color)
      : color_mode_(color_mode), color_(color), blending_mode_(blending_mode) {}

  void operator()(uint8_t *p, uint32_t offset) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    auto color = ApplyRawBlending(
        blending_mode_, subpixel.ReadSubPixelColor(*target, pixel_index),
        *color_++, color_mode_);
    subpixel.applySubPixelColor(color, target, pixel_index);
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    // TODO: this loop can be optimized to work on an array of color at a time.
    while (count-- > 0) {
      auto color = ApplyRawBlending(
          blending_mode_, subpixel.ReadSubPixelColor(*target, pixel_index),
          *color_++, color_mode_);
      subpixel.applySubPixelColor(color, target, pixel_index);
      if (++pixel_index == pixels_per_byte) {
        pixel_index = 0;
        target++;
      }
    }
  }

 private:
  ColorMode &color_mode_;
  Color *color_;
  BlendingMode blending_mode_;
};

// For color modes in which a pixel takes up at least 1 byte.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          typename storage_type>
class GenericWriter<ColorMode, pixel_order, byte_order, 1, storage_type> {
 public:
  GenericWriter(const ColorMode &color_mode, BlendingMode blending_mode,
                Color *color)
      : color_mode_(color_mode), color_(color), blending_mode_(blending_mode) {}

  void operator()(uint8_t *p, uint32_t offset) {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    itr.write(
        ApplyRawBlending(blending_mode_, itr.read(), *color_++, color_mode_));
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    while (count-- > 0) {
      itr.write(ApplyRawBlending(blending_mode_, itr.read(), *color_++));
      ++itr;
    }
  }

 private:
  const ColorMode &color_mode_;
  Color *color_;
  BlendingMode blending_mode_;
};

// Filler template constract is similar to the writer template contract, except
// that filler uses a single color.

// BlendingFiller is a Filler that will blend the specified color over
// previous content, templated on the specified blender.

// For sub-byte color modes.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          BlendingMode blending_mode,
          uint8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
class BlendingFillerOperator {
 public:
  BlendingFillerOperator(ColorMode &color_mode, Color color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    RawBlender<ColorMode, blending_mode> blender;
    auto color = blender(subpixel.ReadSubPixelColor(*target, pixel_index),
                         color_, color_mode_);
    subpixel.applySubPixelColor(color, target, pixel_index);
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    RawBlender<ColorMode, blending_mode> blender;
    while (count-- > 0) {
      auto color = blender(subpixel.ReadSubPixelColor(*target, pixel_index),
                           color_, color_mode_);
      subpixel.applySubPixelColor(color, target, pixel_index);
      if (++pixel_index == pixels_per_byte) {
        pixel_index = 0;
        target++;
      }
    }
  }

 private:
  ColorMode &color_mode_;
  Color color_;
};

// For color modes in which a pixel takes up at least 1 byte.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          BlendingMode blending_mode, typename storage_type>
class BlendingFillerOperator<ColorMode, pixel_order, byte_order, blending_mode,
                             1, storage_type> {
 public:
  BlendingFillerOperator(const ColorMode &color_mode, Color color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) const {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    RawBlender<ColorMode, blending_mode> blender;
    itr.write(blender(itr.read(), color_, color_mode_));
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) const {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    RawBlender<ColorMode, blending_mode> blender;
    while (count-- > 0) {
      itr.write(blender(itr.read(), color_, color_mode_));
      ++itr;
    }
  }

 private:
  const ColorMode &color_mode_;
  Color color_;
};

// Optimized specialization for BLENDING_MODE_SOURCE.

// For sub-byte color modes.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          uint8_t pixels_per_byte, typename storage_type>
class BlendingFillerOperator<ColorMode, pixel_order, byte_order,
                             BLENDING_MODE_SOURCE, pixels_per_byte,
                             storage_type> {
 public:
  BlendingFillerOperator(ColorMode &color_mode, Color color)
      : color_mode_(color_mode),
        raw_color_(color_mode_.fromArgbColor(color)),
        raw_color_full_byte_(
            SubPixelColorHelper<ColorMode, pixel_order>().RawToFullByte(
                raw_color_)) {}

  void operator()(uint8_t *p, uint32_t offset) const {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    subpixel.applySubPixelColor(raw_color_, p + offset / pixels_per_byte,
                                offset % pixels_per_byte);
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) const {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    if (pixel_index > 0) {
      do {
        if (count-- == 0) return;
        subpixel.applySubPixelColor(raw_color_, target, pixel_index++);
      } while (pixel_index < pixels_per_byte);
      pixel_index = 0;
      ++target;
    }
    uint32_t contiguous_byte_count = count / pixels_per_byte;
    pattern_fill<1>(target, contiguous_byte_count, &raw_color_full_byte_);
    count = count % pixels_per_byte;
    target += contiguous_byte_count;
    for (int i = 0; i < count; ++i) {
      subpixel.applySubPixelColor(raw_color_, target, i);
    }
  }

 private:
  ColorMode &color_mode_;
  uint8_t raw_color_;
  uint8_t raw_color_full_byte_;
};

// Used to convert a raw color to byte array, so that we can use it in
// fill_pattern, respecting the byte order.
template <typename storage_type, int bytes, ByteOrder byte_order>
void ReadRaw(storage_type in, uint8_t *out);

template <>
inline void ReadRaw<uint8_t, 1, BYTE_ORDER_BIG_ENDIAN>(uint8_t in,
                                                       uint8_t *out) {
  out[0] = in;
}

template <>
inline void ReadRaw<uint8_t, 1, BYTE_ORDER_LITTLE_ENDIAN>(uint8_t in,
                                                          uint8_t *out) {
  out[0] = in;
}

template <>
inline void ReadRaw<uint16_t, 2, BYTE_ORDER_BIG_ENDIAN>(uint16_t in,
                                                        uint8_t *out) {
  out[0] = in >> 8;
  out[1] = in;
}

template <>
inline void ReadRaw<uint16_t, 2, BYTE_ORDER_LITTLE_ENDIAN>(uint16_t in,
                                                           uint8_t *out) {
  out[0] = in;
  out[1] = in >> 8;
}

template <>
inline void ReadRaw<uint32_t, 3, BYTE_ORDER_BIG_ENDIAN>(uint32_t in,
                                                        uint8_t *out) {
  out[0] = in >> 16;
  out[1] = in >> 8;
  out[2] = in;
}

template <>
inline void ReadRaw<uint32_t, 3, BYTE_ORDER_LITTLE_ENDIAN>(uint32_t in,
                                                           uint8_t *out) {
  out[0] = in;
  out[1] = in >> 8;
  out[2] = in >> 16;
}

template <>
inline void ReadRaw<uint32_t, 4, BYTE_ORDER_BIG_ENDIAN>(uint32_t in,
                                                        uint8_t *out) {
  out[0] = in >> 24;
  out[1] = in >> 16;
  out[2] = in >> 8;
  out[3] = in;
}

template <>
inline void ReadRaw<uint32_t, 4, BYTE_ORDER_LITTLE_ENDIAN>(uint32_t in,
                                                           uint8_t *out) {
  out[0] = in;
  out[1] = in >> 8;
  out[2] = in >> 16;
  out[3] = in >> 24;
}

// For color modes in which a pixel takes up at least 1 byte.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          typename storage_type>
class BlendingFillerOperator<ColorMode, pixel_order, byte_order,
                             BLENDING_MODE_SOURCE, 1, storage_type> {
 public:
  BlendingFillerOperator(const ColorMode &color_mode, Color color)
      : color_mode_(color_mode) {
    internal::ReadRaw<typename ColorTraits<ColorMode>::storage_type,
                      ColorMode::bits_per_pixel / 8, byte_order>(
        color_mode_.fromArgbColor(color), raw_color_);
  }

  void operator()(uint8_t *p, uint32_t offset) const {
    pattern_write<ColorMode::bits_per_pixel / 8>(
        p + offset * ColorMode::bits_per_pixel / 8, raw_color_);
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) const {
    pattern_fill<ColorMode::bits_per_pixel / 8>(
        p + offset * ColorMode::bits_per_pixel / 8, count, raw_color_);
  }

 private:
  const ColorMode &color_mode_;
  uint8_t raw_color_[ColorMode::bits_per_pixel / 8];
};

inline void AddressWindow::advance() {
  if (cursor_x_ < x1_) {
    ++cursor_x_;
    offset_ += advance_x_;
  } else {
    cursor_x_ = x0_;
    ++cursor_y_;
    offset_ += advance_x_;
    offset_ += advance_y_;
  }
}

inline void AddressWindow::advance(uint32_t count) {
  if (count < x1_ - cursor_x_ + 1) {
    cursor_x_ += count;
    offset_ += advance_x_ * count;
    return;
  }
  offset_ += advance_x_ * (x1_ - cursor_x_ + 1);
  offset_ += advance_y_;
  count -= (x1_ - cursor_x_ + 1);
  cursor_x_ = x0_;
  ++cursor_y_;
  int16_t full_lines = count / width();
  count = count % width();
  if (full_lines > 0) {
    offset_ += full_lines * (advance_x_ * width() + advance_y_);
    cursor_y_ += full_lines;
  }
  cursor_x_ += count;
  offset_ += advance_x_ * count;
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order>
struct BlendingFiller {
  template <BlendingMode blending_mode>
  using Operator =
      BlendingFillerOperator<ColorMode, pixel_order, byte_order, blending_mode>;
};

// GenericFiller is similar to BlendingFiller, but it resolves the blender at
// run time. It might therefore be a bit slower, but it does not waste program
// space.

// For sub-byte color modes.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          uint8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
class GenericFiller {
 public:
  GenericFiller(ColorMode &color_mode, BlendingMode blending_mode, Color color)
      : color_mode_(color_mode), color_(color), blending_mode_(blending_mode) {}

  void operator()(uint8_t *p, uint32_t offset) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    auto color = ApplyRawBlending(
        blending_mode_, subpixel.ReadSubPixelColor(*target, pixel_index),
        color_, color_mode_);
    subpixel.applySubPixelColor(color, target, pixel_index);
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    // TODO: this loop can be optimized to work on an array of color at a time.
    while (count-- > 0) {
      auto color = ApplyRawBlending(
          blending_mode_, subpixel.ReadSubPixelColor(*target, pixel_index),
          color_, color_mode_);
      subpixel.applySubPixelColor(color, target, pixel_index);
      if (++pixel_index == pixels_per_byte) {
        pixel_index = 0;
        target++;
      }
    }
  }

 private:
  ColorMode &color_mode_;
  Color color_;
  BlendingMode blending_mode_;
};

// For color modes in which a pixel takes up at least 1 byte.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          typename storage_type>
class GenericFiller<ColorMode, pixel_order, byte_order, 1, storage_type> {
 public:
  GenericFiller(const ColorMode &color_mode, BlendingMode blending_mode,
                Color color)
      : color_mode_(color_mode), color_(color), blending_mode_(blending_mode) {}

  void operator()(uint8_t *p, uint32_t offset) const {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    itr.write(
        ApplyRawBlending(blending_mode_, itr.read(), color_, color_mode_));
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) const {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    while (count-- > 0) {
      itr.write(
          ApplyRawBlending(blending_mode_, itr.read(), color_, color_mode_));
      ++itr;
    }
  }

 private:
  const ColorMode &color_mode_;
  Color color_;
  BlendingMode blending_mode_;
};

inline BlendingMode ResolveBlendingModeForFill(
    BlendingMode mode, TransparencyMode transparency_mode, Color color) {
  if (transparency_mode == TRANSPARENCY_NONE) {
    if (color.isOpaque()) {
      switch (mode) {
        case BLENDING_MODE_SOURCE_OVER:
        case BLENDING_MODE_SOURCE_OVER_OPAQUE:
        case BLENDING_MODE_SOURCE_IN:
        case BLENDING_MODE_SOURCE_ATOP: {
          return BLENDING_MODE_SOURCE;
        }
        case BLENDING_MODE_DESTINATION_IN:
        case BLENDING_MODE_DESTINATION_OUT:
        case BLENDING_MODE_DESTINATION_ATOP:
        case BLENDING_MODE_CLEAR:
        case BLENDING_MODE_EXCLUSIVE_OR: {
          return BLENDING_MODE_DESTINATION;
        }
        default: {
          return mode;
        }
      };
    } else if (color.a() == 0) {
      switch (mode) {
        case BLENDING_MODE_SOURCE:
        case BLENDING_MODE_SOURCE_IN:
        case BLENDING_MODE_SOURCE_OUT:
        case BLENDING_MODE_DESTINATION_IN:
        case BLENDING_MODE_DESTINATION_ATOP:
        case BLENDING_MODE_SOURCE_OVER:
        case BLENDING_MODE_SOURCE_OVER_OPAQUE:
        case BLENDING_MODE_SOURCE_ATOP:
        case BLENDING_MODE_DESTINATION_OVER:
        case BLENDING_MODE_DESTINATION_OUT:
        case BLENDING_MODE_EXCLUSIVE_OR: {
          return BLENDING_MODE_DESTINATION;
        }
        default: {
          return mode;
        }
      }
    } else {
      switch (mode) {
        case BLENDING_MODE_SOURCE_OVER:
        case BLENDING_MODE_SOURCE_ATOP: {
          return BLENDING_MODE_SOURCE_OVER_OPAQUE;
        }
        case BLENDING_MODE_SOURCE_IN: {
          return BLENDING_MODE_SOURCE;
        }
        case BLENDING_MODE_SOURCE_OUT:
        case BLENDING_MODE_DESTINATION_OVER:
        case BLENDING_MODE_DESTINATION_IN:
        case BLENDING_MODE_DESTINATION_OUT:
        case BLENDING_MODE_DESTINATION_ATOP:
        case BLENDING_MODE_CLEAR:
        case BLENDING_MODE_EXCLUSIVE_OR: {
          return BLENDING_MODE_DESTINATION;
        }
        default: {
          return mode;
        }
      }
    }
  } else {
    if (color.isOpaque()) {
      switch (mode) {
        case BLENDING_MODE_SOURCE_OVER:
        case BLENDING_MODE_SOURCE_OVER_OPAQUE: {
          return BLENDING_MODE_SOURCE;
        }
        case BLENDING_MODE_SOURCE_ATOP: {
          return BLENDING_MODE_SOURCE_IN;
        }
        case BLENDING_MODE_DESTINATION_IN: {
          return BLENDING_MODE_DESTINATION;
        }
        case BLENDING_MODE_DESTINATION_OUT: {
          return BLENDING_MODE_CLEAR;
        }
        case BLENDING_MODE_DESTINATION_ATOP: {
          return BLENDING_MODE_DESTINATION_OVER;
        }
        case BLENDING_MODE_EXCLUSIVE_OR: {
          return BLENDING_MODE_SOURCE_OUT;
        }
        default: {
          return mode;
        }
      };
    } else if (color.a() == 0) {
      switch (mode) {
        case BLENDING_MODE_SOURCE:
        case BLENDING_MODE_SOURCE_IN:
        case BLENDING_MODE_SOURCE_OUT:
        case BLENDING_MODE_DESTINATION_IN:
        case BLENDING_MODE_DESTINATION_ATOP: {
          return BLENDING_MODE_CLEAR;
        }
        case BLENDING_MODE_SOURCE_OVER:
        case BLENDING_MODE_SOURCE_OVER_OPAQUE:
        case BLENDING_MODE_SOURCE_ATOP:
        case BLENDING_MODE_DESTINATION_OVER:
        case BLENDING_MODE_DESTINATION_OUT:
        case BLENDING_MODE_EXCLUSIVE_OR: {
          return BLENDING_MODE_DESTINATION;
        }
        default: {
          return mode;
        }
      }
    } else {
      return mode;
    }
  }
  return mode;
}

inline BlendingMode ResolveBlendingModeForWrite(
    BlendingMode mode, TransparencyMode transparency_mode) {
  if (transparency_mode == TRANSPARENCY_NONE) {
    switch (mode) {
      case BLENDING_MODE_SOURCE_OVER:
      case BLENDING_MODE_SOURCE_ATOP: {
        return BLENDING_MODE_SOURCE_OVER_OPAQUE;
      }
      case BLENDING_MODE_SOURCE_IN: {
        return BLENDING_MODE_SOURCE;
      }
      case BLENDING_MODE_SOURCE_OUT:
      case BLENDING_MODE_DESTINATION_OVER:
      case BLENDING_MODE_DESTINATION_IN:
      case BLENDING_MODE_DESTINATION_OUT:
      case BLENDING_MODE_DESTINATION_ATOP:
      case BLENDING_MODE_CLEAR:
      case BLENDING_MODE_EXCLUSIVE_OR: {
        return BLENDING_MODE_DESTINATION;
      }
      default: {
        return mode;
      }
    }
  } else {
    return mode;
  }
}

}  // namespace internal

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                     storage_type>::orientationUpdated() {
  orienter_.setOrientation(orientation());
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                     storage_type>::setAddress(uint16_t x0, uint16_t y0,
                                               uint16_t x1, uint16_t y1,
                                               BlendingMode blending_mode) {
  window_.setAddress(x0, y0, x1, y1, raw_width(), raw_height(),
                     orienter_.orientation());
  if (blending_mode != BLENDING_MODE_SOURCE) {
    blending_mode = internal::ResolveBlendingModeForWrite(
        blending_mode, color_mode_.transparency());
  }
  blending_mode_ = blending_mode;
}

// template <typename Device, typename ColorMode, ColorPixelOrder pixel_order,
//           ByteOrder byte_order>
// struct WriteOp {
//   template <BlendingMode blending_mode>
//   void operator()(Device &device, ColorMode &color_mode, Color *color,
//                   uint16_t pixel_count) const {
//     internal::BlendingWriter<ColorMode, pixel_order, byte_order,
//     blending_mode>
//         writer(color_mode, color);
//     device.writeToWindow(writer, pixel_count);
//   }
// };

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                     storage_type>::write(Color *color, uint32_t pixel_count) {
  if (blending_mode_ == BLENDING_MODE_SOURCE) {
    typename internal::BlendingWriter<ColorMode, pixel_order, byte_order>::
        template Operator<BLENDING_MODE_SOURCE>
            writer(color_mode_, color);
    writeToWindow(writer, pixel_count);
  } else {
    if (blending_mode_ == BLENDING_MODE_DESTINATION) return;
    if (blending_mode_ == BLENDING_MODE_SOURCE_OVER_OPAQUE) {
      typename internal::BlendingWriter<ColorMode, pixel_order, byte_order>::
          template Operator<BLENDING_MODE_SOURCE_OVER_OPAQUE>
              writer(color_mode_, color);
      writeToWindow(writer, pixel_count);
    } else if (blending_mode_ == BLENDING_MODE_SOURCE_OVER) {
      typename internal::BlendingWriter<ColorMode, pixel_order, byte_order>::
          template Operator<BLENDING_MODE_SOURCE_OVER>
              writer(color_mode_, color);
      writeToWindow(writer, pixel_count);
    } else {
      internal::GenericWriter<ColorMode, pixel_order, byte_order> writer(
          color_mode_, blending_mode_, color);
      writeToWindow(writer, pixel_count);
    }
  }

  // internal::BlenderSpecialization<
  //     WriteOp<OffscreenDevice<ColorMode, pixel_order, byte_order,
  //                             pixels_per_byte, storage_type>,
  //             ColorMode, pixel_order, byte_order>>(
  //     blending_mode_, *this, color_mode(), color, pixel_count);
}

// template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder
// byte_order> struct WritePixelsOp {
//   template <BlendingMode blending_mode>
//   void operator()(ColorMode &color_mode, Color *color, uint8_t *buffer,
//                   int16_t w, int16_t *x, int16_t *y,
//                   uint16_t pixel_count) const {
//     internal::BlendingWriter<ColorMode, pixel_order, byte_order,
//     blending_mode>
//         write(color_mode, color);
//     while (pixel_count-- > 0) {
//       write(buffer, *x++ + *y++ * w);
//     }
//   }
// };

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                     storage_type>::writePixels(BlendingMode blending_mode,
                                                Color *color, int16_t *x,
                                                int16_t *y,
                                                uint16_t pixel_count) {
  uint8_t *buffer = buffer_;
  int16_t w = raw_width();
  orienter_.OrientPixels(x, y, pixel_count);
  if (blending_mode == BLENDING_MODE_SOURCE) {
    typename internal::BlendingWriter<ColorMode, pixel_order, byte_order>::
        template Operator<BLENDING_MODE_SOURCE>
            write(color_mode_, color);
    while (pixel_count-- > 0) {
      write(buffer, *x++ + *y++ * w);
    }
  } else {
    blending_mode = internal::ResolveBlendingModeForWrite(
        blending_mode, color_mode_.transparency());
    if (blending_mode == BLENDING_MODE_DESTINATION) return;
    if (blending_mode == BLENDING_MODE_SOURCE_OVER_OPAQUE) {
      typename internal::BlendingWriter<ColorMode, pixel_order, byte_order>::
          template Operator<BLENDING_MODE_SOURCE_OVER_OPAQUE>
              write(color_mode_, color);
      while (pixel_count-- > 0) {
        write(buffer, *x++ + *y++ * w);
      }
    } else if (blending_mode == BLENDING_MODE_SOURCE_OVER) {
      typename internal::BlendingWriter<ColorMode, pixel_order, byte_order>::
          template Operator<BLENDING_MODE_SOURCE_OVER>
              write(color_mode_, color);
      while (pixel_count-- > 0) {
        write(buffer, *x++ + *y++ * w);
      }
    } else {
      internal::GenericWriter<ColorMode, pixel_order, byte_order> write(
          color_mode_, blending_mode, color);
      while (pixel_count-- > 0) {
        write(buffer, *x++ + *y++ * w);
      }
    }
  }
  // internal::BlenderSpecialization<
  //     WritePixelsOp<ColorMode, pixel_order, byte_order>>(
  //     blending_mode, color_mode(), color, buffer, w, x, y, pixel_count);
}

// template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder
// byte_order> struct FillPixelsOp {
//   template <BlendingMode blending_mode>
//   void operator()(ColorMode &color_mode, Color color, uint8_t *buffer,
//                   int16_t w, int16_t *x, int16_t *y,
//                   uint16_t pixel_count) const {
//     internal::BlendingFiller<ColorMode, pixel_order, byte_order,
//     blending_mode>
//         fill(color_mode, color);
//     while (pixel_count-- > 0) {
//       fill(buffer, *x++ + *y++ * w);
//     }
//   }
// };

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                     storage_type>::fillPixels(BlendingMode blending_mode,
                                               Color color, int16_t *x,
                                               int16_t *y,
                                               uint16_t pixel_count) {
  uint8_t *buffer = buffer_;
  int16_t w = raw_width();
  orienter_.OrientPixels(x, y, pixel_count);
  if (blending_mode != BLENDING_MODE_SOURCE) {
    blending_mode = internal::ResolveBlendingModeForFill(
        blending_mode, color_mode_.transparency(), color);
    if (blending_mode == BLENDING_MODE_DESTINATION) return;
  }
  if (blending_mode == BLENDING_MODE_DESTINATION) return;
  if (blending_mode == BLENDING_MODE_SOURCE_OVER_OPAQUE) {
    typename internal::BlendingFiller<ColorMode, pixel_order, byte_order>::
        template Operator<BLENDING_MODE_SOURCE_OVER_OPAQUE>
            fill(color_mode_, color);
    while (pixel_count-- > 0) {
      fill(buffer, *x++ + *y++ * w);
    }
  } else if (blending_mode == BLENDING_MODE_SOURCE_OVER) {
    typename internal::BlendingFiller<ColorMode, pixel_order, byte_order>::
        template Operator<BLENDING_MODE_SOURCE_OVER>
            fill(color_mode_, color);
    while (pixel_count-- > 0) {
      fill(buffer, *x++ + *y++ * w);
    }
  } else {
    internal::GenericFiller<ColorMode, pixel_order, byte_order> fill(
        color_mode_, blending_mode, color);
    while (pixel_count-- > 0) {
      fill(buffer, *x++ + *y++ * w);
    }
  }

  // internal::BlenderSpecialization<
  //     FillPixelsOp<ColorMode, pixel_order, byte_order>>(
  //     blending_mode, color_mode(), color, buffer, w, x, y, pixel_count);
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                     storage_type>::writeRects(BlendingMode blending_mode,
                                               Color *color, int16_t *x0,
                                               int16_t *y0, int16_t *x1,
                                               int16_t *y1, uint16_t count) {
  orienter_.OrientRects(x0, y0, x1, y1, count);
  if (blending_mode != BLENDING_MODE_SOURCE) {
    blending_mode = internal::ResolveBlendingModeForWrite(
        blending_mode, color_mode_.transparency());
    if (blending_mode == BLENDING_MODE_DESTINATION) return;
  }
  while (count-- > 0) {
    fillRectsAbsolute(blending_mode, *color++, x0++, y0++, x1++, y1++, 1);
  }
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                     storage_type>::fillRects(BlendingMode blending_mode,
                                              Color color, int16_t *x0,
                                              int16_t *y0, int16_t *x1,
                                              int16_t *y1, uint16_t count) {
  orienter_.OrientRects(x0, y0, x1, y1, count);
  if (blending_mode != BLENDING_MODE_SOURCE) {
    blending_mode = internal::ResolveBlendingModeForFill(
        blending_mode, color_mode_.transparency(), color);
    if (blending_mode == BLENDING_MODE_DESTINATION) return;
  }
  if (y0 == y1) {
    fillHlinesAbsolute(blending_mode, color, x0, y0, x1, count);
  } else if (x0 == x1) {
    fillVlinesAbsolute(blending_mode, color, x0, y0, y1, count);
  } else {
    fillRectsAbsolute(blending_mode, color, x0, y0, x1, y1, count);
  }
}

template <typename Filler>
void fillRectsAbsoluteImpl(Filler &fill, uint8_t *buffer, int16_t width,
                           int16_t *x0, int16_t *y0, int16_t *x1, int16_t *y1,
                           uint32_t count) {
  while (count-- > 0) {
    uint32_t offset = *x0 + *y0 * width;
    int16_t my_w = (*x1 - *x0 + 1);
    int16_t my_h = (*y1 - *y0 + 1);
    if (my_w == width) {
      fill(buffer, offset, my_w * my_h);
    } else {
      while (my_h-- > 0) {
        fill(buffer, offset, my_w);
        offset += width;
      }
    }
    x0++;
    y0++;
    x1++;
    y1++;
  }
}

template <typename Filler>
void fillHlinesAbsoluteImpl(Filler &fill, uint8_t *buffer, int16_t width,
                            int16_t *x0, int16_t *y0, int16_t *x1,
                            uint16_t count) {
  while (count-- > 0) {
    int16_t n = *x1++ - *x0 + 1;
    fill(buffer, *x0++ + *y0++ * width, n);
  }
}

template <typename Filler>
void fillVlinesAbsoluteImpl(Filler &fill, uint8_t *buffer, int16_t width,
                            int16_t *x0, int16_t *y0, int16_t *y1,
                            uint16_t count) {
  while (count-- > 0) {
    int16_t n = *y1++ - *y0 + 1;
    uint32_t offset = *x0++ + *y0++ * width;
    while (n-- > 0) {
      fill(buffer, offset);
      offset += width;
    }
  }
}

// template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder
// byte_order> struct FillRectsOp {
//   template <BlendingMode blending_mode>
//   void operator()(ColorMode &color_mode, Color color, uint8_t *buffer,
//                   int16_t w, int16_t *x0, int16_t *y0, int16_t *x1, int16_t
//                   *y1, uint16_t count) const {
//     internal::BlendingFiller<ColorMode, pixel_order, byte_order,
//     blending_mode>
//         fill(color_mode, color);
//     fillRectsAbsoluteImpl(fill, buffer, w, x0, y0, x1, y1, count);
//   }
// };

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                     storage_type>::fillRectsAbsolute(BlendingMode
                                                          blending_mode,
                                                      Color color, int16_t *x0,
                                                      int16_t *y0, int16_t *x1,
                                                      int16_t *y1,
                                                      uint16_t count) {
  if (y0 == y1) {
    fillHlinesAbsolute(blending_mode, color, x0, y0, x1, count);
  } else if (x0 == x1) {
    fillVlinesAbsolute(blending_mode, color, x0, y0, y1, count);
  } else {
    int16_t w = raw_width();
    uint8_t *buffer = buffer_;
    if (blending_mode == BLENDING_MODE_SOURCE) {
      typename internal::BlendingFiller<ColorMode, pixel_order, byte_order>::
          template Operator<BLENDING_MODE_SOURCE>
              fill(color_mode_, color);
      fillRectsAbsoluteImpl(fill, buffer, w, x0, y0, x1, y1, count);
    } else if (blending_mode == BLENDING_MODE_SOURCE_OVER_OPAQUE) {
      typename internal::BlendingFiller<ColorMode, pixel_order, byte_order>::
          template Operator<BLENDING_MODE_SOURCE_OVER_OPAQUE>
              fill(color_mode_, color);
      fillRectsAbsoluteImpl(fill, buffer, w, x0, y0, x1, y1, count);
    } else if (blending_mode == BLENDING_MODE_SOURCE_OVER) {
      typename internal::BlendingFiller<ColorMode, pixel_order, byte_order>::
          template Operator<BLENDING_MODE_SOURCE_OVER>
              fill(color_mode_, color);
      fillRectsAbsoluteImpl(fill, buffer, w, x0, y0, x1, y1, count);
    } else {
      internal::GenericFiller<ColorMode, pixel_order, byte_order> fill(
          color_mode_, blending_mode, color);
      fillRectsAbsoluteImpl(fill, buffer, w, x0, y0, x1, y1, count);
    }
  }
}

// template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder
// byte_order> struct FillHLinesOp {
//   template <BlendingMode blending_mode>
//   void operator()(ColorMode &color_mode, Color color, uint8_t *buffer,
//                   int16_t w, int16_t *x0, int16_t *y0, int16_t *x1,
//                   uint16_t count) const {
//     internal::BlendingFiller<ColorMode, pixel_order, byte_order,
//     blending_mode>
//         fill(color_mode, color);
//     fillHlinesAbsoluteImpl(fill, buffer, w, x0, y0, x1, count);
//   }
// };

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                     storage_type>::fillHlinesAbsolute(BlendingMode
                                                           blending_mode,
                                                       Color color, int16_t *x0,
                                                       int16_t *y0, int16_t *x1,
                                                       uint16_t count) {
  uint8_t *buffer = buffer_;
  int16_t w = raw_width();
  if (blending_mode == BLENDING_MODE_SOURCE) {
    typename internal::BlendingFiller<ColorMode, pixel_order, byte_order>::
        template Operator<BLENDING_MODE_SOURCE>
            fill(color_mode_, color);
    fillHlinesAbsoluteImpl(fill, buffer, w, x0, y0, x1, count);
  } else if (blending_mode == BLENDING_MODE_SOURCE_OVER_OPAQUE) {
    typename internal::BlendingFiller<ColorMode, pixel_order, byte_order>::
        template Operator<BLENDING_MODE_SOURCE_OVER_OPAQUE>
            fill(color_mode_, color);
    fillHlinesAbsoluteImpl(fill, buffer, w, x0, y0, x1, count);
  } else if (blending_mode == BLENDING_MODE_SOURCE_OVER) {
    typename internal::BlendingFiller<ColorMode, pixel_order, byte_order>::
        template Operator<BLENDING_MODE_SOURCE_OVER>
            fill(color_mode_, color);
    fillHlinesAbsoluteImpl(fill, buffer, w, x0, y0, x1, count);
  } else {
    internal::GenericFiller<ColorMode, pixel_order, byte_order> fill(
        color_mode_, blending_mode, color);
    fillHlinesAbsoluteImpl(fill, buffer, w, x0, y0, x1, count);
  }
}
// internal::BlenderSpecialization<
//     FillHLinesOp<ColorMode, pixel_order, byte_order>>(
//     blending_mode, color_mode(), color, buffer_, raw_width(), x0, y0, x1,
//     count);
// }

// template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder
// byte_order> struct FillVLinesOp {
//   template <BlendingMode blending_mode>
//   void operator()(ColorMode &color_mode, Color color, uint8_t *buffer,
//                   int16_t w, int16_t *x0, int16_t *y0, int16_t *y1,
//                   uint16_t count) const {
//     internal::BlendingFiller<ColorMode, pixel_order, byte_order,
//     blending_mode>
//         fill(color_mode, color);
//     fillVlinesAbsoluteImpl(fill, buffer, w, x0, y0, y1, count);
//   }
// };

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void OffscreenDevice<ColorMode, pixel_order, byte_order, pixels_per_byte,
                     storage_type>::fillVlinesAbsolute(BlendingMode
                                                           blending_mode,
                                                       Color color, int16_t *x0,
                                                       int16_t *y0, int16_t *y1,
                                                       uint16_t count) {
  uint8_t *buffer = buffer_;
  int16_t w = raw_width();
  if (blending_mode == BLENDING_MODE_SOURCE) {
    typename internal::BlendingFiller<ColorMode, pixel_order, byte_order>::
        template Operator<BLENDING_MODE_SOURCE>
            fill(color_mode_, color);
    fillVlinesAbsoluteImpl(fill, buffer, w, x0, y0, y1, count);
  } else if (blending_mode == BLENDING_MODE_SOURCE_OVER_OPAQUE) {
    typename internal::BlendingFiller<ColorMode, pixel_order, byte_order>::
        template Operator<BLENDING_MODE_SOURCE_OVER_OPAQUE>
            fill(color_mode_, color);
    fillVlinesAbsoluteImpl(fill, buffer, w, x0, y0, y1, count);
  } else if (blending_mode == BLENDING_MODE_SOURCE_OVER) {
    typename internal::BlendingFiller<ColorMode, pixel_order, byte_order>::
        template Operator<BLENDING_MODE_SOURCE_OVER>
            fill(color_mode_, color);
    fillVlinesAbsoluteImpl(fill, buffer, w, x0, y0, y1, count);
  } else {
    internal::GenericFiller<ColorMode, pixel_order, byte_order> fill(
        color_mode_, blending_mode, color);
    fillVlinesAbsoluteImpl(fill, buffer, w, x0, y0, y1, count);
  }
  // internal::BlenderSpecialization<
  //     FillVLinesOp<ColorMode, pixel_order, byte_order>>(
  //     blending_mode, color_mode(), color, buffer_, raw_width(), x0, y0, y1,
  //     count);
}

namespace internal {

inline void Orienter::OrientPixels(int16_t *&x, int16_t *&y, int16_t count) {
  if (orientation_ != Orientation::Default()) {
    if (orientation_.isXYswapped()) {
      std::swap(x, y);
    }
    if (orientation_.isRightToLeft()) {
      revertX(x, count);
    }
    if (orientation_.isBottomToTop()) {
      revertY(y, count);
    }
  }
}

inline void Orienter::OrientRects(int16_t *&x0, int16_t *&y0, int16_t *&x1,
                                  int16_t *&y1, int16_t count) {
  if (orientation_ != Orientation::Default()) {
    if (orientation_.isXYswapped()) {
      std::swap(x0, y0);
      std::swap(x1, y1);
    }
    if (orientation_.isRightToLeft()) {
      revertX(x0, count);
      if (x0 != x1) {
        revertX(x1, count);
        std::swap(x0, x1);
      }
    }
    if (orientation_.isBottomToTop()) {
      revertY(y0, count);
      if (y0 != y1) {
        revertY(y1, count);
        std::swap(y0, y1);
      }
    }
  }
}

inline void Orienter::revertX(int16_t *x, uint16_t count) {
  int16_t xMax = xMax_;
  while (count-- > 0) {
    *x = xMax - *x;
    ++x;
  }
}

inline void Orienter::revertY(int16_t *y, uint16_t count) {
  int16_t yMax = yMax_;
  while (count-- > 0) {
    *y = yMax - *y;
    ++y;
  }
}

}  // namespace internal

}  // namespace roo_display