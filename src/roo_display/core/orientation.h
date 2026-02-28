#pragma once

#include <assert.h>
#include <inttypes.h>

namespace roo_display {

/// Represents the orientation of a display device.
///
/// A display has a natural physical placement defining absolute up/down/left/
/// right. `Orientation` maps logical (x, y) coordinates to those directions.
/// Examples:
/// - RightDown: x increases left-to-right, y increases top-to-bottom (default).
/// - UpRight: x increases bottom-to-top, y increases left-to-right.
///
/// Internally, orientation is stored as a byte and can be passed by value.
/// There are 8 possible values, which can be viewed as:
/// - 4 rotations (90-degree steps) with optional mirror flip
/// - 3 binary parameters: x/y swapped, horizontal direction, vertical
///   direction.
///
/// Helper methods are provided for common transformations.
class Orientation {
 public:
  /// Horizontal direction of the display.
  enum class HorizontalDirection { kLeftToRight = 0, kRightToLeft = 1 };

  /// Vertical direction of the display.
  enum class VerticalDirection { kTopToBottom = 0, kBottomToTop = 1 };

  constexpr Orientation() : orientation_(0) {}

  /// Return the default orientation (RightDown).
  static constexpr Orientation Default() { return RightDown(); }

  /// Return a specific orientation value.
  ///
  /// Each static factory describes how logical x/y axes map to physical
  /// directions, using the naming convention {x-direction}{y-direction}.
  /// For example, RightDown means x increases to the right and y increases
  /// downward.
  static constexpr Orientation RightDown() { return Orientation(0); }
  /// Return orientation where x increases downward and y increases to the
  /// right.
  static constexpr Orientation DownRight() { return Orientation(1); }
  /// Return orientation where x increases to the left and y increases downward.
  static constexpr Orientation LeftDown() { return Orientation(2); }
  /// Return orientation where x increases downward and y increases to the left.
  static constexpr Orientation DownLeft() { return Orientation(3); }
  /// Return orientation where x increases to the right and y increases upward.
  static constexpr Orientation RightUp() { return Orientation(4); }
  /// Return orientation where x increases upward and y increases to the right.
  static constexpr Orientation UpRight() { return Orientation(5); }
  /// Return orientation where x increases to the left and y increases upward.
  static constexpr Orientation LeftUp() { return Orientation(6); }
  /// Return orientation where x increases upward and y increases to the left.
  static constexpr Orientation UpLeft() { return Orientation(7); }

  /// Return the default orientation rotated by `count` * 90 degrees.
  ///
  /// @param count Number of 90-degree clockwise steps (negative allowed).
  static Orientation RotatedByCount(int count) {
    return Default().rotateClockwise(count);
  }

  /// Swap the mapping between x/y and horizontal/vertical directions.
  Orientation swapXY() const { return Orientation(orientation_ ^ 1); }

  /// Return whether x maps to the vertical direction.
  bool isXYswapped() const { return (orientation_ & 1) == 1; }

  /// Return the horizontal direction.
  HorizontalDirection getHorizontalDirection() const {
    return (orientation_ & 2) == 0 ? LEFT_TO_RIGHT : RIGHT_TO_LEFT;
  }

  /// Return whether horizontal direction is left-to-right.
  bool isLeftToRight() const {
    return getHorizontalDirection() == LEFT_TO_RIGHT;
  }

  /// Return whether horizontal direction is right-to-left.
  bool isRightToLeft() const {
    return getHorizontalDirection() == RIGHT_TO_LEFT;
  }

  /// Flip the horizontal direction.
  Orientation flipHorizontally() const { return Orientation(orientation_ ^ 2); }

  /// Return the vertical direction.
  VerticalDirection getVerticalDirection() const {
    return (orientation_ & 4) == 0 ? TOP_TO_BOTTOM : BOTTOM_TO_TOP;
  }

  /// Return whether vertical direction is top-to-bottom.
  bool isTopToBottom() const { return getVerticalDirection() == TOP_TO_BOTTOM; }

  /// Return whether vertical direction is bottom-to-top.
  bool isBottomToTop() const { return getVerticalDirection() == BOTTOM_TO_TOP; }

  /// Flip the vertical direction.
  Orientation flipVertically() const { return Orientation(orientation_ ^ 4); }

  /// Return whether text would appear mirrored in this orientation.
  bool isMirrored() const {
    static const bool mirrored[8] = {0, 1, 1, 0, 1, 0, 0, 1};
    return mirrored[orientation_];
  }

  /// Rotate 90 degrees clockwise.
  Orientation rotateRight() const {
    static const uint8_t successors[8] = {3, 2, 7, 6, 1, 0, 5, 4};
    return Orientation(successors[orientation_]);
  }

  /// Rotate 90 degrees counter-clockwise.
  Orientation rotateLeft() const { return rotateRight().rotateUpsideDown(); }

  /// Rotate 180 degrees.
  Orientation rotateUpsideDown() const {
    // Combination of vertical and horizontal flip.
    return Orientation(orientation_ ^ 0x06);
  }

  /// Rotate clockwise by `turns` * 90 degrees.
  Orientation rotateClockwise(int turns) const {
    switch (turns & 3) {
      case 0:
        return *this;
      case 1:
        return rotateRight();
      case 2:
        return rotateUpsideDown();
      case 3:
        return rotateLeft();
      default:
        assert(false);
        return *this;
    }
  }

  /// Rotate counter-clockwise by `turns` * 90 degrees.
  Orientation rotateCounterClockwise(int turns) const {
    return rotateClockwise(-turns);
  }

  /// Return how many right rotations reach this orientation from default.
  int getRotationCount() const {
    const uint8_t counts[] = {0, 1, 3, 2};
    return counts[orientation_ >> 1];
  }

  /// Flip along the x axis (horizontal or vertical depending on swap).
  Orientation flipX() const {
    return isXYswapped() ? flipVertically() : flipHorizontally();
  }

  /// Flip along the y axis (vertical or horizontal depending on swap).
  Orientation flipY() const {
    return isXYswapped() ? flipHorizontally() : flipVertically();
  }

  /// Return a common name for this orientation.
  const char* asString() const {
    static const char* names[] = {"RightDown", "DownRight", "LeftDown",
                                  "DownLeft",  "RightUp",   "UpRight",
                                  "LeftUp",    "UpLeft"};
    return names[orientation_];
  }

 private:
  friend bool operator==(Orientation a, Orientation b);
  friend bool operator!=(Orientation a, Orientation b);

  constexpr Orientation(uint8_t orientation) : orientation_(orientation) {}

  uint8_t orientation_;
};

inline bool operator==(Orientation a, Orientation b) {
  return a.orientation_ == b.orientation_;
}

inline bool operator!=(Orientation a, Orientation b) {
  return a.orientation_ != b.orientation_;
}

}  // namespace roo_display
