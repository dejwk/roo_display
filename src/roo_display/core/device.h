#pragma once

#include <inttypes.h>

#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_display/core/box.h"
#include "roo_display/core/orientation.h"
#include "roo_time.h"

namespace roo_display {

// The abstraction for drawing to a display.
class DisplayOutput {
 public:
  virtual ~DisplayOutput() {}

  // Enter a 'write transaction'.
  //
  // Normally called by the DrawingContext constructor, and does not need to be
  // called explicitly.
  virtual void begin() {}

  // Finalize the previously entered 'write transaction', flushing
  // all potentially pending writes.
  //
  // Normally called by the DrawingContext destructor, and does not need to be
  // called explicitly.
  virtual void end() {}

  // Convenience shortcut.
  void setAddress(const Box &bounds, BlendingMode blending_mode) {
    setAddress(bounds.xMin(), bounds.yMin(), bounds.xMax(), bounds.yMax(),
               blending_mode);
  }

  // Set a rectangular window that will be filled by subsequent calls to
  // write(), using the specified paint mode.
  virtual void setAddress(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                          BlendingMode blending_mode) = 0;

  // Writes to the subsequent pixels in the address window. The address must
  // have been previously set using setAddress, and not invalidated by calling
  // one of the 'write*' and 'fill*' methods below. Otherwise, the behavior
  // is undefined.
  virtual void write(Color *color, uint32_t pixel_count) = 0;

  // virtual void fill(Color color, uint32_t pixel_count) = 0;

  // Draws the specified pixels. Invalidates the address window.
  virtual void writePixels(BlendingMode blending_mode, Color *color, int16_t *x,
                           int16_t *y, uint16_t pixel_count) = 0;

  // Draws the specified pixels, using the same color. Invalidates the address
  // window.
  virtual void fillPixels(BlendingMode blending_mode, Color color, int16_t *x,
                          int16_t *y, uint16_t pixel_count) = 0;

  // Draws the specified rectangles. Invalidates the address window.
  virtual void writeRects(BlendingMode blending_mode, Color *color, int16_t *x0,
                          int16_t *y0, int16_t *x1, int16_t *y1,
                          uint16_t count) = 0;

  // Draws the specified rectangles, using the same color. Invalidates the
  // address window.
  virtual void fillRects(BlendingMode blending_mode, Color color, int16_t *x0,
                         int16_t *y0, int16_t *x1, int16_t *y1,
                         uint16_t count) = 0;

  // Convenience method to fill a single rectangle. Invalidates the address
  // window.
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
  //   fillRect(BLENDING_MODE_SOURCE, rect, color);
  // }

  // Convenience method to fill a single rectangle.
  // Invalidates the address window.
  inline void fillRect(BlendingMode blending_mode, int16_t x0, int16_t y0,
                       int16_t x1, int16_t y1, Color color) {
    fillRects(blending_mode, color, &x0, &y0, &x1, &y1, 1);
  }

  // Convenience method to fill a single rectangle using REPLACE mode.
  // Invalidates the address window.
  inline void fillRect(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       Color color) {
    fillRects(BLENDING_MODE_SOURCE, color, &x0, &y0, &x1, &y1, 1);
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

  // Devices that support BLENDING_MODE_SOURCE_OVER should ignore this hint.
  // Devices that do not support alpha-blending, e.g. because the hardware
  // does not suport reading from the framebuffer, should take this hint
  // to use the specified color as the assumed background color for any
  // writes that use BLENDING_MODE_SOURCE_OVER.
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

struct TouchPoint {
  TouchPoint() : id(0), x(-1), y(-1), z(-1), vx(0), vy(0) {}

  // ID assigned by the driver, for tracking. For non-multitouch devices, should
  // be left at zero.
  uint16_t id;

  // X-coordinate of the touch.
  int16_t x;

  // Y-coordinate of the touch.
  int16_t y;

  // Z-coordinate of the touch (how hard is it pressed). [0-4095]. Drivers that
  // don't support it should report 1024.
  int16_t z;

  // X-axis velocity at touch point, in pixels / sec.
  int32_t vx;

  // Y-axis velocity at touch point, in pixels / sec.
  int32_t vy;
};

struct TouchResult {
  TouchResult() : timestamp(roo_time::Uptime::Start()), touch_points(0) {}

  TouchResult(roo_time::Uptime timestamp, int touch_points)
      : timestamp(timestamp), touch_points(touch_points) {}

  // Detection time.
  roo_time::Uptime timestamp;

  int touch_points;
};

class TouchDevice {
 public:
  virtual ~TouchDevice() = default;

  virtual void initTouch() {}

  // If touch has not been registered, returns {.touch_points = 0 } and does not
  // modify `points'. If k touch points have been registered, sets max(k,
  // max_points) `points`, and returns {.touch_points = k}. In both cases,
  // returned timestamp specifies the detection time.
  virtual TouchResult getTouch(TouchPoint *points, int max_points) = 0;
};

}  // namespace roo_display
