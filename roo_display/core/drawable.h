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

// Low-level 'handle' to draw to an underlying device. Passed by the library
// to Drawable.draw() (see below). Don't create directly.
class Surface {
 public:
  Surface(DisplayOutput *out, int16_t dx, int16_t dy, Box clip,
          Color bg = color::Transparent, FillMode fill_mode = FILL_MODE_VISIBLE,
          PaintMode paint_mode = PAINT_MODE_BLEND)
      : out(out),
        dx(dx),
        dy(dy),
        clip_box(std::move(clip)),
        bgcolor(bg),
        fill_mode(fill_mode),
        paint_mode(paint_mode) {
    if (bg.a() != 0) {
      fill_mode = FILL_MODE_RECTANGLE;
    }
  }

  Surface(DisplayOutput *out, Box clip, Color bg = color::Transparent,
          FillMode fill_mode = FILL_MODE_VISIBLE,
          PaintMode paint_mode = PAINT_MODE_BLEND)
      : out(out),
        dx(0),
        dy(0),
        clip_box(std::move(clip)),
        bgcolor(bg),
        fill_mode(fill_mode),
        paint_mode(paint_mode) {
    if (bg.a() != 0) {
      fill_mode = FILL_MODE_RECTANGLE;
    }
  }

  Surface(Surface &&other) = default;
  Surface(const Surface &other) = default;

  // Draws the specified drawable object to this surface. Intended for custom
  // implementations of Drawable (below), to draw other objects as part of
  // drawing itself.
  inline void drawObject(const Drawable &object) const;

  Box::ClipResult clipToExtents(Box extents) {
    return clip_box.clip(extents.translate(dx, dy));
  }

  DisplayOutput *out;
  int16_t dx;
  int16_t dy;
  Box clip_box;
  Color bgcolor;
  FillMode fill_mode;
  PaintMode paint_mode;
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