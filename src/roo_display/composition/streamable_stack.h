#pragma once

#include <vector>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"

namespace roo_display {

/// Multi-layer stack of streamables composited in order.
class StreamableStack : public Streamable {
 public:
  /// An input layer in the stack.
  class Input {
   public:
    /// Create an input layer using the source extents.
    Input(const Streamable* obj, Box extents)
        : obj_(obj),
          extents_(extents),
          dx_(0),
          dy_(0),
          blending_mode_(BLENDING_MODE_SOURCE_OVER) {}

    /// Create an input layer with an offset.
    Input(const Streamable* obj, Box extents, uint16_t dx, uint16_t dy)
        : obj_(obj),
          extents_(extents.translate(dx, dy)),
          dx_(dx),
          dy_(dy),
          blending_mode_(BLENDING_MODE_SOURCE_OVER) {}

    /// Return extents in stack coordinates.
    const Box& extents() const { return extents_; }

    /// X offset applied to the input.
    int16_t dx() const { return dx_; }
    /// Y offset applied to the input.
    int16_t dy() const { return dy_; }

    /// Create a stream for the given extents in stack coordinates.
    std::unique_ptr<PixelStream> createStream(const Box& extents) const {
      return obj_->createStream(extents.translate(-dx_, -dy_));
    }

    /// Set blending mode for this input.
    Input& withMode(BlendingMode mode) {
      blending_mode_ = mode;
      return *this;
    }

    /// Return blending mode for this input.
    BlendingMode blending_mode() const { return blending_mode_; }

   private:
    const Streamable* obj_;
    Box extents_;  // After translation, i.e. in the stack coordinates.
    int16_t dx_;
    int16_t dy_;
    BlendingMode blending_mode_;
  };

  /// Create a stack with the given extents.
  StreamableStack(const Box& extents)
      : extents_(extents), anchor_extents_(extents) {}

  /// Add an input using its full extents.
  Input& addInput(const Streamable* input) {
    inputs_.emplace_back(input, input->extents());
    return inputs_.back();
  }

  /// Add an input clipped to `clip_box`.
  Input& addInput(const Streamable* input, Box clip_box) {
    inputs_.emplace_back(input, Box::Intersect(input->extents(), clip_box));
    return inputs_.back();
  }

  /// Add an input with an offset.
  Input& addInput(const Streamable* input, uint16_t dx, uint16_t dy) {
    inputs_.emplace_back(input, input->extents(), dx, dy);
    return inputs_.back();
  }

  /// Add an input with an offset and clip box.
  Input& addInput(const Streamable* input, Box clip_box, uint16_t dx,
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
    for (int i = 1; i < inputs_.size(); i++) {
      result = Box::Extent(result, inputs_[i].extents());
    }
    return result;
  }

  /// Create a stream for the full stack.
  std::unique_ptr<PixelStream> createStream() const override;

  /// Create a stream for a clipped box.
  std::unique_ptr<PixelStream> createStream(const Box& clip_box) const override;

  /// Set the stack extents.
  void setExtents(const Box& extents) { extents_ = extents; }

  /// Set anchor extents used for alignment.
  void setAnchorExtents(const Box& anchor_extents) {
    anchor_extents_ = anchor_extents;
  }

 private:
  void drawTo(const Surface& s) const override;

  Box extents_;
  Box anchor_extents_;
  std::vector<Input> inputs_;
};

}  // namespace roo_display