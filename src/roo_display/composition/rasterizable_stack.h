#pragma once

#include <vector>

#include "roo_display/core/device.h"
#include "roo_display/core/drawable.h"
#include "roo_display/core/rasterizable.h"

namespace roo_display {

/// Multi-layer stack of rasterizables composited in order.
///
/// Each input's blending operation covers the entire stack extents. Outside
/// the input's translated, source-clipped extents, kSource, kSourceIn,
/// kSourceOut, kDestinationIn, kDestinationAtop, and kClear reset the
/// accumulated result to color::Transparent; other modes leave it unchanged.
/// Inside those extents, samples use normal blending, including
/// color::Background semantics. A source clip limits available samples, not the
/// blending operation's bounds. Output clipping limits evaluation without
/// changing these rules.
/// Inputs are borrowed and their extents are captured when added or replaced.
/// Keep the stack and its sources alive and unchanged while consuming a stream.
/// Between draws, rebuild or replace inputs after changing source geometry.
/// References returned by addInput()/setInput() are for immediate
/// configuration; additions and reserveInputs() can invalidate them.
/// clearInputs() invalidates all of them. These objects are not synchronized
/// for concurrent mutation.
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

    /// Create an input layer with a signed translation.
    Input(const Rasterizable* obj, Box extents, int16_t dx, int16_t dy)
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

  /// Add an input clipped to `clip_box` in source coordinates.
  Input& addInput(const Rasterizable* input, Box clip_box) {
    inputs_.emplace_back(input, Box::Intersect(input->extents(), clip_box));
    return inputs_.back();
  }

  /// Add an input with an offset.
  Input& addInput(const Rasterizable* input, int16_t dx, int16_t dy) {
    inputs_.emplace_back(input, input->extents(), dx, dy);
    return inputs_.back();
  }

  /// Add an input with a source-coordinate clip box and an offset.
  Input& addInput(const Rasterizable* input, Box clip_box, int16_t dx,
                  int16_t dy) {
    inputs_.emplace_back(input, Box::Intersect(input->extents(), clip_box), dx,
                         dy);
    return inputs_.back();
  }

  /// Replace an existing layer with a borrowed source and signed translation.
  /// Refreshes captured source extents and resets the mode to kSourceOver.
  /// Fails a CHECK for an invalid index; preserves input order and capacity.
  Input& setInput(size_t index, const Rasterizable* input, int16_t dx = 0,
                  int16_t dy = 0) {
    return setInput(index, input, input->extents(), dx, dy);
  }

  /// Replace an existing layer with a source-coordinate clip and translation.
  /// Refreshes captured extents and resets the mode to kSourceOver.
  /// Fails a CHECK for an invalid index; the source is borrowed.
  Input& setInput(size_t index, const Rasterizable* input, Box clip_box,
                  int16_t dx = 0, int16_t dy = 0) {
    CHECK_LT(index, inputs_.size());
    inputs_[index] =
        Input(input, Box::Intersect(input->extents(), clip_box), dx, dy);
    return inputs_[index];
  }

  /// Remove all inputs while preserving allocated storage for reuse.
  void clearInputs() { inputs_.clear(); }

  /// Reserve storage for at least `capacity` inputs.
  void reserveInputs(size_t capacity) { inputs_.reserve(capacity); }

  /// Return the current number of inputs.
  size_t inputCount() const { return inputs_.size(); }

  /// Return the overall extents of the stack.
  Box extents() const override { return extents_; }

  Box anchorExtents() const override { return anchor_extents_; }

  /// Return kNone when coverage and current source hints prove every pixel
  /// opaque; otherwise return kFull. This query does not read source pixels,
  /// allocate storage, or retain metadata across source changes.
  TransparencyMode getTransparencyMode() const override;

  /// Create a stream for the full stack.
  /// Uses compiled streaming for larger outputs with at most 16 inputs;
  /// otherwise uses bounded raster reads. There is no registered-input limit.
  std::unique_ptr<PixelStream> createStream() const override;

  /// Create a stream for a clipped box.
  /// Uses the same optional compiled path as createStream(), without imposing
  /// an input limit. The clip is intersected with the stack extents.
  std::unique_ptr<PixelStream> createStream(const Box& clip_box) const override;

  /// Return the envelope of nonempty input extents, or an empty box.
  /// Empty inputs still retain their blending effects within the stack.
  Box naturalExtents() const {
    Box result(0, 0, -1, -1);
    for (const Input& input : inputs_) {
      if (input.extents().empty()) continue;
      result = result.empty() ? input.extents()
                              : Box::Extent(result, input.extents());
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

  /// Read a rectangle using at most 64 colors of layer scratch per tile.
  /// Large requests are tiled; scratch also grows with nested call depth.
  /// The caller supplies storage for the entire rectangle.
  bool readColorRect(int16_t xMin, int16_t yMin, int16_t xMax, int16_t yMax,
                     Color* result) const override;

  bool readUniformColorRect(int16_t xMin, int16_t yMin, int16_t xMax,
                            int16_t yMax, Color* result) const override;

 private:
  void drawTo(const Surface& surface) const override;

  Box extents_;
  Box anchor_extents_;
  std::vector<Input> inputs_;
};

}  // namespace roo_display