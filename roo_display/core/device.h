#pragma once

#include <inttypes.h>

#include "roo_display/core/box.h"
#include "roo_display/core/color.h"
#include "roo_display/core/orientation.h"

namespace roo_display {

// The abstraction for drawing to a display.
class DisplayOutput {
 public:
  virtual ~DisplayOutput() {}

  // Convenience shortcut.
  void setAddress(const Box &bounds, PaintMode mode) {
    setAddress(bounds.xMin(), bounds.yMin(), bounds.xMax(), bounds.yMax(),
               mode);
  }

  // Set a rectangular window that will be filled by subsequent calls to
  // write(), using the specified paint mode.
  virtual void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                          PaintMode mode) = 0;

  // Writes to the subsequent pixels in the address window. The address must
  // have been previously set using setAddress, and not invalidated by calling
  // one of the 'write*' and 'fill*' methods below. Otherwise, the behavior
  // is undefined.
  virtual void write(Color *color, uint32_t pixel_count) = 0;

  // virtual void fill(Color color, uint32_t pixel_count) = 0;

  // Draws the specified pixels. Invalidates the address window.
  virtual void writePixels(PaintMode mode, Color *color, int16_t *x, int16_t *y,
                           uint16_t pixel_count) = 0;

  // Draws the specified pixels, using the same color. Invalidates the address
  // window.
  virtual void fillPixels(PaintMode mode, Color color, int16_t *x, int16_t *y,
                          uint16_t pixel_count) = 0;

  // Draws the specified rectangles. Invalidates the address window.
  virtual void writeRects(PaintMode mode, Color *color, int16_t *x0,
                          int16_t *y0, int16_t *x1, int16_t *y1,
                          uint16_t count) = 0;

  // Draws the specified rectangles, using the same color. Invalidates the
  // address window.
  virtual void fillRects(PaintMode mode, Color color, int16_t *x0, int16_t *y0,
                         int16_t *x1, int16_t *y1, uint16_t count) = 0;

  // Convenience method to fill a single rectangle. Invalidates the address
  // window.
  inline void fillRect(PaintMode mode, const Box &rect, Color color) {
    int16_t x0 = rect.xMin();
    int16_t y0 = rect.yMin();
    int16_t x1 = rect.xMax();
    int16_t y1 = rect.yMax();

    fillRects(mode, color, &x0, &y0, &x1, &y1, 1);
  }

  // Convenience method to fill a single rectangle using REPLACE mode.
  // Invalidates the address window.
  inline void fillRect(const Box &rect, Color color) {
    fillRect(PAINT_MODE_REPLACE, rect, color);
  }

  // Convenience method to fill a single rectangle using REPLACE mode.
  // Invalidates the address window.
  inline void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       Color color) {
    fillRects(PAINT_MODE_REPLACE, color, &x0, &y0, &x1, &y1, 1);
  }
};

// Base class for display device drivers. In addition to the methods
// for drawing to the display, also exposes methods to initialize
// and configure the display.
//
// The access pattern supports a notion of a 'transaction' (which
// may, but does not have to, map to the underlying hardware
// transaction). The transaction begins with a call to begin(),
// and ends with a call to end(). Calls to DisplayOutput
// methods are allowed only within the 'transaction'
// (i.e., in between begin() and end()). The driver may defer
// writes, but they must be flushed on end().
class DisplayDevice : public DisplayOutput {
 public:
  virtual ~DisplayDevice() {}

  // Must be called once before any methods on the DisplayOutput interface
  // are called. Can be called at any time, subsequently, to re-initialize
  // the display to the default settings. Must be called outside transaction.
  virtual void init() {}

  // Enter a 'write transaction'.
  virtual void begin() {}

  // Finalize the previously entered 'write transaction', flushing
  // all potentially pending writes.
  virtual void end() {}

  // Sets the orientation of the display.
  void setOrientation(Orientation orientation) {
    if (orientation_ != orientation) {
      orientation_ = orientation;
      orientationUpdated();
    }
  }

  // Devices that support orientation change should override this method.
  virtual void orientationUpdated() {
    assert(orientation_ == Orientation::Default());
  }

  // Returns the orientation of the display.
  Orientation orientation() const { return orientation_; }

  // Returns the width of the display in its native orientation (i.e.,
  // independent of the orientation actually set on the display). Returns
  // the same value every time it is called, even if called
  // before init().
  int16_t raw_width() const { return raw_width_; }

  // Returns the height of the display in its native orientation (i.e.,
  // independent of the orientation actually set on the display). Returns
  // the same value every time it is called, even if called
  // before init().
  int16_t raw_height() const { return raw_height_; }

  // Returns the display width, considering the current orientation.
  int16_t effective_width() const {
    return orientation().isXYswapped() ? raw_height() : raw_width();
  }

  // Returns the display height, considering the current orientation.
  int16_t effective_height() const {
    return orientation().isXYswapped() ? raw_width() : raw_height();
  }

  // Devices that support PAINT_MODE_BLEND should ignore this hint.
  // Devices that do not support alpha-blending, e.g. because the hardware
  // does not suport reading from the framebuffer, should take this hint
  // to use the specified color as the assumed background color for any
  // writes that use PAINT_MODE_BLEND.
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

class TouchDevice {
 public:
  virtual ~TouchDevice() {}

  // If touch has been registered, returns true, and sets (x, y, z) to touch
  // coordinates, where (x, y) are in the range of 0 to 4095, and z is 'press
  // intensity' in the range of 0-255. If the touch has not been registered,
  // returns false without modifying x, y, or z.
  virtual bool getTouch(int16_t *x, int16_t *y, int16_t *z) = 0;
};

}  // namespace roo_display
