#pragma once

#include <vector>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"

namespace roo_display {

namespace internal {

class Input {
 public:
  Input(const Streamable* obj) : obj_(obj), extents_(obj->extents()) {}

  Input(const Streamable* obj, uint16_t dx, uint16_t dy)
      : obj_(obj), extents_(obj->extents().translate(dx, dy)) {}

  const Box& extents() const { return extents_; }

  std::unique_ptr<PixelStream> createStream() const {
    return obj_->createStream();
  }

 private:
  const Streamable* obj_;
  Box extents_;
};

}  // namespace internal

// Combo represents a multi-layered stack of streamables.
class Combo : public Drawable {
 public:
  // creates new Combo with the given extents.
  Combo(const Box& extents) : extents_(extents) {}

  // Adds a new input to the combo.
  void addInput(const Streamable* input) { inputs_.emplace_back(input); }

  // Adds a new input to the combo, with the specified offset.
  void addInput(const Streamable* input, uint16_t dx, uint16_t dy) {
    inputs_.emplace_back(input, dx, dy);
  }

  // Returns the overall extents of the combo.
  Box extents() const override { return extents_; }

  // Returns minimal extents that will fit all components without clipping.
  Box naturalExtents() {
    if (inputs_.empty()) return Box(0, 0, -1, -1);
    Box result = inputs_[0].extents();
    for (int i = 1; i < inputs_.size(); i++) {
      result = Box::Extent(result, inputs_[i].extents());
    }
    return result;
  }

  // Overrides this combo's overall extents.
  void set_extents(const Box& extents) { extents_ = extents; }

 private:
  void drawTo(const Surface& s) const override;

  Box extents_;
  std::vector<internal::Input> inputs_;
};

}  // namespace roo_display