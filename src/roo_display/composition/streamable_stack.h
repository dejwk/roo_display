#pragma once

#include <vector>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/streamable.h"

namespace roo_display {

/// Multi-layer stack of streamables composited in order.
///
/// Each input's blending operation covers the entire stack extents. Outside
/// the input's translated, source-clipped extents, kSource, kSourceIn,
/// kSourceOut, kDestinationIn, kDestinationAtop, and kClear reset the
/// accumulated result to color::Transparent; other modes leave it unchanged.
/// Inside those extents, samples use normal blending, including
/// color::Background semantics. A source clip limits available samples, not the
/// blending operation's bounds. Output clipping limits evaluation without
/// changing these rules.
///
/// Drawing and stream creation for nonempty output support at most kMaxInputs
/// registered inputs, including clipped-out inputs. Exceeding this limit fails
/// a release-enabled CHECK before creating child streams. Empty output is
/// exempt.
/// Inputs are borrowed and their extents are captured when added or replaced.
/// Keep the stack and its sources alive and unchanged while consuming a stream.
/// Between draws, rebuild or replace inputs after changing source geometry.
/// References returned by addInput()/setInput() are for immediate
/// configuration; additions and reserveInputs() can invalidate them.
/// clearInputs() invalidates all of them. These objects are not synchronized
/// for concurrent mutation.
class StreamableStack : public Streamable {
 public:
  /// Maximum registered inputs in a nonempty compiled composition.
  static constexpr size_t kMaxInputs = 16;

  /// An input layer in the stack.
  class Input {
   public:
    /// Create an input layer using the source extents.
    Input(const Streamable* obj, Box extents)
        : obj_(obj),
          extents_(extents),
          dx_(0),
          dy_(0),
          blending_mode_(BlendingMode::kSourceOver) {}

    /// Create an input layer with an offset.
    Input(const Streamable* obj, Box extents, int16_t dx, int16_t dy)
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

    /// Create a stream for the given extents in stack coordinates.
    std::unique_ptr<PixelStream> createStream(const Box& extents) const {
      return obj_->createStream(extents.translate(-dx_, -dy_));
    }

    /// Return the borrowed source.
    const Streamable* source() const { return obj_; }

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

  /// Add an input clipped to `clip_box` in source coordinates.
  Input& addInput(const Streamable* input, Box clip_box) {
    inputs_.emplace_back(input, Box::Intersect(input->extents(), clip_box));
    return inputs_.back();
  }

  /// Add an input with an offset.
  Input& addInput(const Streamable* input, int16_t dx, int16_t dy) {
    inputs_.emplace_back(input, input->extents(), dx, dy);
    return inputs_.back();
  }

  /// Add an input with a source-coordinate clip box and an offset.
  Input& addInput(const Streamable* input, Box clip_box, int16_t dx,
                  int16_t dy) {
    inputs_.emplace_back(input, Box::Intersect(input->extents(), clip_box), dx,
                         dy);
    return inputs_.back();
  }

  /// Replace an existing layer with a borrowed source and signed translation.
  /// Refreshes captured source extents and resets the mode to kSourceOver.
  /// Fails a CHECK for an invalid index; preserves input order and capacity.
  Input& setInput(size_t index, const Streamable* input, int16_t dx = 0,
                  int16_t dy = 0) {
    return setInput(index, input, input->extents(), dx, dy);
  }

  /// Replace an existing layer with a source-coordinate clip and translation.
  /// Refreshes captured extents and resets the mode to kSourceOver.
  /// Fails a CHECK for an invalid index; the source is borrowed.
  Input& setInput(size_t index, const Streamable* input, Box clip_box,
                  int16_t dx = 0, int16_t dy = 0) {
    CHECK_LT(index, inputs_.size());
    inputs_[index] =
        Input(input, Box::Intersect(input->extents(), clip_box), dx, dy);
    return inputs_[index];
  }

  /// Remove all inputs while preserving allocated storage for reuse.
  void clearInputs() { inputs_.clear(); }

  /// Reserve storage for at least @p capacity inputs.
  void reserveInputs(size_t capacity) { inputs_.reserve(capacity); }

  /// Return the number of registered inputs, including clipped-out layers.
  size_t inputCount() const { return inputs_.size(); }

  /// Check the compiler's input capacity for the full output without
  /// allocating.
  bool canCreateStream() const { return canCreateStream(extents_); }

  /// Check input capacity after intersecting a stack-coordinate output clip.
  /// Empty output is always supported. This does not validate source lifetimes
  /// or guarantee allocation success; drawing has the same capacity limit.
  bool canCreateStream(const Box& clip_box) const {
    return inputs_.size() <= kMaxInputs ||
           Box::Intersect(extents_, clip_box).empty();
  }

  /// Return the overall extents of the stack.
  Box extents() const override { return extents_; }

  Box anchorExtents() const override { return anchor_extents_; }

  /// Return minimal extents that fit all inputs without clipping.
  Box naturalExtents() const {
    if (inputs_.empty()) return Box(0, 0, -1, -1);
    Box result = inputs_[0].extents();
    for (size_t i = 1; i < inputs_.size(); i++) {
      result = Box::Extent(result, inputs_[i].extents());
    }
    return result;
  }

  /// Create a stream for the full stack.
  /// Fails a CHECK if nonempty output has more than kMaxInputs registered
  /// inputs.
  std::unique_ptr<PixelStream> createStream() const override;

  /// Create a stream for a clipped box.
  /// Fails a CHECK if nonempty output has more than kMaxInputs registered
  /// inputs, including inputs outside the clip box.
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