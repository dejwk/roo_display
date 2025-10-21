#pragma once

#include <functional>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"
#include "roo_display/filter/background.h"
#include "roo_display/filter/clip_mask.h"
#include "roo_display/filter/front_to_back_writer.h"
#include "roo_display/filter/transformation.h"
#include "roo_display/products/combo_device.h"
#include "roo_display/touch/calibration.h"
#include "roo_display/ui/alignment.h"

namespace roo_display {

class FrontToBackWriter;

class TouchDisplay {
 public:
  TouchDisplay(DisplayDevice& display_device, TouchDevice& touch_device,
               TouchCalibration touch_calibration = TouchCalibration())
      : display_device_(display_device),
        touch_device_(touch_device),
        touch_calibration_(touch_calibration) {}

  void init() { touch_device_.initTouch(); }

  // Calibrated, and using the display coordinates.
  TouchResult getTouch(TouchPoint* points, int max_points);

  // Uncalibrated, and using the absolute coordinates: 0-4095.
  TouchResult getRawTouch(TouchPoint* points, int max_points) {
    return touch_device_.getTouch(points, max_points);
  }

  void setCalibration(TouchCalibration touch_calibration) {
    touch_calibration_ = touch_calibration;
  }

  const TouchCalibration& calibration() const { return touch_calibration_; }

 private:
  DisplayDevice& display_device_;
  TouchDevice& touch_device_;
  TouchCalibration touch_calibration_;

  int16_t raw_touch_x_;
  int16_t raw_touch_y_;
  int16_t raw_touch_z_;
};

class Display {
 public:
  Display(DisplayDevice& display_device)
      : Display(display_device, nullptr, TouchCalibration()) {}

  Display(DisplayDevice& display_device, TouchDevice& touch_device,
          TouchCalibration touch_calibration = TouchCalibration())
      : Display(display_device, &touch_device, touch_calibration) {}

  Display(ComboDevice& device)
      : Display(device.display(), device.touch(), device.touch_calibration()) {}

  int32_t area() const {
    return display_device_.raw_width() * display_device_.raw_height();
  }

  const Box& extents() const { return extents_; }

  int16_t width() const { return extents_.width(); }
  int16_t height() const { return extents_.height(); }

  // Initializes the device.
  void init() {
    display_device_.init();
    touch_.init();
  }

  // Initializes the device, fills screen with the specified color, and sets
  // that color as the defaul background hint.
  void init(Color bgcolor);

  // Sets the orientation of the display. Setting the orientation resets
  // the clip box to the maximum display area.
  void setOrientation(Orientation orientation);

  Orientation orientation() const { return orientation_; }

  const TouchCalibration& touchCalibration() const {
    return touch_.calibration();
  }

  DisplayOutput& output() { return display_device_; }
  const DisplayOutput& output() const { return display_device_; }

  // If touch has not been registered, returns {.touch_points = 0 } and does not
  // modify `points'. If k touch points have been registered, sets max(k,
  // max_points) `points`, and returns {.touch_points = k}. In both cases,
  // returned timestamp_us specifies the micros() corresponding to the
  // detection time.
  TouchResult getTouch(TouchPoint* points, int max_points) {
    return touch_.getTouch(points, max_points);
  }

  // As above, but results use absolute coordinates (0-4095) and ignore
  // touch calibration settings.
  TouchResult getRawTouch(TouchPoint* points, int max_points) {
    return touch_.getRawTouch(points, max_points);
  }

  // If touched, returns true and sets the touch (x, y) coordinates (in device
  // coordinates). If not touched, returns false and does not modify (x, y).
  bool getTouch(int16_t& x, int16_t& y);

  // Resets the clip box to the maximum device-allowed values.
  void resetExtents() {
    extents_ = Box(0, 0, display_device_.effective_width() - 1,
                   display_device_.effective_height() - 1);
  }

  // Sets a default clip box, inherited by all derived contexts.
  void setExtents(const Box& extents) {
    resetExtents();
    extents_ = Box::Intersect(extents_, extents);
  }

  // Sets a rasterizable background to be used by all derived contexts.
  void setBackground(const Rasterizable* bg) {
    background_ = bg;
    bgcolor_ = color::Transparent;
  }

  void setTouchCalibration(TouchCalibration touch_calibration) {
    touch_.setCalibration(touch_calibration);
  }

  const Rasterizable* getRasterizableBackground() const { return background_; }

  // Sets a background color to be used by all derived contexts.
  // Initially set to color::Transparent.
  void setBackgroundColor(Color bgcolor) {
    bgcolor_ = bgcolor;
    display_device_.setBgColorHint(bgcolor);
  }

  Color getBackgroundColor() const { return bgcolor_; }

  // Clears the display, respecting the clip box, and background settings.
  void clear();

 private:
  Display(DisplayDevice& display_device, TouchDevice* touch_device,
          TouchCalibration touch_calibration);

  friend class DrawingContext;

  void nest() {
    if (nest_level_++ == 0) {
      display_device_.begin();
    }
  }

  void unnest() {
    if (--nest_level_ == 0) {
      display_device_.end();
    }
  }

  int16_t dx() const { return 0; }
  int16_t dy() const { return 0; }
  bool is_write_once() const { return false; }
  FillMode fill_mode() const { return FILL_MODE_VISIBLE; }
  BlendingMode blending_mode() const { return BLENDING_MODE_SOURCE_OVER; }

  DisplayDevice& display_device_;
  TouchDisplay touch_;
  int16_t nest_level_;
  Orientation orientation_;

  Box extents_;
  Color bgcolor_;
  const Rasterizable* background_;
};

// Primary top-level interface for drawing to screens, off-screen buffers,
// or other devices. Supports rectangular clip regions, as well as arbitrary
// clip masks.
//
// Able to render two types of objects:
// * Objects implementing the Drawable interface. These are capable of
//   drawing themselves to a device.
// * Objects implementing the template contract of a 'streamable'. These are
//   capable of generating a stream of colors, representing pixels corresponding
//   to a rectangular shape, row-order, top-to-bottom and left-to-right.
//   (Non-rectangular shapes can be provided by using transparency as a color.)
//
// The streamable contract is only implemented by some specific but important
// types of drawable objects, such as images. The streamables have one important
// benefit, which is the sole motivation for the existence of this interface:
// two (or more) streamables, which may be partially overlapping, and which may
// have transparency, can be rendered together to an underlying device, alpha-
// blended when necessary, such that each pixel is only drawn once (i.e. no
// flicker), but without using any additional memory buffers. An important use
// case is rendering of kern pairs of font glyphs (e.g. AV) without using extra
// buffers and yet without any flicker, which normally happens if glyphs are
// drawn one after another.
//
// Note: streamable is a template contract, rather than a virtual method
// contract, so that the code for overlaying the images can be optimized by the
// compiler, and tuned to the specific types. It yields the best possible
// performance, although it has a drawback of increasing program size. TODO: if
// program sizes turn out to be a problem, it may be worth exploring virtual
// version of this contract, using vector APIs (i.e., returning a large, e.g.
// O(100), number of pixels from a single virtual call) which may be a good
// balance between performance and program size.
class DrawingContext {
 public:
  template <typename Display>
  DrawingContext(Display& display)
      : DrawingContext(display, display.extents()) {}

  template <typename Display>
  DrawingContext(Display& display, Box bounds)
      : output_(display.output()),
        dx_(display.dx()),
        dy_(display.dy()),
        bounds_(bounds),
        max_clip_box_(Box::Intersect(bounds, display.extents())),
        clip_box_(max_clip_box_),
        unnest_([&display]() { display.unnest(); }),
        write_once_(display.is_write_once()),
        fill_mode_(display.fill_mode()),
        blending_mode_(display.blending_mode()),
        clip_mask_(nullptr),
        background_(display.getRasterizableBackground()),
        bgcolor_(display.getBackgroundColor()),
        transformed_(false),
        transformation_() {
    display.nest();
  }

  ~DrawingContext();

  const Box& bounds() const { return bounds_; }

  void setBackground(const Rasterizable* bg) { background_ = bg; }
  const Rasterizable* getBackground() const { return background_; }

  Color getBackgroundColor() const { return bgcolor_; }
  void setBackgroundColor(Color bgcolor) { bgcolor_ = bgcolor; }

  FillMode fillMode() const { return fill_mode_; }
  void setFillMode(FillMode fill_mode) { fill_mode_ = fill_mode; }

  BlendingMode blendingMode() const { return blending_mode_; }
  void setBlendingMode(BlendingMode blending_mode) {
    blending_mode_ = blending_mode;
  }

  // Clears the display, respecting the clip box, and background settings.
  void clear();

  // Fills the display with the specified color, respecting the clip box.
  void fill(Color color);

  void setClipBox(const Box& clip_box) {
    clip_box_ = Box::Intersect(clip_box, max_clip_box_);
  }

  void setClipBox(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    setClipBox(Box(x0, y0, x1, y1));
  }

  void setClipMask(const ClipMask* clip_mask) { clip_mask_ = clip_mask; }

  const Box& getClipBox() const { return clip_box_; }

  // void applyTransformation(Transformation t) {
  //   transformation_ = transformation_.transform(t);
  // }

  void setTransformation(Transformation t) {
    transformation_ = t;
    transformed_ = (t.xy_swap() || t.is_rescaled() || t.is_translated());
  }

  const Transformation& transformation() const { return transformation_; }

  void setWriteOnce();

  bool isWriteOnce() const { return write_once_; }

  void drawPixels(const std::function<void(ClippingBufferedPixelWriter&)>& fn,
                  BlendingMode blending_mode = BLENDING_MODE_SOURCE_OVER);

  // Draws the object using its inherent coordinates. The point (0, 0) in the
  // object's coordinates maps to (0, 0) in the context's coordinates.
  inline void draw(const Drawable& object) {
    drawInternal(object, 0, 0, bgcolor_);
  }

  // Draws the object using the specified absolute offset. The point (0, 0) in
  // the object's coordinates maps to (dx, dy) in the context's coordinates.
  inline void draw(const Drawable& object, int16_t dx, int16_t dy) {
    drawInternal(object, dx, dy, bgcolor_);
  }

  // Draws the object applying the specified alignment, relative to the
  // bounds(). For example, for with kMiddle | kCenter, the object will be
  // centered relative to the bounds().
  void draw(const Drawable& object, Alignment alignment) {
    Box anchorExtents = object.anchorExtents();
    if (transformed_) {
      anchorExtents = transformation_.transformBox(anchorExtents);
    }
    Offset offset = alignment.resolveOffset(bounds(), anchorExtents);
    drawInternal(object, offset.dx, offset.dy, bgcolor_);
  }

  // Analogous to draw(object), but instead of drawing, replaces all the output
  // pixels with the background color.
  void erase(const Drawable& object);

  // Analogous to draw(object, dx, dy), but instead of drawing, replaces all the
  // output pixels with the background color.
  void erase(const Drawable& object, int16_t dx, int16_t dy);

  // Analogous to draw(object, alignment), but instead of drawing, replaces all
  // the output pixels with the background color.
  void erase(const Drawable& object, Alignment alignment);

 private:
  DisplayOutput& output() { return output_; }
  const DisplayOutput& output() const { return output_; }

  void drawInternal(const Drawable& object, int16_t dx, int16_t dy,
                    Color bgcolor);

  void drawInternalWithBackground(Surface& s, const Drawable& object);

  void drawInternalTransformed(Surface& s, const Drawable& object);

  DisplayOutput& output_;

  // Offset of the origin in the output coordinates. Empty Transformation maps
  // (0, 0) in drawing coordinates onto (dx_, dy_) in device coordinates.
  int16_t dx_;
  int16_t dy_;

  // Bounds used for alignment. Default to device extents. If different than
  // device extents, they both constrain the initial and max clip box.
  Box bounds_;

  // The maximum allowed clip box. setClipBox will intersect its argument
  // with it. Equal to the intersection of bounds() and the original device
  // extents.
  const Box max_clip_box_;

  // Absolute coordinates of the clip region in the device space. Inclusive.
  Box clip_box_;

  std::function<void()> unnest_;

  bool write_once_;
  std::unique_ptr<FrontToBackWriter> front_to_back_writer_;

  FillMode fill_mode_;
  BlendingMode blending_mode_;

  const ClipMask* clip_mask_;
  const Rasterizable* background_;
  Color bgcolor_;
  bool transformed_;
  Transformation transformation_;
};

// Fill is an 'infinite' single-color area. When drawn, it will fill the
// entire clip box with the given color.
class Fill : public Rasterizable {
 public:
  Fill(Color color) : color_(color) {}

  Box extents() const override { return Box::MaximumBox(); }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override;

  bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                     Color* result) const override;

 private:
  void drawTo(const Surface& s) const override;

  Color color_;
};

// Clear is an 'infinite' transparent area. When drawn, it will fill the
// entire clip box with the color implied by background settings.
class Clear : public Rasterizable {
 public:
  Clear() {}

  Box extents() const override { return Box::MaximumBox(); }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override;

  bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                     Color* result) const override;

 private:
  void drawTo(const Surface& s) const override;
};

// Utility that can be used while a DrawingContext is active, to temporarily
// stop the underlying transport transaction, and release the bus (e.g. SPI). It
// allows custom drawables to perform operations on the same bus (e.g., read
// date from an SD card). When an instance of this class is alive, and unless
// ResumeOutput is also used, DrawingContexts do not function, and no drawing
// operations should be attempted.
//
// See jpeg.cpp and png.cpp for an application example.
class PauseOutput {
 public:
  PauseOutput(DisplayOutput& out) : out_(out) { out_.end(); }
  ~PauseOutput() { out_.begin(); }

 private:
  DisplayOutput& out_;
};

// The inverse of 'PauseOutput', above. It allows to resume drawing that was
// paused by PauseOutput.
class ResumeOutput {
 public:
  ResumeOutput(DisplayOutput& out) : out_(out) { out_.begin(); }
  ~ResumeOutput() { out_.end(); }

 private:
  DisplayOutput& out_;
};

}  // namespace roo_display
