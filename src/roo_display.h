/// @file roo_display.h
/// @brief Public API surface for roo_display display, touch, and drawing
/// utilities.
#pragma once

#include <functional>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"
#include "roo_display/filter/background.h"
#include "roo_display/filter/background_fill_optimizer.h"
#include "roo_display/filter/clip_mask.h"
#include "roo_display/filter/front_to_back_writer.h"
#include "roo_display/filter/transformation.h"
#include "roo_display/products/combo_device.h"
#include "roo_display/touch/calibration.h"
#include "roo_display/ui/alignment.h"

namespace roo_display {

class FrontToBackWriter;

/// @brief Wrapper providing calibrated touch input for a display device.
class TouchDisplay {
 public:
  /// Constructs a touch display wrapper.
  TouchDisplay(DisplayDevice& display_device, TouchDevice& touch_device,
               TouchCalibration touch_calibration = TouchCalibration())
      : display_device_(display_device),
        touch_device_(touch_device),
        touch_calibration_(touch_calibration) {}

  /// Initializes the touch device.
  void init() { touch_device_.initTouch(); }

  /// Returns calibrated touch points in display coordinates.
  TouchResult getTouch(TouchPoint* points, int max_points);

  /// Returns raw touch points in absolute 0-4095 coordinates.
  TouchResult getRawTouch(TouchPoint* points, int max_points) {
    return touch_device_.getTouch(points, max_points);
  }

  /// Sets the touch calibration mapping.
  void setCalibration(TouchCalibration touch_calibration) {
    touch_calibration_ = touch_calibration;
  }

  /// Returns the current touch calibration.
  const TouchCalibration& calibration() const { return touch_calibration_; }

 private:
  DisplayDevice& display_device_;
  TouchDevice& touch_device_;
  TouchCalibration touch_calibration_;

  int16_t raw_touch_x_;
  int16_t raw_touch_y_;
  int16_t raw_touch_z_;
};

/// @brief Display facade that owns a display device and optional touch input.
class Display {
 public:
  /// Constructs a display without touch support.
  Display(DisplayDevice& display_device)
      : Display(display_device, nullptr, TouchCalibration()) {}

  /// Constructs a display with touch support and optional calibration.
  Display(DisplayDevice& display_device, TouchDevice& touch_device,
          TouchCalibration touch_calibration = TouchCalibration())
      : Display(display_device, &touch_device, touch_calibration) {}

  /// Constructs a display from a combo device.
  Display(ComboDevice& device)
      : Display(device.display(), device.touch(), device.touch_calibration()) {}

  /// Returns the total pixel area of the raw device.
  int32_t area() const {
    return display_device_.raw_width() * display_device_.raw_height();
  }

  /// Returns the current extents used by this display.
  const Box& extents() const { return extents_; }

  /// Returns the width in pixels of the current extents.
  int16_t width() const { return extents_.width(); }
  /// Returns the height in pixels of the current extents.
  int16_t height() const { return extents_.height(); }

  /// Initializes the display and touch devices.
  void init() {
    display_device_.init();
    touch_.init();
  }

  /// Initializes the device, fills the screen with the specified color, and
  /// sets that color as the default background hint.
  void init(Color bgcolor);

  /// Sets the display orientation. Resets the clip box to the max display area.
  void setOrientation(Orientation orientation);

  /// Returns the current orientation.
  Orientation orientation() const { return orientation_; }

  /// Returns the touch calibration for the display.
  const TouchCalibration& touchCalibration() const {
    return touch_.calibration();
  }

  /// Returns mutable access to the display output.
  DisplayOutput& output() { return *output_; }
  /// Returns const access to the display output.
  const DisplayOutput& output() const { return *output_; }

  /// Returns calibrated touch points in display coordinates.
  ///
  /// If no touch has been registered, returns {.touch_points = 0} and does not
  /// modify `points`. If k touch points have been registered, sets up to
  /// max_points entries in `points`, and returns {.touch_points = k}. In both
  /// cases, the returned timestamp specifies the detection time.
  TouchResult getTouch(TouchPoint* points, int max_points) {
    return touch_.getTouch(points, max_points);
  }

  /// Returns raw touch points in absolute coordinates (0-4095).
  TouchResult getRawTouch(TouchPoint* points, int max_points) {
    return touch_.getRawTouch(points, max_points);
  }

  /// Returns true and sets (x, y) if touched; otherwise returns false.
  bool getTouch(int16_t& x, int16_t& y);

  /// Resets the clip box to the maximum device-allowed values.
  void resetExtents() {
    extents_ = Box(0, 0, display_device_.effective_width() - 1,
                   display_device_.effective_height() - 1);
  }

  /// Sets a default clip box, inherited by derived contexts.
  void setExtents(const Box& extents) {
    resetExtents();
    extents_ = Box::Intersect(extents_, extents);
  }

  /// Sets a rasterizable background for all derived contexts.
  void setBackground(const Rasterizable* bg) {
    background_ = bg;
    bgcolor_ = color::Transparent;
  }

  /// Sets the touch calibration mapping.
  void setTouchCalibration(TouchCalibration touch_calibration) {
    touch_.setCalibration(touch_calibration);
  }

  /// Returns the rasterizable background for derived contexts.
  const Rasterizable* getRasterizableBackground() const { return background_; }

  /// Sets a background color used by all derived contexts.
  /// Initially set to color::Transparent.
  void setBackgroundColor(Color bgcolor) {
    bgcolor_ = bgcolor;
    display_device_.setBgColorHint(bgcolor);
  }

  /// Returns the background color for derived contexts.
  Color getBackgroundColor() const { return bgcolor_; }

  /// Clears the display, respecting the clip box and background settings.
  void clear();

  void enableTurbo();

  void disableTurbo();

  bool isTurboEnabled() const { return turbo_ != nullptr; }

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
  FillMode fill_mode() const { return FillMode::kVisible; }
  BlendingMode blending_mode() const { return BlendingMode::kSourceOver; }

  DisplayDevice& display_device_;
  std::unique_ptr<BackgroundFillOptimizer::FrameBuffer> turbo_frame_buffer_;
  std::unique_ptr<BackgroundFillOptimizer> turbo_;
  // Set to either the display_device_ or the turbo wrapper.
  DisplayOutput* output_;
  TouchDisplay touch_;
  int16_t nest_level_;
  Orientation orientation_;

  Box extents_;
  Color bgcolor_;
  const Rasterizable* background_;
};

/// @brief Primary top-level interface for drawing to screens, off-screen
/// buffers, or other devices.
///
/// Supports rectangular clip regions, as well as arbitrary clip masks.
///
/// Able to render objects implementing the Drawable interface.
class DrawingContext {
 public:
  /// Constructs a drawing context covering the entire display.
  template <typename Display>
  DrawingContext(Display& display)
      : DrawingContext(display, 0, 0, display.extents()) {}

  /// Constructs a drawing context covering the specified bounds, relative to
  /// the display's extents. If bounds exceed the display's extents, they will
  /// be clipped accordingly, but the requested bounds will still be used for
  /// alignment purposes. For example, drawing a centered object will center it
  /// relative to the bounds(), even if they exceed the display's extents.
  template <typename Display>
  DrawingContext(Display& display, Box bounds)
      : DrawingContext(display, 0, 0, bounds) {}

  /// Constructs a drawing context covering the entire display, with device
  /// coordinates offset by the specified amounts.
  template <typename Display>
  DrawingContext(Display& display, int16_t x_offset, int16_t y_offset)
      : DrawingContext(display, x_offset, y_offset,
                       display.extents().translate(-x_offset, -y_offset)) {}

  /// Constructs a drawing context covering the specified bounds, with device
  /// coordinates offset by the specified amounts. The bounds are expressed
  /// post-translation. That is, bounds corresponding to the entire display
  /// surface would be equal to display.extents().translate(-x_offset,
  /// -y_offset). If bounds exceed the display's extents, they will be clipped
  /// accordingly, but the requested bounds will still be used for alignment
  /// purposes. For example, drawing a centered object will center it relative
  /// to the requested bounds, even if they exceed the display's extents.
  template <typename Display>
  DrawingContext(Display& display, int16_t x_offset, int16_t y_offset,
                 Box bounds)
      : output_(display.output()),
        dx_(display.dx() + x_offset),
        dy_(display.dy() + y_offset),
        bounds_(bounds),
        max_clip_box_(Box::Intersect(
            bounds, display.extents().translate(-x_offset, -y_offset))),
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

  /// Returns the bounds used for alignment.
  const Box& bounds() const { return bounds_; }

  /// Returns the width of the drawing context. Equivalent to bounds().width().
  const uint16_t width() const { return bounds_.width(); }

  /// Returns the height of the drawing context. Equivalent to
  /// bounds().height().
  const uint16_t height() const { return bounds_.height(); }

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

  /// Clears the display, respecting the clip box, and background settings.
  void clear();

  /// Fills the display with the specified color, respecting the clip box.
  void fill(Color color);

  /// Sets the clip box, intersected with the maximum allowed clip box.
  /// Expressed in device coordinates.
  void setClipBox(const Box& clip_box) {
    clip_box_ = Box::Intersect(clip_box, max_clip_box_);
  }

  /// Sets the clip box, intersected with the maximum allowed clip box.
  /// Expressed in device coordinates.
  void setClipBox(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    setClipBox(Box(x0, y0, x1, y1));
  }

  void setClipMask(const ClipMask* clip_mask) { clip_mask_ = clip_mask; }

  /// Returns the current clip box, in device coordinates.
  const Box& getClipBox() const { return clip_box_; }

  // void applyTransformation(Transformation t) {
  //   transformation_ = transformation_.transform(t);
  // }

  /// Sets a transformation to be applied to all drawn objects when converting
  /// the drawing coordinates to device coordinates.
  void setTransformation(Transformation t) {
    transformation_ = t;
    transformed_ = (t.xy_swap() || t.is_rescaled() || t.is_translated());
  }

  /// Returns the current transformation. (The identity transformation by
  /// default).
  const Transformation& transformation() const { return transformation_; }

  void setWriteOnce();

  bool isWriteOnce() const { return write_once_; }

  /// Allows to efficiently draw pixels to the context, using a buffered
  /// pixel writer (avoiding virtual calls per pixel, and allowing for batch
  /// writes). The provided function `fn` will be called with a
  /// ClippingBufferedPixelWriter that respects the current clip box and
  /// uses the specified blending mode.
  void drawPixels(const std::function<void(ClippingBufferedPixelWriter&)>& fn,
                  BlendingMode blending_mode = BlendingMode::kSourceOver);

  /// Draws the object using its inherent coordinates. The point (0, 0) in the
  /// object's coordinates maps to (0, 0) in the context's coordinates
  /// (subject to the optional transformation).
  inline void draw(const Drawable& object) {
    drawInternal(object, 0, 0, bgcolor_);
  }

  /// Draws the object using the specified absolute offset. The point (0, 0) in
  /// the object's coordinates maps to (dx, dy) in the context's coordinates
  /// (subject to the optional transformation).
  inline void draw(const Drawable& object, int16_t dx, int16_t dy) {
    drawInternal(object, dx, dy, bgcolor_);
  }

  /// Draws the object applying the specified alignment, relative to the
  /// bounds(). For example, for with kMiddle | kCenter, the object will be
  /// centered relative to the bounds().
  void draw(const Drawable& object, Alignment alignment) {
    Box anchorExtents = object.anchorExtents();
    if (transformed_) {
      anchorExtents = transformation_.transformBox(anchorExtents);
    }
    Offset offset = alignment.resolveOffset(bounds(), anchorExtents);
    drawInternal(object, offset.dx, offset.dy, bgcolor_);
  }

  /// Analogous to draw(object), but instead of drawing, replaces all the output
  /// pixels with the background color.
  void erase(const Drawable& object);

  /// Analogous to draw(object, dx, dy), but instead of drawing, replaces all
  /// the output pixels with the background color.
  void erase(const Drawable& object, int16_t dx, int16_t dy);

  /// Analogous to draw(object, alignment), but instead of drawing, replaces all
  /// the output pixels with the background color.
  void erase(const Drawable& object, Alignment alignment);

 private:
  DisplayOutput& output() { return output_; }
  const DisplayOutput& output() const { return output_; }

  void drawInternal(const Drawable& object, int16_t dx, int16_t dy,
                    Color bgcolor);

  void drawInternalWithBackground(Surface& s, const Drawable& object);

  void drawInternalTransformed(Surface& s, const Drawable& object);

  DisplayOutput& output_;

  /// Offset of the origin in the output coordinates. Empty Transformation maps
  /// (0, 0) in drawing coordinates onto (dx_, dy_) in device coordinates.
  int16_t dx_;
  int16_t dy_;

  /// Bounds used for alignment. Default to device extents. If different than
  /// device extents, they both constrain the initial and max clip box.
  Box bounds_;

  /// The maximum allowed clip box. setClipBox will intersect its argument
  /// with it. Equal to the intersection of bounds() and the original device
  /// extents.
  const Box max_clip_box_;

  /// Absolute coordinates of the clip region in the device space. Inclusive.
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

/**
 * @brief Infinite single-color area.
 *
 * When drawn, it fills the entire clip box with the given color.
 */
class Fill : public Rasterizable {
 public:
  /// Constructs a fill with a constant color.
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

/**
 * @brief Infinite transparent area.
 *
 * When drawn, it fills the entire clip box with the color implied by the
 * background settings.
 */
class Clear : public Rasterizable {
 public:
  /// Constructs a clear fill.
  Clear() {}

  Box extents() const override { return Box::MaximumBox(); }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override;

  bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                     Color* result) const override;

 private:
  void drawTo(const Surface& s) const override;
};

/**
 * @brief Temporarily pauses the output transport while a DrawingContext is
 * active.
 *
 * This releases the bus (e.g. SPI) to allow custom drawables to perform
 * operations on the same bus (e.g. reading data from an SD card). When an
 * instance of this class is alive, and unless ResumeOutput is also used,
 * DrawingContexts do not function and no drawing operations should be
 * attempted.
 *
 * See jpeg.cpp and png.cpp for an application example.
 */
class PauseOutput {
 public:
  /// Ends the output transaction on construction.
  PauseOutput(DisplayOutput& out) : out_(out) { out_.end(); }
  /// Restarts the output transaction on destruction.
  ~PauseOutput() { out_.begin(); }

 private:
  DisplayOutput& out_;
};

/**
 * @brief Resumes a paused output transaction.
 *
 * Inverse of PauseOutput; enables drawing while in scope.
 */
class ResumeOutput {
 public:
  /// Begins the output transaction on construction.
  ResumeOutput(DisplayOutput& out) : out_(out) { out_.begin(); }
  /// Ends the output transaction on destruction.
  ~ResumeOutput() { out_.end(); }

 private:
  DisplayOutput& out_;
};

}  // namespace roo_display
