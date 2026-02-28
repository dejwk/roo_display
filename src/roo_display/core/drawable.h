#pragma once

#include "roo_display/color/blending.h"
#include "roo_display/color/color.h"
#include "roo_display/core/box.h"
#include "roo_logging/stream.h"

namespace roo_display {

class DisplayOutput;
class Drawable;

/// Specifies whether a `Drawable` should fill its entire extents box,
/// including fully transparent pixels.
enum class FillMode {
  /// Fill the entire extents box (possibly with fully transparent pixels).
  ///
  /// Useful when drawing over a synthetic background and you want previous
  /// content replaced with that background.
  kExtents = 0,

  /// Fully transparent pixels do not need to be filled.
  kVisible = 1,
};

[[deprecated("Use `FillMode::kExtents` instead.")]]
constexpr FillMode FILL_MODE_RECTANGLE = FillMode::kExtents;
[[deprecated("Use `FillMode::kVisible` instead.")]]
constexpr FillMode FILL_MODE_VISIBLE = FillMode::kVisible;

roo_logging::Stream &operator<<(roo_logging::Stream &os, FillMode mode);

class Rasterizable;

/// Low-level handle used to draw to an underlying device.
///
/// This is passed by the library to `Drawable::drawTo()`; do not create it
/// directly.
///
/// The device uses absolute coordinates starting at (0, 0). To translate an
/// object's extents into device coordinates, apply the surface offsets:
///
///   extents.translate(s.dx(), s.dy())
///
/// Constrain to `clip_box()` when drawing:
///
///   Box::Intersect(s.clip_box(), extents.translate(s.dx(), s.dy()))
///
/// To quickly reject objects outside the clip box:
///
///   Box bounds =
///     Box::Intersect(s.clip_box(), extents.translate(s.dx(), s.dy()));
///   if (bounds.empty()) return;
///
/// To compute clipped bounds in object coordinates:
///
///   Box clipped =
///     Box::Intersect(s.clip_box().translate(-s.dx(), -s.dy()), extents);
///
class Surface {
 public:
  /// Create a surface targeting a device output.
  ///
  /// @param out Output device.
  /// @param dx X offset to apply to drawn objects.
  /// @param dy Y offset to apply to drawn objects.
  /// @param clip Clip box in device coordinates.
  /// @param is_write_once Whether this surface is write-once.
  /// @param bg Background color used for blending.
  /// @param fill_mode Fill behavior for transparent pixels.
  /// @param blending_mode DefaFillMode::kVisiblee.
  Surface(DisplayOutput &out, int16_t dx, int16_t dy, Box clip,
          bool is_write_once, Color bg = color::Transparent,
          FillMode fill_mode = FillMode::kVisible,
          BlendingMode blending_mode = BlendingMode::kSourceOver)
      : out_(&out),
        dx_(dx),
        dy_(dy),
        clip_box_(std::move(clip)),
        is_write_once_(is_write_once),
        bgcolor_(bg),
        fill_mode_(fill_mode),
        blending_mode_(blending_mode) {
    if (bg.a() == 0xFF) {
      blending_mode = BlendingMode::kSource;
    }
  }

  /// Create a surface targeting a device output with no offset.
  ///
  /// @param out Output device.
  /// @param clip Clip box in device coordinates.
  /// @param is_write_once Whether this surface is write-once.
  /// @param bg Background color used for blending.
  /// @param fill_mode Fill behavior for transparent pixels.FillMode::kVisible
  /// @param blending_mode Default blending mode.
  Surface(DisplayOutput *out, Box clip, bool is_write_once,
          Color bg = color::Transparent,
          FillMode fill_mode = FillMode::kVisible,
          BlendingMode blending_mode = BlendingMode::kSourceOver)
      : out_(out),
        dx_(0),
        dy_(0),
        clip_box_(std::move(clip)),
        is_write_once_(is_write_once),
        bgcolor_(bg),
        fill_mode_(fill_mode),
        blending_mode_(blending_mode) {
    if (bg.a() == 0xFF && (blending_mode == BlendingMode::kSourceOver ||
                           blending_mode == BlendingMode::kSourceOverOpaque)) {
      blending_mode = BlendingMode::kSource;
    }
  }

  Surface(Surface &&other) = default;
  Surface(const Surface &other) = default;

  /// Return the device output.
  DisplayOutput &out() const { return *out_; }

  /// Replace the output device pointer.
  void set_out(DisplayOutput *out) { out_ = out; }

  /// Return the x offset to apply to drawn objects.
  int16_t dx() const { return dx_; }

  /// Return the y offset to apply to drawn objects.
  int16_t dy() const { return dy_; }

  /// Return whether this surface is write-once.
  bool is_write_once() const { return is_write_once_; }

  /// Return the clip box in device coordinates (independent of offsets).
  const Box &clip_box() const { return clip_box_; }

  /// Set the clip box in device coordinates.
  void set_clip_box(const Box &clip_box) { clip_box_ = clip_box; }

  /// Return the background color used for blending.
  Color bgcolor() const { return bgcolor_; }

  /// Set the background color used for blending.
  void set_bgcolor(Color bgcolor) { bgcolor_ = bgcolor; }

  /// Return the fill mode the drawable should observe.
  /// FillMode::kVisible
  /// If `FillMode::kExtents`, the drawable must fill its entire (clipped)
  /// extents even if some pixels are completely transparent. If
  /// `FillMode::kVisible`, the drawable may omit fully transparent pixels.
  /// This assumes the appropriate background has been pre-applied.
  FillMode fill_mode() const { return fill_mode_; }

  /// Set the fill mode for drawables.
  void set_fill_mode(FillMode fill_mode) { fill_mode_ = fill_mode; }

  /// Return the default blending mode for drawing.
  ///
  /// If the mode is `BlendingMode::kSourceOver`, a drawable may replace it
  /// with `BlendingMode::kSource` when all pixels it writes are fully opaque.
  /// If an opaque background is specified, `BlendingMode::kSourceOver` is
  /// automatically replaced with `BlendingMode::kSource`.
  BlendingMode blending_mode() const { return blending_mode_; }

  /// Set the default blending mode.
  void set_blending_mode(BlendingMode blending_mode) {
    blending_mode_ = blending_mode;
  }

  /// Set the x offset applied to drawables.
  void set_dx(int16_t dx) { dx_ = dx; }
  /// Set the y offset applied to drawables.
  void set_dy(int16_t dy) { dy_ = dy; }

  /// Draw a drawable object to this surface.
  ///
  /// Intended for custom `Drawable` implementations to draw child objects.
  inline void drawObject(const Drawable &object) const;

  /// Draw a drawable object with an additional offset.
  ///
  /// Convenience wrapper around `drawObject()`.
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

  /// Clip the given extents to the surface clip box.
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

/// Interface for objects that can be drawn to an output device.
///
/// Examples include images, shapes, and UI widgets. Some drawables also
/// implement Streamable (including Rasterizable types). Streamable and
/// Rasterizable objects can be used in specialized contexts that generic
/// drawables cannot, such as backgrounds, overlays, and streamable or
/// rasterizable stacks.
///
/// Implementations must:
/// - Override `extents()` to return the bounding box at (0, 0).
/// - Override either `drawTo()` or `drawInteriorTo()` to implement drawing.
class Drawable {
 public:
  virtual ~Drawable() {}

  /// Return the bounding box encompassing all pixels that need to be drawn.
  ///
  /// This method is called during a transaction and must not block or perform
  /// I/O.
  virtual Box extents() const = 0;

  /// Return the bounds used for alignment.
  ///
  /// Defaults to `extents()`. Some drawables (notably text labels) may want
  /// different alignment bounds.
  ///
  /// This method is called during a transaction and must not block or perform
  /// I/O.
  virtual Box anchorExtents() const { return extents(); }

  /// A singleton representing a no-op drawable with no bounding box.
  ///
  /// @return Pointer to a shared empty drawable instance.
  static const Drawable *Empty();

 private:
  friend void Surface::drawObject(const Drawable &object) const;

  /// Draw this object's content, respecting the fill mode.
  ///
  /// If `s.fill_mode() == FillMode::kExtents`, the method must fill the entire
  /// (clipped) `extents()` rectangle (using `s.bgcolor()` for transparent
  /// parts).
  ///
  /// The default implementation fills the clipped `extents()` rectangle with
  /// `bgcolor` and then calls `drawInteriorTo()`. That can cause flicker, so
  /// override this method if a better implementation is possible.
  ///
  /// The implementation must respect surface parameters, particularly
  /// `clip_box()`, and must not draw outside it. The surface clip box is
  /// pre-clipped to fit within this drawable's `extents()`.
  virtual void drawTo(const Surface &s) const;

  /// Draw this object's content, ignoring `s.fill_mode()`. See `drawTo()`.
  ///
  /// The implementation must respect surface parameters, particularly
  /// `clip_box()`, and must not draw outside it.
  virtual void drawInteriorTo(const Surface &s) const {}
};

inline void Surface::drawObject(const Drawable &object) const {
  Surface s = *this;
  if (s.clipToExtents(object.extents()) == Box::CLIP_RESULT_EMPTY) return;
  object.drawTo(s);
}

}  // namespace roo_display