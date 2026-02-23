#pragma once

#include <inttypes.h>

#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_display/color/pixel_order.h"
#include "roo_display/core/box.h"
#include "roo_display/core/orientation.h"
#include "roo_time.h"

namespace roo_display {

/// The abstraction for drawing to a display.
class DisplayOutput {
 public:
  class ColorFormat;

  virtual ~DisplayOutput() {}

  /// Enter a write transaction.
  ///
  /// Normally called by the `DrawingContext` constructor, and does not need to
  /// be called explicitly.
  virtual void begin() {}

  /// Finalize the previously entered write transaction, flushing any pending
  /// writes.
  ///
  /// Normally called by the `DrawingContext` destructor, and does not need to
  /// be called explicitly.
  virtual void end() {}

  /// Convenience overload for `setAddress()` using a `Box`.
  ///
  /// @param bounds The target rectangle.
  /// @param blending_mode Blending mode for subsequent writes.
  void setAddress(const Box &bounds, BlendingMode blending_mode) {
    setAddress(bounds.xMin(), bounds.yMin(), bounds.xMax(), bounds.yMax(),
               blending_mode);
  }

  /// Set a rectangular window filled by subsequent calls to `write()`.
  ///
  /// @param x0 Left coordinate (inclusive).
  /// @param y0 Top coordinate (inclusive).
  /// @param x1 Right coordinate (inclusive).
  /// @param y1 Bottom coordinate (inclusive).
  /// @param blending_mode Blending mode for subsequent writes.
  virtual void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                          BlendingMode blending_mode) = 0;

  /// Write pixels into the current address window.
  ///
  /// The address must be set via `setAddress()` and not invalidated by any of
  /// the `write*` / `fill*` methods. Otherwise, the behavior is undefined.
  ///
  /// @param color Pointer to source pixels.
  /// @param pixel_count Number of pixels to write.
  virtual void write(Color *color, uint32_t pixel_count) = 0;

  /// Write `pixel_count` copies of the same color into the current address
  /// window.
  ///
  /// Default implementation falls back to chunked `write()`. Drivers and
  /// filters can override this for a more efficient single-color path.
  virtual void fill(Color color, uint32_t pixel_count);

  /// Draw the specified pixels (per-pixel colors). Invalidates the address
  /// window.
  ///
  /// @param blending_mode Blending mode used for drawing.
  /// @param color Pointer to colors for each pixel.
  /// @param x Pointer to x-coordinates for each pixel.
  /// @param y Pointer to y-coordinates for each pixel.
  /// @param pixel_count Number of pixels.
  virtual void writePixels(BlendingMode blending_mode, Color *color, int16_t *x,
                           int16_t *y, uint16_t pixel_count) = 0;

  /// Draw the specified pixels using the same color. Invalidates the address
  /// window.
  ///
  /// @param blending_mode Blending mode used for drawing.
  /// @param color The color to use for all pixels.
  /// @param x Pointer to x-coordinates for each pixel.
  /// @param y Pointer to y-coordinates for each pixel.
  /// @param pixel_count Number of pixels.
  virtual void fillPixels(BlendingMode blending_mode, Color color, int16_t *x,
                          int16_t *y, uint16_t pixel_count) = 0;

  /// Draw the specified rectangles (per-rectangle colors). Invalidates the
  /// address window.
  ///
  /// @param blending_mode Blending mode used for drawing.
  /// @param color Pointer to colors for each rectangle.
  /// @param x0 Pointer to left coordinates for each rectangle.
  /// @param y0 Pointer to top coordinates for each rectangle.
  /// @param x1 Pointer to right coordinates for each rectangle.
  /// @param y1 Pointer to bottom coordinates for each rectangle.
  /// @param count Number of rectangles.
  virtual void writeRects(BlendingMode blending_mode, Color *color, int16_t *x0,
                          int16_t *y0, int16_t *x1, int16_t *y1,
                          uint16_t count) = 0;

  /// Draw the specified rectangles using the same color. Invalidates the
  /// address window.
  ///
  /// @param blending_mode Blending mode used for drawing.
  /// @param color The color to use for all rectangles.
  /// @param x0 Pointer to left coordinates for each rectangle.
  /// @param y0 Pointer to top coordinates for each rectangle.
  /// @param x1 Pointer to right coordinates for each rectangle.
  /// @param y1 Pointer to bottom coordinates for each rectangle.
  /// @param count Number of rectangles.
  virtual void fillRects(BlendingMode blending_mode, Color color, int16_t *x0,
                         int16_t *y0, int16_t *x1, int16_t *y1,
                         uint16_t count) = 0;

  /// Fill a single rectangle. Invalidates the address window.
  ///
  /// @param blending_mode Blending mode used for drawing.
  /// @param rect The rectangle to fill.
  /// @param color The fill color.
  inline void fillRect(BlendingMode blending_mode, const Box &rect,
                       Color color) {
    int16_t x0 = rect.xMin();
    int16_t y0 = rect.yMin();
    int16_t x1 = rect.xMax();
    int16_t y1 = rect.yMax();

    fillRects(blending_mode, color, &x0, &y0, &x1, &y1, 1);
  }

  // // Convenience method to fill a single rectangle using REPLACE mode.
  // // Invalidates the address window.
  // inline void fillRect(const Box &rect, Color color) {
  //   fillRect(kBlendingSource, rect, color);
  // }

  /// Fill a single rectangle. Invalidates the address window.
  ///
  /// @param blending_mode Blending mode used for drawing.
  /// @param x0 Left coordinate (inclusive).
  /// @param y0 Top coordinate (inclusive).
  /// @param x1 Right coordinate (inclusive).
  /// @param y1 Bottom coordinate (inclusive).
  /// @param color The fill color.
  inline void fillRect(BlendingMode blending_mode, int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1, Color color) {
    fillRects(blending_mode, color, &x0, &y0, &x1, &y1, 1);
  }

  /// Fill a single rectangle using `kBlendingSource`.
  ///
  /// Invalidates the address window.
  ///
  /// @param x0 Left coordinate (inclusive).
  /// @param y0 Top coordinate (inclusive).
  /// @param x1 Right coordinate (inclusive).
  /// @param y1 Bottom coordinate (inclusive).
  /// @param color The fill color.
  inline void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       Color color) {
    fillRects(kBlendingSource, color, &x0, &y0, &x1, &y1, 1);
  }

  /// Return the native color format used by this device for direct drawing.
  virtual const ColorFormat &getColorFormat() const = 0;

  /// Draw a rectangle represented in the device's native color format.
  ///
  /// Source data pointer correspond to the (0, 0) point in the source
  /// coordinate system, with stride of `row_width_bytes`. The source rectangle
  /// at (src_x0, src_y0, src_x1, src_y1) relative to the data pointer, gets
  /// copied to the destination rectangle with top-left corner at `(dst_x0,
  /// dst_y0)`. The caller must ensure that the destination rectangle fits
  /// within the device's bounds.
  ///
  /// The default implementation processes the rectangle by small tiles,
  /// converting the data to an array of regular colors, and calling regular
  /// window functions to draw them. Specific devices can override this method
  /// to provide a more efficient implementation that draws directly from the
  /// source data.
  virtual void drawDirectRect(const roo::byte *data, size_t row_width_bytes,
                              int16_t src_x0, int16_t src_y0, int16_t src_x1,
                              int16_t src_y1, int16_t dst_x0, int16_t dst_y0);
};

/// Base class for display device drivers.
///
/// In addition to the drawing methods, exposes initialization and configuration
/// APIs for the display.
///
/// The access pattern supports a notion of a transaction (which may, but does
/// not have to, map to the underlying hardware transaction). The transaction
/// begins with a call to `begin()` and ends with a call to `end()`. Calls to
/// `DisplayOutput` methods are allowed only within the transaction. The driver
/// may defer writes, but they must be flushed on `end()`.
class DisplayDevice : public DisplayOutput {
 public:
  virtual ~DisplayDevice() {}

  /// Initialize the display driver.
  ///
  /// Must be called once before any `DisplayOutput` methods are used. Can be
  /// called later to re-initialize the display to default settings. Must be
  /// called outside a transaction.
  virtual void init() {}

  /// Set the orientation of the display.
  void setOrientation(Orientation orientation) {
    if (orientation_ != orientation) {
      orientation_ = orientation;
      orientationUpdated();
    }
  }

  /// Invoked when `orientation()` is updated.
  ///
  /// Devices that support orientation change should override this method.
  virtual void orientationUpdated() {
    assert(orientation_ == Orientation::Default());
  }

  /// Return the current orientation of the display.
  Orientation orientation() const { return orientation_; }

  /// Return the width of the display in its native orientation.
  ///
  /// This value does not depend on the current orientation, and is stable even
  /// before `init()` is called.
  int16_t raw_width() const { return raw_width_; }

  /// Return the height of the display in its native orientation.
  ///
  /// This value does not depend on the current orientation, and is stable even
  /// before `init()` is called.
  int16_t raw_height() const { return raw_height_; }

  /// Return the display width in the current orientation.
  int16_t effective_width() const {
    return orientation().isXYswapped() ? raw_height() : raw_width();
  }

  /// Return the display height in the current orientation.
  int16_t effective_height() const {
    return orientation().isXYswapped() ? raw_width() : raw_height();
  }

  /// Provide a background color hint for source-over blending.
  ///
  /// Devices that support `kBlendingSourceOver` should ignore this hint.
  /// Devices that do not support alpha blending (e.g. hardware can't read the
  /// framebuffer) should use this color as the assumed background for writes
  /// using `kBlendingSourceOver`.
  virtual void setBgColorHint(Color bgcolor) {}

 protected:
  DisplayDevice(int16_t raw_width, int16_t raw_height)
      : DisplayDevice(Orientation::Default(), raw_width, raw_height) {}

  DisplayDevice(Orientation orientation, int16_t raw_width, int16_t raw_height)
      : orientation_(orientation),
        raw_width_(raw_width),
        raw_height_(raw_height) {}

 private:
  Orientation orientation_;
  int16_t raw_width_;
  int16_t raw_height_;
};

class DisplayOutput::ColorFormat {
 public:
  enum Mode {
    kModeArgb8888,
    kModeRgba8888,
    kModeRgb888,
    kModeArgb6666,
    kModeArgb4444,
    kModeRgb565,
    kModeGrayscale8,
    kModeGrayAlpha8,
    kModeGrayscale4,
    kModeAlpha8,
    kModeAlpha4,
    kModeMonochrome,
    kModeIndexed1,
    kModeIndexed2,
    kModeIndexed4,
    kModeIndexed8,
  };

  /// Decodes a sub-rectangle of data, converting it to an array of colors.
  ///
  /// If the format has 1+ byte per pixel, then an (x, y) coordinate maps to the
  /// byte at `data + y * row_width_bytes + x * bytes_per_pixel`.
  ///
  /// If the format is packed with multiple pixels per byte, then an (x, y)
  /// coordinate maps to the byte at `data + y * row_width_bytes + (x /
  /// pixels_per_byte)`, and the relevant pixel within that byte is determined
  /// by the pixel order.
  ///
  /// @param data Pointer to the raw pixel data.
  /// @param row_width_bytes The byte width of a single row in `data`.
  /// @param x0 Left coordinate of the rectangle.
  /// @param y0 Top coordinate of the rectangle.
  /// @param x1 Right coordinate of the rectangle.
  /// @param y1 Bottom coordinate of the rectangle.
  /// @param output Output buffer for the decoded colors. Must have capacity
  ///               of at least `(x1 - x0 + 1) * (y1 - y0 + 1)`.
  virtual void decode(const roo::byte *data, size_t row_width_bytes, int16_t x0,
                      int16_t y0, int16_t x1, int16_t y1,
                      Color *output) const = 0;

  /// Returns true if all pixels in the specified rectangle have the same raw
  /// representation in this format.
  ///
  /// If true, writes the corresponding decoded color to `output`.
  virtual bool decodeIfUniform(const roo::byte *data, size_t row_width_bytes,
                               int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                               Color *output) const = 0;

  Mode mode() const { return mode_; }

  roo_io::ByteOrder byte_order() const { return byte_order_; }

  ColorPixelOrder pixel_order() const { return pixel_order_; }

  TransparencyMode transparency() const { return transparency_; }

 protected:
  ColorFormat(Mode mode, roo_io::ByteOrder byte_order,
              ColorPixelOrder pixel_order = COLOR_PIXEL_ORDER_MSB_FIRST)
      : mode_(mode),
        byte_order_(byte_order),
        pixel_order_(pixel_order),
        transparency_(CalculateTransparency(mode)) {}

  virtual ~ColorFormat() {}

 private:
  static TransparencyMode CalculateTransparency(Mode mode) {
    switch (mode) {
      case kModeArgb8888:
      case kModeRgba8888:
      case kModeArgb6666:
      case kModeArgb4444:
      case kModeGrayAlpha8:
      case kModeAlpha8:
      case kModeAlpha4:
        return kTransparency;
      default:
        return kNoTransparency;
    }
  }

  Mode mode_;
  roo_io::ByteOrder byte_order_;
  ColorPixelOrder pixel_order_;
  TransparencyMode transparency_;
};

/// A single touch point returned by a touch controller.
struct TouchPoint {
  TouchPoint() : id(0), x(-1), y(-1), z(-1), vx(0), vy(0) {}

  /// ID assigned by the driver, for tracking. For non-multitouch devices, this
  /// should be left at zero.
  uint16_t id;

  /// X-coordinate of the touch.
  int16_t x;

  /// Y-coordinate of the touch.
  int16_t y;

  /// Z-coordinate of the touch (pressure). Range [0-4095]. Drivers that don't
  /// support it should report 1024.
  int16_t z;

  /// X-axis velocity at the touch point, in pixels/sec.
  int32_t vx;

  /// Y-axis velocity at the touch point, in pixels/sec.
  int32_t vy;
};

/// Metadata for a touch sampling result.
struct TouchResult {
  TouchResult() : timestamp(roo_time::Uptime::Start()), touch_points(0) {}

  TouchResult(roo_time::Uptime timestamp, int touch_points)
      : timestamp(timestamp), touch_points(touch_points) {}

  /// Detection time.
  roo_time::Uptime timestamp;

  int touch_points;
};

/// Touch controller interface.
class TouchDevice {
 public:
  virtual ~TouchDevice() = default;

  /// Initialize the touch controller.
  virtual void initTouch() {}

  /// Read the current touch state.
  ///
  /// If no touch is registered, returns `{.touch_points = 0}` and does not
  /// modify `points`. If $k$ touch points are registered, writes
  /// `min(k, max_points)` entries to `points`, and returns
  /// `{.touch_points = k}`. In both cases, the returned timestamp is the
  /// detection time.
  ///
  /// @param points Output buffer for touch points.
  /// @param max_points Capacity of `points`.
  /// @return Touch result metadata.
  virtual TouchResult getTouch(TouchPoint *points, int max_points) = 0;
};

}  // namespace roo_display
