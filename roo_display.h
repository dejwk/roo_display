#pragma once

#include <functional>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"
#include "roo_display/filter/background.h"
#include "roo_display/filter/clip_mask.h"
#include "roo_display/filter/front_to_back_writer.h"
#include "roo_display/filter/transformed.h"
#include "roo_display/ui/alignment.h"

namespace roo_display {

class FrontToBackWriter;

class TouchDisplay {
 public:
  TouchDisplay(DisplayDevice &display_device, TouchDevice &touch_device)
      : display_device_(display_device),
        touch_device_(touch_device),
        touched_(false) {}

  bool getTouch(int16_t &x, int16_t &y);

 private:
  DisplayDevice &display_device_;
  TouchDevice &touch_device_;

  bool touched_;
  long last_sample_time_;
  int16_t raw_touch_x_;
  int16_t raw_touch_y_;
  int16_t raw_touch_z_;
};

class Display {
 public:
  Display(DisplayDevice &display_device) : Display(display_device, nullptr) {}

  Display(DisplayDevice &display_device, TouchDevice &touch_device)
      : Display(display_device, &touch_device) {}

  int32_t area() const {
    return display_device_.raw_width() * display_device_.raw_height();
  }

  const Box &extents() const { return extents_; }

  int16_t width() const { return extents_.width(); }
  int16_t height() const { return extents_.height(); }

  // Initializes the device.
  void init() { display_device_.init(); }

  // Initializes the device, fills screen with the specified color, and sets
  // that color as the defaul background hint.
  void init(Color bgcolor);

  // Sets the orientation of the display. Setting the orientation resets
  // the clip box to the maximum display area.
  void setOrientation(Orientation orientation);

  Orientation orientation() const { return orientation_; }

  DisplayOutput &output() { return display_device_; }
  const DisplayOutput &output() const { return display_device_; }

  bool getTouch(int16_t &x, int16_t &y) { return touch_.getTouch(x, y); }

  // Resets the clip box to the maximum device-allowed values.
  void resetExtents() {
    extents_ = Box(0, 0, display_device_.effective_width() - 1,
                   display_device_.effective_height() - 1);
  }

  // Sets a default clip box, inherited by all derived contexts.
  void setExtents(const Box &extents) {
    resetExtents();
    extents_ = Box::intersect(extents_, extents);
  }

  // Sets a rasterizable background to be used by all derived contexts.
  void setBackground(const Rasterizable *bg) {
    background_ = bg;
    bgcolor_ = color::Transparent;
  }

  const Rasterizable *getRasterizableBackground() const { return background_; }

  // Sets a background color to be used by all derived contexts.
  // Initially set to color::Transparent.
  void setBackground(Color bgcolor) {
    background_ = nullptr;
    bgcolor_ = bgcolor;
    display_device_.setBgColorHint(bgcolor);
  }

  Color getBgColor() const { return bgcolor_; }

  // Clears the display, respecting the clip box, and background settings.
  void clear();

 private:
  Display(DisplayDevice &display_device, TouchDevice *touch_device);

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

  DisplayDevice &display_device_;
  TouchDisplay touch_;
  int16_t nest_level_;
  Orientation orientation_;

  Box extents_;
  Color bgcolor_;
  const Rasterizable *background_;
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
  DrawingContext(Display &display)
      : output_(display.output()),
        dx_(display.dx()),
        dy_(display.dy()),
        bounds_(display.extents()),
        clip_box_(bounds_),
        unnest_([&display]() { display.unnest(); }),
        write_once_(display.is_write_once()),
        fill_mode_(FILL_MODE_VISIBLE),
        paint_mode_(PAINT_MODE_BLEND),
        clip_mask_(nullptr),
        background_(display.getRasterizableBackground()),
        bgcolor_(display.getBgColor()),
        transformed_(false),
        transform_() {
    display.nest();
  }

  ~DrawingContext();

  const Box &bounds() const { return bounds_; }

  void setBackground(const Rasterizable *bg) {
    background_ = bg;
    bgcolor_ = color::Transparent;
  }

  void setBackground(Color bgcolor) {
    background_ = nullptr;
    bgcolor_ = bgcolor;
  }

  FillMode fillMode() const { return fill_mode_; }
  void setFillMode(FillMode fill_mode) { fill_mode_ = fill_mode; }

  PaintMode paintMode() const { return paint_mode_; }
  void setPaintMode(PaintMode paint_mode) { paint_mode_ = paint_mode; }

  // Clears the display, respecting the clip box, and background settings.
  void clear();

  // Fills the display with the specified color, respecting the clip box.
  void fill(Color color);

  void setClipBox(const Box &clip_box) {
    clip_box_ = Box::intersect(clip_box, bounds());
  }

  void setClipBox(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    setClipBox(Box(x0, y0, x1, y1));
  }

  void setClipMask(const ClipMask *clip_mask) { clip_mask_ = clip_mask; }

  const Box &getClipBox() const { return clip_box_; }

  // void applyTransform(Transform t) {
  //   transform_ = transform_.transform(t);
  // }

  void setTransform(Transform t) {
    transform_ = t;
    transformed_ = (t.xy_swap() || t.is_rescaled() || t.is_translated());
  }

  const Transform &transform() const { return transform_; }

  void setWriteOnce();

  void drawPixels(const std::function<void(ClippingBufferedPixelWriter &)> &fn,
                  PaintMode paint_mode = PAINT_MODE_BLEND);

  // Draws the object using its inherent coordinates. The point (0, 0) in the
  // object's coordinates maps to (0, 0) in the context's coordinates.
  void draw(const Drawable &object) { drawInternal(object, 0, 0, bgcolor_); }

  // Draws the object using the specified offset. The point (0, 0) in the
  // object's coordinates maps to (dx, dy) in the context's coordinates.
  void draw(const Drawable &object, int16_t dx, int16_t dy) {
    drawInternal(object, dx, dy, bgcolor_);
  }

  // Draws the object applying the specified offset, and the specified
  // alignment. The point indicated by the alignment, in the object's
  // coordinates, maps to (dx, dy) in the contex't coordinates. For example, for
  // halign = Left(), the object's xMin will be drawn at dx; for valign =
  // Bottom(), the object's yMax will be drawn at dy - 1, and so one.
  //
  // Caution: don't use this to right-align numeric values. The digit '1' has
  // the nasty property of being narrower than others, so aligning numbers this
  // way will cause some jitter when the last digit changes to and from '1'.
  // Instead, explicitly shift dx by the text's advance, e.g.:
  //
  // TextLabel label(...);
  // dc.draw(label, dx - label.metrics().advance(), dy);
  void draw(const Drawable &object, int16_t dx, int16_t dy,
            Alignment alignment) {
    const Box &extents = object.extents();
    int16_t xMin = extents.xMin();
    int16_t yMin = extents.yMin();
    int16_t xMax = extents.xMax();
    int16_t yMax = extents.yMax();
    if (transformed_) transform_.transformRect(xMin, yMin, xMax, yMax);
    drawInternal(object, dx - alignment.h().GetOffset(xMin, xMax),
                 dy - alignment.v().GetOffset(yMin, yMax), bgcolor_);
  }

  // Analogous to draw(object), but instead of drawing, replaces all the output
  // pixels with the background color.
  void erase(const Drawable &object);

  // Analogous to draw(object, dx, dy), but instead of drawing, replaces all the
  // output pixels with the background color.
  void erase(const Drawable &object, int16_t dx, int16_t dy);

  // Analogous to draw(object, dx, dy, halign, valign), but instead of drawing,
  // replaces all the output pixels with the background color.
  void erase(const Drawable &object, int16_t dx, int16_t dy,
             Alignment alignment);

 private:
  DisplayOutput &output() { return output_; }
  const DisplayOutput &output() const { return output_; }

  void drawInternal(const Drawable &object, int16_t dx, int16_t dy,
                    Color bgcolor);

  void drawInternalWithBackground(DisplayOutput &output, const Drawable &object,
                                  int16_t dx, int16_t dy, Color bgcolor);

  void drawInternalTransformed(DisplayOutput &output, const Drawable &object,
                               int16_t dx, int16_t dy, Color bgcolor);

  DisplayOutput &output_;

  // Offset of the origin in the output coordinates. Empty transform maps (0, 0)
  // in drawing coordinates onto (dx_, dy_) in device coordinates.
  int16_t dx_;
  int16_t dy_;

  Box bounds_;
  // Absolute coordinates of the clip region in the device space. Inclusive.
  Box clip_box_;

  std::function<void()> unnest_;

  bool write_once_;
  std::unique_ptr<FrontToBackWriter> front_to_back_writer_;

  FillMode fill_mode_;
  PaintMode paint_mode_;

  const ClipMask *clip_mask_;
  const Rasterizable *background_;
  Color bgcolor_;
  bool transformed_;
  Transform transform_;
};

// Fill is an 'infinite' single-color area. When drawn, it will fill the
// entire clip box with the given color. Fill ignores the surface's paint
// mode, and always uses PAINT_MODE_REPLACE.
class Fill : public Drawable {
 public:
  Fill(Color color) : color_(color) {}

  Box extents() const override { return Box::MaximumBox(); }

 private:
  void drawTo(const Surface &s) const override {
    Color color = alphaBlend(s.bgcolor(), color_);
    s.out().fillRect(PAINT_MODE_REPLACE, s.clip_box(), color);
  }

  Color color_;
};

// Clear is an 'infinite' transparent area. When drawn, it will fill the
// entire clip box with the color implied by background settings. Clear
// ignores the surface's paint mode, and always uses PAINT_MODE_REPLACE.
class Clear : public Drawable {
 public:
  Clear() {}

  Box extents() const override { return Box::MaximumBox(); }

 private:
  void drawTo(const Surface &s) const override {
    s.out().fillRect(PAINT_MODE_REPLACE, s.clip_box(), s.bgcolor());
  }
};

}  // namespace roo_display
