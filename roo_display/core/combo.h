#pragma once

#include <vector>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"

namespace roo_display {

namespace internal {

class Input {
 public:
  Input(const Streamable* obj, Box extents)
      : obj_(obj), extents_(extents), dx_(0), dy_(0) {}

  Input(const Streamable* obj, Box extents, uint16_t dx, uint16_t dy)
      : obj_(obj), extents_(extents.translate(dx, dy)), dx_(dx), dy_(dy) {}

  const Box& extents() const { return extents_; }

  std::unique_ptr<PixelStream> createStream() const {
    return obj_->createStream(extents_.translate(-dx_, -dy_));
  }

 private:
  const Streamable* obj_;
  Box extents_;
  int16_t dx_;
  int16_t dy_;
};

}  // namespace internal

// Combo represents a multi-layered stack of streamables.
class Combo : public Streamable {
 public:
  // creates new Combo with the given extents.
  Combo(const Box& extents) : extents_(extents), anchor_extents_(extents) {}

  // Adds a new input to the combo.
  void addInput(const Streamable* input) {
    inputs_.emplace_back(input, input->extents());
  }

  // Adds a new clipped input to the combo.
  void addInput(const Streamable* input, Box clip_box) {
    inputs_.emplace_back(input, Box::Intersect(input->extents(), clip_box));
  }

  // Adds a new input to the combo, with the specified offset.
  void addInput(const Streamable* input, uint16_t dx, uint16_t dy) {
    inputs_.emplace_back(input, input->extents(), dx, dy);
  }

  // Adds a new clipped input to the combo, with the specified offset.
  void addInput(const Streamable* input, Box clip_box, uint16_t dx,
                uint16_t dy) {
    inputs_.emplace_back(input, Box::Intersect(input->extents(), clip_box), dx,
                         dy);
  }

  // Returns the overall extents of the combo.
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

  std::unique_ptr<PixelStream> createStream() const override;

  std::unique_ptr<PixelStream> createStream(const Box& clip_box) const override;

  // Overrides this combo's extents.
  void setExtents(const Box& extents) { extents_ = extents; }

  // Sets this combo's anchor extents.
  void setAnchorExtents(const Box& anchor_extents) {
    anchor_extents_ = anchor_extents;
  }

 private:
  void drawTo(const Surface& s) const override;

  Box extents_;
  Box anchor_extents_;
  std::vector<internal::Input> inputs_;
};

}  // namespace roo_display