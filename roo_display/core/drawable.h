#pragma once

#include "roo_display/core/box.h"
#include "roo_display/core/color.h"

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
//   Box::intersect(s.clip_box(), extents.translate(s.dx(), s.dy()))
//
// It is often useful to quickly reject objects that are entirely outside the
// clip box:
//
//   Box bounds =
//     Box::intersect(s.clip_box(), extents.translate(s.dx(), s.dy()));
//   if (bounds.empty()) return;
//
// In order to compute the clipped bounding box in the object's coordinates,
// the transformation can be applied in the other direction:
//
//   Box clipped =
//     Box::intersect(s.clip_box().translace(-s.dx(), -s.dy()), extents);
//
class Surface {
 public:
  Surface(DisplayOutput &out, int16_t dx, int16_t dy, Box clip,
          Color bg = color::Transparent, FillMode fill_mode = FILL_MODE_VISIBLE,
          PaintMode paint_mode = PAINT_MODE_BLEND)
      : out_(&out),
        dx_(dx),
        dy_(dy),
        clip_box_(std::move(clip)),
        bgcolor_(bg),
        fill_mode_(fill_mode),
        paint_mode_(paint_mode) {
    if (bg.a() == 0xFF) {
      paint_mode = PAINT_MODE_REPLACE;
    }
  }

  Surface(DisplayOutput *out, Box clip, Color bg = color::Transparent,
          FillMode fill_mode = FILL_MODE_VISIBLE,
          PaintMode paint_mode = PAINT_MODE_BLEND)
      : out_(out),
        dx_(0),
        dy_(0),
        clip_box_(std::move(clip)),
        bgcolor_(bg),
        fill_mode_(fill_mode),
        paint_mode_(paint_mode) {
    if (bg.a() == 0xFF) {
      paint_mode = PAINT_MODE_REPLACE;
    }
  }

  Surface(Surface &&other) = default;
  Surface(const Surface &other) = default;

  // Returns the device to which to draw.
  DisplayOutput& out() const { return *out_; }

  void set_out(DisplayOutput *out) { out_ = out; }

  // Returns the x offset that should be applied to the drawn object.
  int16_t dx() const { return dx_; }

  // Returns the y offset that should be applied to the drawn object.
  int16_t dy() const { return dy_; }

  // Returns the clip box, in the device coordinates (independent of
  // the x, y offset), that must be respected by the drawn object.
  const Box &clip_box() const { return clip_box_; }

  void set_clip_box(const Box& clip_box) { clip_box_ = clip_box; }

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

  // Returns the paint mode that should generally be passed to the device
  // when the object is drawn. The object may overwrite the paint mode
  // (with PAINT_MODE_REPLACE) only if it is certain that all pixels it
  // writes are completely opaque. Note that in case when the background
  // is specified and completely non-opaque, the paint mode is automatically
  // set to PAINT_MODE_REPLACE.
  PaintMode paint_mode() const { return paint_mode_; }

  void set_paint_mode(PaintMode paint_mode) { paint_mode_ = paint_mode; }

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

  DisplayOutput& output() const { return *out_; }
  Box extents() const { return clip_box_; }
  void nest() const {}
  void unnest() const {}
  const Rasterizable* getRasterizableBackground() const { return nullptr; }
  Color getBgColor() const { return bgcolor_; }

  DisplayOutput *out_;
  int16_t dx_;
  int16_t dy_;
  Box clip_box_;
  Color bgcolor_;
  FillMode fill_mode_;
  PaintMode paint_mode_;
};

// Interface implemented by any class that represents things (like images in
// various formats) that can be drawn to an output device (such as a TFT
// screen, or an off-screen buffer).
//
// Note: each drawable object internally decides on its PaintMode (e.g.
// 'replace' vs 'alpha-blend').
//
// Implementations must override two methods:
// * either drawTo or drawInteriorTo (see below), to implement the actual
//   drawing logic;
// * extents(), returning a bounding box which the drawing fits into,
//   assuming it were drawn at (0, 0).
class Drawable {
 public:
  virtual ~Drawable() {}
  virtual Box extents() const = 0;

  // A singleton, representing a 'no-op' drawable with no bounding box.
  static const Drawable *Empty();

 private:
  friend void Surface::drawObject(const Drawable &object) const;

  // Draws this object's content, filling the entire (clipped) extents()
  // rectangle. The default implementation fills the clipped extents() rectangle
  // with bgcolor (if it is not transparent), and then calls drawInteriorTo. If
  // you override this method, rather than drawInteriorTo, you're responsible
  // for filling up the entire extents() rectangle, using bgcolor for
  // transparent parts, unless bgcolor is itself fully transparent.
  //
  // The implementation MUST respect the surface's parameters, particularly
  // the clip_box - i.e. it is not allowed to draw to output outside of the
  // clip_box.
  virtual void drawTo(const Surface &s) const;

  // Draws this object's content without necessarily filling up the entire
  // extents() rectangle.
  // If you override this method, the default implementation of drawTo will
  // first fill the extents() rectangle with a background color if provided. It
  // will work, but it will generally cause flicker. To avoid it, override
  // drawTo() method instead.
  //
  // The implementation MUST respect the surface's parameters, particularly
  // the clip_box - i.e. it is not allowed to draw to output outside of the
  // clip_box.
  virtual void drawInteriorTo(const Surface &s) const {}
};

inline void Surface::drawObject(const Drawable &object) const {
  object.drawTo(*this);
}

}  // namespace roo_display