#pragma once

#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_display/core/box.h"

namespace roo_display {

class DisplayOutput;
class Drawable;

// FillMode specifies whether the Drawable should fill the entire extents box,
// including the pixels that are fully transparent.
enum FillMode {
  // Specifies that the entire extents box should get filled (possibly with
  // fully transparent pixels). It is useful e.g. when the image is drawn
  // over a synthetic background, and we want to have previous content
  // replaces with the background.
  FILL_MODE_RECTANGLE = 0,

  // Specifies that the fully-transparent pixels don't need to get filled.
  FILL_MODE_VISIBLE = 1
};

class Rasterizable;

// Low-level 'handle' to draw to an underlying device. Passed by the library
// to Drawable.draw() (see below). Don't create directly.
//
// Note that the device uses an absolute coordinate system starting at (0, 0).
// In order to translate the object's extents to device coordinates in
// accordance with the surface's possible translation offset, use the following
// formula:
//
//   extents.translate(s.dx(), s.dy())
//
// Additionally, make sure that the above box is constrained to the clip_box;
// e.g:
//
//   Box::Intersect(s.clip_box(), extents.translate(s.dx(), s.dy()))
//
// It is often useful to quickly reject objects that are entirely outside the
// clip box:
//
//   Box bounds =
//     Box::Intersect(s.clip_box(), extents.translate(s.dx(), s.dy()));
//   if (bounds.empty()) return;
//
// In order to compute the clipped bounding box in the object's coordinates,
// the transformation can be applied in the other direction:
//
//   Box clipped =
//     Box::Intersect(s.clip_box().translace(-s.dx(), -s.dy()), extents);
//
class Surface {
 public:
  Surface(DisplayOutput &out, int16_t dx, int16_t dy, Box clip,
          bool is_write_once, Color bg = color::Transparent,
          FillMode fill_mode = FILL_MODE_VISIBLE,
          BlendingMode blending_mode = BLENDING_MODE_SOURCE_OVER)
      : out_(&out),
        dx_(dx),
        dy_(dy),
        clip_box_(std::move(clip)),
        is_write_once_(is_write_once),
        bgcolor_(bg),
        fill_mode_(fill_mode),
        blending_mode_(blending_mode) {
    if (bg.a() == 0xFF) {
      blending_mode = BLENDING_MODE_SOURCE;
    }
  }

  Surface(DisplayOutput *out, Box clip, bool is_write_once,
          Color bg = color::Transparent, FillMode fill_mode = FILL_MODE_VISIBLE,
          BlendingMode blending_mode = BLENDING_MODE_SOURCE_OVER)
      : out_(out),
        dx_(0),
        dy_(0),
        clip_box_(std::move(clip)),
        is_write_once_(is_write_once),
        bgcolor_(bg),
        fill_mode_(fill_mode),
        blending_mode_(blending_mode) {
    if (bg.a() == 0xFF && (blending_mode == BLENDING_MODE_SOURCE_OVER ||
                           blending_mode == BLENDING_MODE_SOURCE_OVER_OPAQUE)) {
      blending_mode = BLENDING_MODE_SOURCE;
    }
  }

  Surface(Surface &&other) = default;
  Surface(const Surface &other) = default;

  // Returns the device to which to draw.
  DisplayOutput &out() const { return *out_; }

  void set_out(DisplayOutput *out) { out_ = out; }

  // Returns the x offset that should be applied to the drawn object.
  int16_t dx() const { return dx_; }

  // Returns the y offset that should be applied to the drawn object.
  int16_t dy() const { return dy_; }

  bool is_write_once() const { return is_write_once_; }

  // Returns the clip box, in the device coordinates (independent of
  // the x, y offset), that must be respected by the drawn object.
  const Box &clip_box() const { return clip_box_; }

  void set_clip_box(const Box &clip_box) { clip_box_ = clip_box; }

  // Returns the background color that should be applied in case when the
  // drawn object has any non-fully-opaque content.
  Color bgcolor() const { return bgcolor_; }

  void set_bgcolor(Color bgcolor) { bgcolor_ = bgcolor; }

  // Returns the fill mode that should be observed by the drawn object.
  // If FILL_MODE_RECTANGLE, the drawn object MUST fill its entire
  // (clipped) extents, even if some pixels are completely transparent.
  // If FILL_MODE_VISIBLE, the drawn object may omit completely transparent
  // pixels. Note that it is OK to omit such pixels even if a background
  // color is also specified. This fill mode essentially assumes that the
  // appropriate background has been pre-applied.
  FillMode fill_mode() const { return fill_mode_; }

  void set_fill_mode(FillMode fill_mode) { fill_mode_ = fill_mode; }

  // Returns the blending mode that should generally be passed to the device
  // when the object is drawn. Defaults to BLENDING_MODE_SOURCE_OVER.
  // If the mode is BLENDING_MODE_SOURCE_OVER, the object may replace it
  // with BLENDING_MODE_SOURCE when it is certain that all pixels it
  // writes are completely opaque. Note that in case when the background
  // is specified and completely opaque, BLENDING_MODE_SOURCE_OVER is
  // automatically replaced with BLENDING_MODE_SOURCE.
  BlendingMode blending_mode() const { return blending_mode_; }

  void set_blending_mode(BlendingMode blending_mode) {
    blending_mode_ = blending_mode;
  }

  void set_dx(int16_t dx) { dx_ = dx; }
  void set_dy(int16_t dy) { dy_ = dy; }

  // Draws the specified drawable object to this surface. Intended for custom
  // implementations of Drawable (below), to draw other objects as part of
  // drawing itself.
  inline void drawObject(const Drawable &object) const;

  // Draws the specified drawable object to this surface, at the specified
  // offset. A convenience wrapper around drawObject().
  inline void drawObject(const Drawable &object, int16_t dx, int16_t dy) const {
    if (dx == 0 && dy == 0) {
      drawObject(object);
      return;
    }
    Surface s = *this;
    s.set_dx(s.dx() + dx);
    s.set_dy(s.dy() + dy);
    s.drawObject(object);
  }

  Box::ClipResult clipToExtents(Box extents) {
    return clip_box_.clip(extents.translate(dx_, dy_));
  }

 private:
  friend class DrawingContext;

  DisplayOutput &output() const { return *out_; }
  Box extents() const { return clip_box_.translate(-dx_, -dy_); }
  void nest() const {}
  void unnest() const {}
  const Rasterizable *getRasterizableBackground() const { return nullptr; }
  Color getBackgroundColor() const { return bgcolor_; }

  DisplayOutput *out_;
  int16_t dx_;
  int16_t dy_;
  Box clip_box_;
  bool is_write_once_;
  Color bgcolor_;
  FillMode fill_mode_;
  BlendingMode blending_mode_;
};

// Interface implemented by any class that represents things (like images in
// various formats) that can be drawn to an output device (such as a TFT
// screen, or an off-screen buffer).
//
// Implementations must override two methods:
// * either drawTo or drawInteriorTo (see below), to implement the actual
//   drawing logic;
// * extents(), returning a bounding box which the drawing fits into,
//   assuming it were drawn at (0, 0).
class Drawable {
 public:
  virtual ~Drawable() {}

  // Returns the bounding box encompassing all pixels that need to be drawn.
  virtual Box extents() const = 0;

  // Returns the boundaries to be used when aligning this drawable. By default,
  // equivalent to extents(). Some drawables, notably text labels, may want to
  // use different bounds for alignment.
  virtual Box anchorExtents() const { return extents(); }

  // A singleton, representing a 'no-op' drawable with no bounding box.
  static const Drawable *Empty();

 private:
  friend void Surface::drawObject(const Drawable &object) const;

  // Draws this object's content, respecting the fill mode. That is, if
  // s.fill_mode() == FILL_MODE_RECTANGLE, the method must fill the entire
  // (clipped) extents() rectangle (using s.bgcolor() for transparent parts).
  //
  // The default implementation fills the clipped extents() rectangle
  // with bgcolor, and then calls drawInteriorTo. That causes flicker, so you
  // should try to override this method if a better implementation is possible.
  //
  // The implementation must also respect the other surface's parameters,
  // particularly the clip_box - i.e. it is not allowed to draw to output
  // outside of the clip_box.
  virtual void drawTo(const Surface &s) const;

  // Draws this object's content, ignoring s.fill_mode(). See drawTo().
  //
  // The implementation most respect the other surface's parameters,
  // particularly the clip_box - i.e. it is not allowed to draw to output
  // outside of the clip_box.
  virtual void drawInteriorTo(const Surface &s) const {}
};

inline void Surface::drawObject(const Drawable &object) const {
  object.drawTo(*this);
}

}  // namespace roo_display