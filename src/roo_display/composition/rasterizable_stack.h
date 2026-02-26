#pragma once

#include <vector>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/rasterizable.h"

namespace roo_display {

/// Multi-layer stack of rasterizables composited in order.
class RasterizableStack : public Rasterizable {
 public:
  /// An input layer in the stack.
  class Input {
   public:
    /// Create an input layer using the source extents.
    Input(const Rasterizable* obj, Box extents)
        : obj_(obj),
          extents_(extents),
          dx_(0),
          dy_(0),
          blending_mode_(BlendingMode::kSourceOver) {}

    /// Create an input layer with an offset.
    Input(const Rasterizable* obj, Box extents, uint16_t dx, uint16_t dy)
        : obj_(obj),
          extents_(extents.translate(dx, dy)),
          dx_(dx),
          dy_(dy),
          blending_mode_(BlendingMode::kSourceOver) {}

    /// Return extents in stack coordinates.
    const Box& extents() const { return extents_; }

    /// X offset applied to the input.
    int16_t dx() const { return dx_; }
    /// Y offset applied to the input.
    int16_t dy() const { return dy_; }

    /// Source rasterizable.
    const Rasterizable* source() const { return obj_; }

    /// Blending mode used for this input.
    BlendingMode blending_mode() const { return blending_mode_; }

    /// Set blending mode for this input.
    Input& withMode(BlendingMode mode) {
      blending_mode_ = mode;
      return *this;
    }

   private:
    const Rasterizable* obj_;
    Box extents_;  // After translation, i.e. in the stack coordinates.
    int16_t dx_;
    int16_t dy_;
    BlendingMode blending_mode_;
  };

  /// Create a stack with the given extents.
  RasterizableStack(const Box& extents)
      : extents_(extents), anchor_extents_(extents) {}

  /// Add an input using its full extents.
  Input& addInput(const Rasterizable* input) {
    inputs_.emplace_back(input, input->extents());
    return inputs_.back();
  }

  /// Add an input clipped to `clip_box`.
  Input& addInput(const Rasterizable* input, Box clip_box) {
    inputs_.emplace_back(input, Box::Intersect(input->extents(), clip_box));
    return inputs_.back();
  }

  /// Add an input with an offset.
  Input& addInput(const Rasterizable* input, uint16_t dx, uint16_t dy) {
    inputs_.emplace_back(input, input->extents(), dx, dy);
    return inputs_.back();
  }

  /// Add an input with an offset and clip box.
  Input& addInput(const Rasterizable* input, Box clip_box, uint16_t dx,
                  uint16_t dy) {
    inputs_.emplace_back(input, Box::Intersect(input->extents(), clip_box), dx,
                         dy);
    return inputs_.back();
  }

  /// Return the overall extents of the stack.
  Box extents() const override { return extents_; }

  Box anchorExtents() const override { return anchor_extents_; }

  /// Return minimal extents that fit all inputs without clipping.
  Box naturalExtents() {
    if (inputs_.empty()) return Box(0, 0, -1, -1);
    Box result = inputs_[0].extents();
    for (size_t i = 1; i < inputs_.size(); i++) {
      result = Box::Extent(result, inputs_[i].extents());
    }
    return result;
  }

  /// Set the stack extents.
  void setExtents(const Box& extents) { extents_ = extents; }

  /// Set anchor extents used for alignment.
  void setAnchorExtents(const Box& anchor_extents) {
    anchor_extents_ = anchor_extents;
  }

  void readColors(const int16_t* x, const int16_t* y, uint32_t count,
                  Color* result) const override;

  bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                     Color* result) const override;

 private:
  Box extents_;
  Box anchor_extents_;
  std::vector<Input> inputs_;
};

}  // namespace roo_display