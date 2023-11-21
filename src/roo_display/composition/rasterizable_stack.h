#pragma once

#include <vector>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/rasterizable.h"

namespace roo_display {

// RasterizableStack represents a multi-layered stack of rasterizables.
class RasterizableStack : public Rasterizable {
 public:
  class Input {
   public:
    Input(const Rasterizable* obj, Box extents)
        : obj_(obj),
          extents_(extents),
          dx_(0),
          dy_(0),
          blending_mode_(BLENDING_MODE_SOURCE_OVER) {}

    Input(const Rasterizable* obj, Box extents, uint16_t dx, uint16_t dy)
        : obj_(obj),
          extents_(extents.translate(dx, dy)),
          dx_(dx),
          dy_(dy),
          blending_mode_(BLENDING_MODE_SOURCE_OVER) {}

    const Box& extents() const { return extents_; }

    int16_t dx() const { return dx_; }
    int16_t dy() const { return dy_; }

    const Rasterizable* source() const { return obj_; }

    BlendingMode blending_mode() const { return blending_mode_; }

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

  // creates new RasterizableStack with the given extents.
  RasterizableStack(const Box& extents)
      : extents_(extents), anchor_extents_(extents) {}

  // Adds a new input to the stack.
  Input& addInput(const Rasterizable* input) {
    inputs_.emplace_back(input, input->extents());
    return inputs_.back();
  }

  // Adds a new clipped input to the stack.
  Input& addInput(const Rasterizable* input, Box clip_box) {
    inputs_.emplace_back(input, Box::Intersect(input->extents(), clip_box));
    return inputs_.back();
  }

  // Adds a new input to the stack, with the specified offset.
  Input& addInput(const Rasterizable* input, uint16_t dx, uint16_t dy) {
    inputs_.emplace_back(input, input->extents(), dx, dy);
    return inputs_.back();
  }

  // Adds a new clipped input to the stack, with the specified offset.
  Input& addInput(const Rasterizable* input, Box clip_box, uint16_t dx,
                  uint16_t dy) {
    inputs_.emplace_back(input, Box::Intersect(input->extents(), clip_box), dx,
                         dy);
    return inputs_.back();
  }

  // Returns the overall extents of the stack.
  Box extents() const override { return extents_; }

  Box anchorExtents() const override { return anchor_extents_; }

  // Returns minimal extents that will fit all components without clipping.
  Box naturalExtents() {
    if (inputs_.empty()) return Box(0, 0, -1, -1);
    Box result = inputs_[0].extents();
    for (int i = 1; i < inputs_.size(); i++) {
      result = Box::Extent(result, inputs_[i].extents());
    }
    return result;
  }

  // Overrides this stack's extents.
  void setExtents(const Box& extents) { extents_ = extents; }

  // Sets this stack's anchor extents.
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