#pragma once

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"
#include "roo_display/filter/background.h"
#include "roo_display/filter/clip_mask.h"
#include "roo_display/filter/transformed.h"
#include "roo_display/ui/alignment.h"

namespace roo_display {

class TouchDisplay {
 public:
  TouchDisplay(DisplayDevice &display, TouchDevice &touch)
      : display_(display), touch_(touch) {}

  bool getTouch(int16_t &x, int16_t &y);

 private:
  DisplayDevice &display_;
  TouchDevice &touch_;
};

class Display {
 public:
  Display(DisplayDevice &display) : Display(display, nullptr) {}

  Display(DisplayDevice &display, TouchDevice &touch)
      : Display(display, &touch) {}

  int16_t width() const { return width_; }
  int16_t height() const { return height_; }
  int32_t area() const { return width_ * height_; }
  Box extents() const { return Box(0, 0, width() - 1, height() - 1); }

  // Initializes the device.
  void init() { display_.init(); }

  // Initializes the device, fills screen with the specified color, and sets
  // that color as the defaul background hint.
  void init(Color bgcolor);

  // Sets the orientation of the display. Setting the orientation resets
  // the clip box to the maximum display area.
  void setOrientation(Orientation orientation);

  Orientation orientation() const { return orientation_; }

  DisplayOutput &output() { return display_; }
  const DisplayOutput &output() const { return display_; }

  bool getTouch(int16_t &x, int16_t &y) { return touch_.getTouch(x, y); }

  // Sets a default clip box, inherited by all derived contexts.
  void setClipBox(int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    setClipBox(Box(x0, y0, x1, y1));
  }

  // Sets a default clip box, inherited by all derived contexts.
  void setClipBox(const Box &clip_box) {
    clip_box_ = Box::intersect(clip_box, extents());
  }

  // Returns the default clip box.
  const Box &getClipBox() const { return clip_box_; }

  // Sets a rasterizable background to be used by all derived contexts.
  void setBackground(const Rasterizable *bg) {
    background_ = bg;
    bgcolor_ = color::Transparent;
  }

  // Sets a background color to be used by all derived contexts.
  // Initially set to color::Transparent.
  void setBackground(Color bgcolor) {
    background_ = nullptr;
    bgcolor_ = bgcolor;
    display_.setBgColorHint(bgcolor);
  }

  // Clears the display, respecting the clip box, and background settings.
  void clear();

 private:
  Display(DisplayDevice &display, TouchDevice *touch);

  friend class DrawingContext;

  void nest() {
    if (nest_level_++ == 0) {
      display_.begin();
    }
  }

  void unnest() {
    if (--nest_level_ == 0) {
      display_.end();
    }
  }

  void updateBounds();

  DisplayDevice &display_;
  TouchDisplay touch_;
  int16_t nest_level_;
  int16_t width_;
  int16_t height_;
  Orientation orientation_;

  Box clip_box_;
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
  DrawingContext(Display display)
      : display_(display),
        fill_mode_(FILL_MODE_VISIBLE),
        paint_mode_(PAINT_MODE_BLEND),
        clip_box_(display.clip_box_),
        clip_mask_(nullptr),
        background_(display.background_),
        bgcolor_(display.bgcolor_),
        transformed_(false),
        transform_() {
    display_.nest();
  }

  ~DrawingContext() { display_.unnest(); }

  int16_t width() const { return display_.width(); }
  int16_t height() const { return display_.height(); }

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
    clip_box_ = Box::intersect(clip_box, display_.clip_box_);
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

  inline void drawPixel(int16_t x, int16_t y, Color color) {
    drawPixel(x, y, color, paint_mode_);
  }

  inline void drawPixel(int16_t x, int16_t y, Color color,
                        PaintMode paint_mode) {
    if (!clip_box_.contains(x, y)) return;
    if (clip_mask_ != nullptr && clip_mask_->isMasked(x, y)) return;
    output().fillPixels(paint_mode, color, &x, &y, 1);
  }

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
  // CAUtION: don't use this to right-align numeric values. The digit '1' has
  // the nasty property of being narrower than others, so aligning numbers this
  // way will cause some jitter when the last digit changes to and from '1'.
  // Instead, explicitly shift dx by the text's advance, e.g.:
  //
  // TextLabel label(...);
  // dc.draw(label, dx - label.metrics().advance(), dy);
  void draw(const Drawable &object, int16_t dx, int16_t dy, HAlign halign,
            VAlign valign) {
    const Box &extents = object.extents();
    drawInternal(object, dx - halign.GetOffset(extents.xMin(), extents.xMax()),
                 dy - valign.GetOffset(extents.yMin(), extents.yMax()),
                 bgcolor_);
  }

  // Analogous to draw(object), but instead of drawing, replaces all the output
  // pixels with the background color.
  void erase(const Drawable &object);

  // Analogous to draw(object, dx, dy), but instead of drawing, replaces all the
  // output pixels with the background color.
  void erase(const Drawable &object, int16_t dx, int16_t dy);

  // Analogous to draw(object, dx, dy, halign, valign), but instead of drawing,
  // replaces all the output pixels with the background color.
  void erase(const Drawable &object, int16_t dx, int16_t dy, HAlign halign,
             VAlign valign);

 private:
  DisplayOutput &output() { return display_.output(); }
  const DisplayOutput &output() const { return display_.output(); }

  void drawInternal(const Drawable &object, int16_t dx, int16_t dy,
                    Color bgcolor) {
    if (clip_mask_ == nullptr) {
      drawInternalWithBackground(output(), object, dx, dy, bgcolor);
    } else {
      ClipMaskFilter filter(output(), clip_mask_);
      drawInternalWithBackground(filter, object, dx, dy, bgcolor);
    }
  }

  void drawInternalWithBackground(DisplayOutput &output, const Drawable &object,
                                  int16_t dx, int16_t dy, Color bgcolor) {
    if (background_ == nullptr) {
      drawInternalTransformed(output, object, dx, dy, bgcolor);
    } else {
      BackgroundFilter filter(output, background_);
      drawInternalTransformed(filter, object, dx, dy, bgcolor);
    }
  }

  void drawInternalTransformed(DisplayOutput &output, const Drawable &object,
                               int16_t dx, int16_t dy, Color bgcolor) {
    Surface s(output, dx, dy, clip_box_, bgcolor, fill_mode_, paint_mode_);
    if (!transformed_) {
      s.drawObject(object);
    } else if (!transform_.is_rescaled() && !transform_.xy_swap()) {
      // Translation only.
      s.set_dx(s.dx() + transform_.x_offset());
      s.set_dy(s.dy() + transform_.y_offset());
      s.drawObject(object);
    } else {
      s.drawObject(TransformedDrawable(transform_, &object));
    }
  }

  Display& display_;
  FillMode fill_mode_;
  PaintMode paint_mode_;

  // Absolute coordinates of the clip region in the device space. Inclusive.
  Box clip_box_;
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
