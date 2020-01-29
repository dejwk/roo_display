#pragma once

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"
#include "roo_display/core/streamable_overlay.h"
#include "roo_display/filter/clip_mask.h"
#include "roo_display/filter/transformed.h"

namespace roo_display {

class TouchDisplay {
 public:
  TouchDisplay(DisplayDevice *display, TouchDevice *touch)
      : display_(display), touch_(touch) {}

  bool getTouch(int16_t *x, int16_t *y);

 private:
  DisplayDevice *display_;
  TouchDevice *touch_;
};

class Display {
 public:
  Display(DisplayDevice *display, TouchDevice *touch = nullptr);

  int16_t width() const { return width_; }
  int16_t height() const { return height_; }
  int32_t area() const { return width_ * height_; }
  Box extents() const { return Box(0, 0, width() - 1, height() - 1); }

  // Initializes the device.
  void init() { display_->init(); }

  // Initializes the device, fills screen with the specified color, and sets
  // that color as the defaul bacground hint.
  void init(Color bgcolor);

  void setOrientation(Orientation orientation);
  Orientation orientation() const { return orientation_; }

  DisplayOutput *output() { return display_; }
  const DisplayOutput &output() const { return *display_; }

  bool getTouch(int16_t *x, int16_t *y) { return touch_.getTouch(x, y); }

 private:
  friend class DrawingContext;

  void nest() {
    if (nest_level_++ == 0) {
      display_->begin();
    }
  }

  void unnest() {
    if (--nest_level_ == 0) {
      display_->end();
    }
  }

  void updateBounds();

  DisplayDevice *display_;
  TouchDisplay touch_;
  int16_t nest_level_;
  int16_t width_;
  int16_t height_;
  Orientation orientation_;
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
  DrawingContext(Display *display)
      : display_(display),
        bgcolor_(color::Transparent),
        clip_box_(0, 0, display->width() - 1, display->height() - 1),
        clip_mask_(nullptr),
        transformed_(false),
        transform_() {
    display_->nest();
  }

  ~DrawingContext() { display_->unnest(); }

  int16_t width() const { return display_->width(); }
  int16_t height() const { return display_->height(); }

  Color bgColor() const { return bgcolor_; }
  void setBgColor(Color color) { bgcolor_ = color; }

  void fill(Color color);

  void setClipBox(const Box &clip_box) {
    clip_box_ = Box::intersect(clip_box, Box(0, 0, width() - 1, height() - 1));
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
    if (!clip_box_.contains(x, y)) return;
    if (clip_mask_ != nullptr && !clip_mask_->isSet(x, y)) return;
    output()->fillPixels(PAINT_MODE_REPLACE, color, &x, &y, 1);
  }

  inline void paintPixel(int16_t x, int16_t y, Color color) {
    if (!clip_box_.contains(x, y)) return;
    if (clip_mask_ != nullptr && !clip_mask_->isSet(x, y)) return;
    output()->fillPixels(PAINT_MODE_BLEND, color, &x, &y, 1);
  }

  void draw(const Drawable &object) { drawInternal(object, 0, 0, bgcolor_); }

  void draw(const Drawable &object, int16_t dx, int16_t dy) {
    drawInternal(object, dx, dy, bgcolor_);
  }

  // T must implement the streamable contract, as defined in "streamable.h"
  // Note. the 2nd template parameter is used to force SFINAE, that is,
  // to make sure that the compiler only considers classes that actually
  // implement 'CreateStream' to be viable for this overload. (If the
  // method is missing, the template substitution fails, and the class
  // is not considered viable because SFINAE).
  template <typename T,
            typename Stream = decltype(std::declval<T>().CreateStream())>
  void draw(const T &object, int16_t dx, int16_t dy) {
    DrawableStreamable<internal::StreamableRef<T>> drawable(Ref(object));
    drawInternal(drawable, dx, dy, bgcolor_);
  }

  template <typename T,
            typename Stream = decltype(std::declval<T>().CreateStream())>
  void draw(const T &object) {
    draw<T, Stream>(object, 0, 0);
  }

 private:
  DisplayOutput *output() { return display_->output(); }
  const DisplayOutput *output() const { return display_->output(); }

  void drawInternal(const Drawable &object, int16_t dx, int16_t dy,
                    Color bgcolor) {
    if (clip_mask_ == nullptr) {
      drawInternalTransformed(output(), object, dx, dy, bgcolor);
    } else {
      ClipMaskFilter filter(output(), clip_mask_);
      drawInternalTransformed(&filter, object, dx, dy, bgcolor);
    }
  }

  void drawInternalTransformed(DisplayOutput *output, const Drawable &object,
                               int16_t dx, int16_t dy, Color bgcolor) {
    Surface s(output, dx, dy, clip_box_, bgcolor);
    if (!transformed_) {
      s.drawObject(object);
    } else {
      s.drawObject(TransformedDrawable(transform_, &object));
    }
  }

  Display *display_;
  Color bgcolor_;

  // Absolute coordinates of the clip region in the device space. Inclusive.
  Box clip_box_;
  const ClipMask *clip_mask_;
  bool transformed_;
  Transform transform_;
};

class Fill : public Drawable {
 public:
  Fill(Color color) : color_(color) {}

  Box extents() const override { return Box::MaximumBox(); }

 private:
  void drawTo(const Surface &s) const override {
    s.out->fillRect(s.clip_box, color_);
  }

  Color color_;
};

}  // namespace roo_display