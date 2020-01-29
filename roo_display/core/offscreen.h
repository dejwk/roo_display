#pragma once

// Support for drawing to in-memory buffers, using various color modes.

#include "roo_display.h"
#include "roo_display/core/byte_order.h"
#include "roo_display/core/color.h"
#include "roo_display/core/memfill.h"
#include "roo_display/core/raster.h"

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

// The Offscreen class is an in-memory implementation of DisplayDevice. It can
// be used just like a regular display device, to buffer arbitrary images and
// display operations. It implements both Drawable and Streamable contracts,
// which allows to be easily drawn to other display devices.
// For example, to draw a sub-rectangle of the Offscreen, simply use
// CreateClippedStream(offscreen, rect).
//
// Offscreen supports arbitrary color modes, byte orders, and pixel orders,
// which can be specified as template parameters:
// * pixel_order applies to sub-byte color modes, and specifies if, within
//   a single byte, the left-most pixel is mapped to the most or the least
//   significant bit(s).
// * byte_order applies to multi-byte color modes, and specifies if the bytes
//   representing a single pixel are stored in big or little endian.
//
// If you draw to/from the offscreen using its public methods only, the choice
// of these two parameters does not really matter; use defaults. But, if you're
// importing or exporting raw data from/to some existing format or device, you
// may need to set these parameters to match that format. For example, XBitmap
// uses LSB_FIRST.
//
// Bounding rectangle, specified during construction, determines how the
// Offscreen behaves when it is drawn to another display device. As any other
// Drawable, Offscreen can have a bounding rectangle that does not start at
// (0,0). The canvas for writing *to* the offscreen as a display device, on
// the other hand, always starts at (0,0), and it is sensitive to the 'device
// orientation' of the Offscreen. When created, the Offscreen has the default
// orientation (Right-Down). You can modify the orientation at any time, using
// setOrientation(). New orientation will not affect existing content, but it
// will affect all future write operations. For example, setting the orientation
// to 'Orientation::Default().rotateLeft()' allows to render content rotated
// counter-clockwise by 90 degrees.
//
// Note: to use the content of the offscreen as Stremable, e.g. to super-impose
// the content over / under another streamable, use raster().
template <typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN,
          int8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
class Offscreen : public DisplayDevice, public Drawable {
 public:
  // Creates an offscreen with specified geometry, using the designated buffer.
  // The buffer must have sufficient capacity, determined as
  // (width * height * ColorMode::bits_per_pixel + 7) / 8. The buffer is not
  // modified; it can contain pre-existing content.
  Offscreen(int16_t width, int16_t height, uint8_t *buffer,
            ColorMode color_mode = ColorMode())
      : Offscreen(Box(0, 0, width - 1, height - 1), buffer, color_mode) {}

  // Creates an offscreen with specified geometry, using the designated buffer.
  // The buffer must have sufficient capacity, determined as
  // (width * height * ColorMode::bits_per_pixel + 7) / 8. The buffer is not
  // modified; it can contain pre-existing content.
  // The specified extents dictate how the Offscreen will behave when drawn
  // (as a Drawable).
  Offscreen(Box extents, uint8_t *buffer, ColorMode color_mode = ColorMode())
      : DisplayDevice(extents.width(), extents.height()),
        buffer_(buffer),
        owns_buffer_(false),
        orienter_(extents.width(), extents.height(), Orientation::Default()),
        raster_(extents, buffer, color_mode) {}

  // Creates an offscreen with specified geometry, using an internally allocated
  // buffer. The buffer is not pre-initialized; it contains random bytes.
  Offscreen(int16_t width, int16_t height, ColorMode color_mode = ColorMode())
      : Offscreen(Box(0, 0, width - 1, height - 1), color_mode) {}

  // Creates an offscreen with specified geometry, using an internally allocated
  // buffer. The buffer is not pre-initialized; it contains random bytes.
  // The specified extents dictate how the Offscreen will behave when drawn
  // (as a Drawable).
  Offscreen(Box extents, ColorMode color_mode = ColorMode())
      : DisplayDevice(extents.width(), extents.height()),
        buffer_(
            new uint8_t[(ColorMode::bits_per_pixel * extents.area() + 7) / 8]),
        owns_buffer_(true),
        orienter_(extents.width(), extents.height(), Orientation::Default()),
        raster_(extents.width(), extents.height(), buffer_, color_mode) {}

  // Creates an offscreen with specified geometry, using an internally allocated
  // buffer. The buffer is pre-filled using the specified  color.
  Offscreen(int16_t width, int16_t height, Color fillColor,
            ColorMode color_mode = ColorMode())
      : Offscreen(Box(0, 0, width - 1, height - 1), fillColor, color_mode) {}

  // Creates an offscreen with specified geometry, using an internally allocated
  // buffer. The buffer is pre-filled using the specified  color.
  // The specified extents dictate how the Offscreen will behave when drawn
  // (as a Drawable).
  Offscreen(Box extents, Color fillColor, ColorMode color_mode = ColorMode())
      : Offscreen(extents, color_mode) {
    fillRect(0, 0, extents.width() - 1, extents.height() - 1, fillColor);
  }

  // Convenience constructor that makes a RAM copy of the specified drawable.
  Offscreen(const Drawable &d, ColorMode color_mode = ColorMode())
      : Offscreen(d.extents(), color_mode) {
    if (color_mode.transparency() != TRANSPARENCY_NONE) {
      fillRect(0, 0, raw_width() - 1, raw_height() - 1, color::Transparent);
    }
    Box extents = d.extents();
    Surface s(this, -extents.xMin(), -extents.yMin(),
              Box(0, 0, raw_width(), raw_height()));
    s.drawObject(d);
  }

  Offscreen(Offscreen &&other) = delete;

  ~Offscreen() override {
    if (owns_buffer_) delete[] buffer_;
  }

  void drawTo(const Surface &s) const override { streamToSurface(s, raster()); }

  // This is to implement the Drawable interface.
  Box extents() const override { return raster_.extents(); }

  void orientationUpdated();

  void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) override;

  void write(PaintMode mode, Color *color, uint32_t pixel_count) override;

  void writePixels(PaintMode mode, Color *color, int16_t *x, int16_t *y,
                   uint16_t pixel_count) override;

  void fillPixels(PaintMode mode, Color color, int16_t *x, int16_t *y,
                  uint16_t pixel_count) override;

  void writeRects(PaintMode mode, Color *color, int16_t *x0, int16_t *y0,
                  int16_t *x1, int16_t *y1, uint16_t count) override;

  void fillRects(PaintMode mode, Color color, int16_t *x0, int16_t *y0,
                 int16_t *x1, int16_t *y1, uint16_t count) override;

  const ColorMode &color_mode() const { return raster_.color_mode(); }

  const Raster<ConstDramResource, ColorMode, pixel_order, byte_order> &raster()
      const {
    return raster_;
  }

  // Allows direct access to the underlying buffer.
  uint8_t *buffer() { return buffer_; }
  const uint8_t *buffer() const { return buffer_; }

 private:
  template <typename Writer>
  void writeToWindow(Writer &write, uint32_t count) {
    while (count-- > 0) {
      write(buffer_, window_.offset());
      window_.advance();
    }
  }

  void fillRectsAbsolute(PaintMode mode, Color color, int16_t *x0, int16_t *y0,
                         int16_t *x1, int16_t *y1, uint16_t count);

  void fillHlinesAbsolute(PaintMode mode, Color color, int16_t *x0, int16_t *y0,
                          int16_t *x1, uint16_t count);

  void fillVlinesAbsolute(PaintMode mode, Color color, int16_t *x0, int16_t *y0,
                          int16_t *y1, uint16_t count);

  uint8_t *buffer_;
  bool owns_buffer_;
  internal::Orienter orienter_;
  // Streaming read acess.
  Raster<ConstDramResource, ColorMode, pixel_order, byte_order> raster_;
  internal::AddressWindow window_;
};

// A convenience class that bundles 'Offscreen' and 'Display', so that you can
// start drawing to an offscreen more easily, like this:
//
// OffscreenDisplay<Rgb565> offscreen;
// DrawingContext dc(&offscreen);
// dc.draw(...);
template <typename ColorMode,
          ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST,
          ByteOrder byte_order = BYTE_ORDER_BIG_ENDIAN,
          int8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
class OffscreenDisplay : public Display, public Drawable {
 public:
  typedef Offscreen<ColorMode, pixel_order, byte_order, pixels_per_byte,
                    storage_type>
      OffscreenDevice;

  // Look at class Offscreen, above, to see supported parameter options.
  template <typename... Args>
  OffscreenDisplay(Args &&... args)
      : Display(new OffscreenDevice(std::forward<Args>(args)...), nullptr) {}

  ~OffscreenDisplay() {
    delete offscreen();
  }

  const OffscreenDevice &offscreen() const {
    return (const OffscreenDevice &)output();
  }

  OffscreenDevice *offscreen() { return (OffscreenDevice *)output(); }

  const Raster<ConstDramResource, ColorMode, pixel_order, byte_order> &raster()
      const {
    return offscreen().raster();
  }

  void drawTo(const Surface &s) const override { streamToSurface(s, raster()); }

  Box extents() const override { return raster().extents(); }
};

// Implementation details follow.

// Writer template contract specifies how to write pixel, or a sequence of
// pixels, using a specified using a specified sequence of colors, into a buffer
// with a given 'start' address, and a given pixel_offset. The writer does not
// need to know the geometry (i.e. width and height) of the display; it treats
// the buffer as a contiguous sequence of pixels, starting at the top-left
// corner, and going left-to-right, and then top-to-bottom.

namespace internal {

// For multi-byte color modes, supports reading and writing raw byte content
// from and to DRAM. Used by Writers and Fillers, below.
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

// ReplaceWriter is a Writer that will overwrite previous content of the buffer
// using the specified colors.

// For sub-byte color modes.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
class ReplaceWriter {
 public:
  ReplaceWriter(const ColorMode &color_mode, const Color *color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    subpixel.applySubPixelColor(color_mode_.fromArgbColor(*color_++), target,
                                pixel_index);
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    while (count-- > 0) {
      Color bg = color_mode_.toArgbColor(
          subpixel.ReadSubPixelColor(*target, pixel_index));
      subpixel.applySubPixelColor(
          color_mode_.fromArgbColor(alphaBlend(bg, *color_++)), target,
          pixel_index);
      if (++pixel_index == pixels_per_byte) {
        pixel_index = 0;
        target++;
      }
    }
  }

 private:
  const ColorMode &color_mode_;
  const Color *color_;
};

// For color modes in which a pixel takes up at least 1 byte.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          typename storage_type>
class ReplaceWriter<ColorMode, pixel_order, byte_order, 1, storage_type> {
 public:
  ReplaceWriter(const ColorMode &color_mode, const Color *color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    itr.write(color_mode_.fromArgbColor(*color_++));
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    while (count-- > 0) {
      itr.write(color_mode_.fromArgbColor(*color_++));
      ++itr;
    }
  }

 private:
  const ColorMode &color_mode_;
  const Color *color_;
};

// BlendWriter is a Writer that will alpha-blend written pixels over the
// previous content.

// For sub-byte color modes.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
class BlendWriter {
 public:
  BlendWriter(const ColorMode &color_mode, const Color *color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    Color bg = color_mode_.toArgbColor(
        subpixel.ReadSubPixelColor(*target, pixel_index));
    subpixel.applySubPixelColor(
        color_mode_.fromArgbColor(alphaBlend(bg, *color_++)), target,
        pixel_index);
  }

  void operator()(storage_type *p, uint32_t offset, uint32_t count) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    while (count-- > 0) {
      Color bg = color_mode_.toArgbColor(
          subpixel.ReadSubPixelColor(*target, pixel_index));
      subpixel.applySubPixelColor(
          color_mode_.fromArgbColor(alphaBlend(bg, *color_++)), target,
          pixel_index);
      if (++pixel_index == pixels_per_byte) {
        pixel_index = 0;
        target++;
      }
    }
  }

 private:
  const ColorMode &color_mode_;
  const Color *color_;
};

// For color modes in which a pixel takes up at least 1 byte.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          typename storage_type>
class BlendWriter<ColorMode, pixel_order, byte_order, 1, storage_type> {
 public:
  BlendWriter(const ColorMode &color_mode, const Color *color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    itr.write(color_mode_.fromArgbColor(
        alphaBlend(color_mode_.toArgbColor(itr.read()), *color_++)));
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    while (count-- > 0) {
      itr.write(color_mode_.fromArgbColor(
          alphaBlend(color_mode_.toArgbColor(itr.read()), *color_++)));
      ++itr;
    }
  }

 private:
  const ColorMode &color_mode_;
  const Color *color_;
};

// Filler template constract is similar to the writer template contract, except
// that filler uses a single color.

// ReplaceFiller is a Filler that will overwrite previous content of the buffer
// using the specified color.

// For sub-byte color modes.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
class ReplaceFiller {
 public:
  ReplaceFiller(const ColorMode &color_mode, Color color)
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
  const ColorMode &color_mode_;
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
class ReplaceFiller<ColorMode, pixel_order, byte_order, 1, storage_type> {
 public:
  ReplaceFiller(const ColorMode &color_mode, Color color)
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

// BlendFiller is a Filler that will alpha-blend the specified color over
// previous content.

// For sub-byte color modes.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte = ColorTraits<ColorMode>::pixels_per_byte,
          typename storage_type = ColorStorageType<ColorMode>>
class BlendFiller {
 public:
  BlendFiller(const ColorMode &color_mode, Color color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    Color bg = color_mode_.toArgbColor(
        subpixel.ReadSubPixelColor(*target, pixel_index));
    subpixel.applySubPixelColor(
        color_mode_.fromArgbColor(alphaBlend(bg, color_)), target, pixel_index);
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) {
    SubPixelColorHelper<ColorMode, pixel_order> subpixel;
    int pixel_index = offset % pixels_per_byte;
    uint8_t *target = p + offset / pixels_per_byte;
    while (count-- > 0) {
      Color bg = color_mode_.toArgbColor(
          subpixel.ReadSubPixelColor(*target, pixel_index));
      subpixel.applySubPixelColor(
          color_mode_.fromArgbColor(alphaBlend(bg, color_)), target,
          pixel_index);
      if (++pixel_index == pixels_per_byte) {
        pixel_index = 0;
        target++;
      }
    }
  }

 private:
  const ColorMode &color_mode_;
  Color color_;
};

// For color modes in which a pixel takes up at least 1 byte.
template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          typename storage_type>
class BlendFiller<ColorMode, pixel_order, byte_order, 1, storage_type> {
 public:
  BlendFiller(const ColorMode &color_mode, Color color)
      : color_mode_(color_mode), color_(color) {}

  void operator()(uint8_t *p, uint32_t offset) const {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    itr.write(color_mode_.fromArgbColor(
        alphaBlend(color_mode_.toArgbColor(itr.read()), color_)));
  }

  void operator()(uint8_t *p, uint32_t offset, uint32_t count) const {
    internal::RawIterator<ColorMode::bits_per_pixel, byte_order> itr(p, offset);
    while (count-- > 0) {
      itr.write(color_mode_.fromArgbColor(
          alphaBlend(color_mode_.toArgbColor(itr.read()), color_)));
      ++itr;
    }
  }

 private:
  const ColorMode &color_mode_;
  Color color_;
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

}  // namespace internal

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void Offscreen<ColorMode, pixel_order, byte_order, pixels_per_byte,
               storage_type>::orientationUpdated() {
  orienter_.setOrientation(orientation());
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void Offscreen<ColorMode, pixel_order, byte_order, pixels_per_byte,
               storage_type>::setAddress(uint16_t x0, uint16_t y0, uint16_t x1,
                                         uint16_t y1) {
  window_.setAddress(x0, y0, x1, y1, raw_width(), raw_height(),
                     orienter_.orientation());
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void Offscreen<ColorMode, pixel_order, byte_order, pixels_per_byte,
               storage_type>::write(PaintMode mode, Color *color,
                                    uint32_t pixel_count) {
  switch (mode) {
    case PAINT_MODE_REPLACE: {
      internal::ReplaceWriter<ColorMode, pixel_order, byte_order> writer(
          color_mode(), color);
      writeToWindow(writer, pixel_count);
      break;
    }
    case PAINT_MODE_BLEND: {
      internal::BlendWriter<ColorMode, pixel_order, byte_order> writer(
          color_mode(), color);
      writeToWindow(writer, pixel_count);
      break;
    }
    default:
      break;
  }
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void Offscreen<ColorMode, pixel_order, byte_order, pixels_per_byte,
               storage_type>::writePixels(PaintMode mode, Color *color,
                                          int16_t *x, int16_t *y,
                                          uint16_t pixel_count) {
  uint8_t *buffer = buffer_;
  int16_t w = raw_width();
  orienter_.OrientPixels(x, y, pixel_count);
  switch (mode) {
    case PAINT_MODE_REPLACE: {
      internal::ReplaceWriter<ColorMode, pixel_order, byte_order> write(
          color_mode(), color);
      while (pixel_count-- > 0) {
        write(buffer, *x++ + *y++ * w);
      }
      break;
    }
    case PAINT_MODE_BLEND: {
      internal::BlendWriter<ColorMode, pixel_order, byte_order> write(
          color_mode(), color);
      while (pixel_count-- > 0) {
        write(buffer, *x++ + *y++ * w);
      }
      break;
    }
    default:
      break;
  }
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void Offscreen<ColorMode, pixel_order, byte_order, pixels_per_byte,
               storage_type>::fillPixels(PaintMode mode, Color color,
                                         int16_t *x, int16_t *y,
                                         uint16_t pixel_count) {
  uint8_t *buffer = buffer_;
  int16_t w = raw_width();
  orienter_.OrientPixels(x, y, pixel_count);
  switch (mode) {
    case PAINT_MODE_REPLACE: {
      internal::ReplaceFiller<ColorMode, pixel_order, byte_order> write(
          color_mode(), color);
      while (pixel_count-- > 0) {
        write(buffer, *x++ + *y++ * w);
      }
      break;
    }
    case PAINT_MODE_BLEND: {
      internal::BlendFiller<ColorMode, pixel_order, byte_order> write(
          color_mode(), color);
      while (pixel_count-- > 0) {
        write(buffer, *x++ + *y++ * w);
      }
      break;
    }
    default:
      break;
  }
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void Offscreen<ColorMode, pixel_order, byte_order, pixels_per_byte,
               storage_type>::writeRects(PaintMode mode, Color *color,
                                         int16_t *x0, int16_t *y0, int16_t *x1,
                                         int16_t *y1, uint16_t count) {
  orienter_.OrientRects(x0, y0, x1, y1, count);
  while (count-- > 0) {
    fillRectsAbsolute(mode, *color++, x0++, y0++, x1++, y1++, 1);
  }
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void Offscreen<ColorMode, pixel_order, byte_order, pixels_per_byte,
               storage_type>::fillRects(PaintMode mode, Color color,
                                        int16_t *x0, int16_t *y0, int16_t *x1,
                                        int16_t *y1, uint16_t count) {
  orienter_.OrientRects(x0, y0, x1, y1, count);
  if (y0 == y1) {
    fillHlinesAbsolute(mode, color, x0, y0, x1, count);
  } else if (x0 == x1) {
    fillVlinesAbsolute(mode, color, x0, y0, y1, count);
  } else {
    fillRectsAbsolute(mode, color, x0, y0, x1, y1, count);
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

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void Offscreen<ColorMode, pixel_order, byte_order, pixels_per_byte,
               storage_type>::fillRectsAbsolute(PaintMode mode, Color color,
                                                int16_t *x0, int16_t *y0,
                                                int16_t *x1, int16_t *y1,
                                                uint16_t count) {
  if (y0 == y1) {
    fillHlinesAbsolute(mode, color, x0, y0, x1, count);
  } else if (x0 == x1) {
    fillVlinesAbsolute(mode, color, x0, y0, y1, count);
  } else {
    switch (mode) {
      case PAINT_MODE_REPLACE: {
        internal::ReplaceFiller<ColorMode, pixel_order, byte_order> fill(
            color_mode(), color);
        fillRectsAbsoluteImpl(fill, buffer_, raw_width(), x0, y0, x1, y1,
                              count);
        break;
      }
      case PAINT_MODE_BLEND: {
        internal::BlendFiller<ColorMode, pixel_order, byte_order> fill(
            color_mode(), color);
        fillRectsAbsoluteImpl(fill, buffer_, raw_width(), x0, y0, x1, y1,
                              count);
      }
      default:
        break;
    }
  }
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void Offscreen<ColorMode, pixel_order, byte_order, pixels_per_byte,
               storage_type>::fillHlinesAbsolute(PaintMode mode, Color color,
                                                 int16_t *x0, int16_t *y0,
                                                 int16_t *x1, uint16_t count) {
  switch (mode) {
    case PAINT_MODE_REPLACE: {
      internal::ReplaceFiller<ColorMode, pixel_order, byte_order> fill(
          color_mode(), color);
      fillHlinesAbsoluteImpl(fill, buffer_, raw_width(), x0, y0, x1, count);
      break;
    }
    case PAINT_MODE_BLEND: {
      internal::BlendFiller<ColorMode, pixel_order, byte_order> fill(
          color_mode(), color);
      fillHlinesAbsoluteImpl(fill, buffer_, raw_width(), x0, y0, x1, count);
      break;
    }
    default:
      break;
  }
}

template <typename ColorMode, ColorPixelOrder pixel_order, ByteOrder byte_order,
          int8_t pixels_per_byte, typename storage_type>
void Offscreen<ColorMode, pixel_order, byte_order, pixels_per_byte,
               storage_type>::fillVlinesAbsolute(PaintMode mode, Color color,
                                                 int16_t *x0, int16_t *y0,
                                                 int16_t *y1, uint16_t count) {
  switch (mode) {
    case PAINT_MODE_REPLACE: {
      internal::ReplaceFiller<ColorMode, pixel_order, byte_order> fill(
          color_mode(), color);
      fillVlinesAbsoluteImpl(fill, buffer_, raw_width(), x0, y0, y1, count);
      break;
    }
    case PAINT_MODE_BLEND: {
      internal::BlendFiller<ColorMode, pixel_order, byte_order> fill(
          color_mode(), color);
      fillVlinesAbsoluteImpl(fill, buffer_, raw_width(), x0, y0, y1, count);
      break;
    }
    default:
      break;
  }
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