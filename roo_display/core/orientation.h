#pragma once

#include <assert.h>
#include <inttypes.h>

namespace roo_display {

// Represents an orientation of a display device.
//
// A display device is assumed to have a 'natural' physical placement,
// determining in absolute terms which way is 'up', 'down', 'left', and 'right'.
// Orientation (represented by this class) clarifies how the logical (x, y)
// coordinates are mapped to those physical directions. For example:
// * RightDown: the x coordinate goes left-to-right, and the y coordinate goes
//   top-to-bottom. This is a 'default' orientation.
// * UpRight: the x coordinate goes bottom-to-up, and y goes left to right
// * etc.
//
// Internally, orientation is represented as a single byte value. The
// orientation should be passed by value.
//
// Orientation can effectively take one of 8 values, which can be thought of as
// either:
// * 4x 90-degree rotations, each optionally mirror-flipped
// * 3 independent binary parameters:
//   * whether the horizontal dimension corresponds to x or y,
//   * whether the horixontal axis is left-to-right vs right-to-left,
//   * whether the vertical axis is top-to-bottom vs bottom-to-top.
//
// Helper methods are provided to transform (re-orient) orientations.
class Orientation {
 public:
  enum HorizontalDirection { LEFT_TO_RIGHT = 0, RIGHT_TO_LEFT = 1 };

  enum VerticalDirection { TOP_TO_BOTTOM = 0, BOTTOM_TO_TOP = 1 };

  Orientation() : orientation_(0) {}

  // Returns a default orientation, (x, y) -> (left-to-right, top-to-bottom).
  static Orientation Default() { return RightDown(); }

  // Returns a specific orientation.
  static Orientation RightDown() { return Orientation(0); }
  static Orientation DownRight() { return Orientation(1); }
  static Orientation LeftDown() { return Orientation(2); }
  static Orientation DownLeft() { return Orientation(3); }
  static Orientation RightUp() { return Orientation(4); }
  static Orientation UpRight() { return Orientation(5); }
  static Orientation LeftUp() { return Orientation(6); }
  static Orientation UpLeft() { return Orientation(7); }

  // Returns an orientation that results from taking a default orientation
  // (RightDown) and rotating it clockwise by 90 degree 'count' times. If count
  // is negative, the rotation is effectively counter-clockwise by abs(count).
  // Rotations with the same value of count % 4 are effectively equivalent.
  static Orientation RotatedByCount(int count) {
    return Default().rotateClockwise(count);
  }

  // Returns an orientation that results from taking this orientation and
  // swapping the mapping between the (x, y) coordinates and the (horizontal,
  // vertical) direction.
  Orientation swapXY() const { return Orientation(orientation_ ^ 1); }

  // Returns whether the 'x' coordinate corresponds to the vertical direction
  // (up-down), rather than the horizontal direction (left-right).
  bool isXYswapped() const { return (orientation_ & 1) == 1; }

  // Returns whether the horizontal direction of this orientation is
  // left-to-right or right-to-left.
  HorizontalDirection getHorizontalDirection() const {
    return (orientation_ & 2) == 0 ? LEFT_TO_RIGHT : RIGHT_TO_LEFT;
  }

  // Returns whether the horizontal direction of this orientation is
  // left-to-right.
  bool isLeftToRight() const {
    return getHorizontalDirection() == LEFT_TO_RIGHT;
  }

  // Returns whether the horizontal direction of this orientation is
  // right-to-left.
  bool isRightToLeft() const {
    return getHorizontalDirection() == RIGHT_TO_LEFT;
  }

  // Returns an orientation that results from taking this orientation and
  // flipping its horizontal direction, from left-to-right to right-to-left and
  // vice versa.
  Orientation flipHorizontally() const { return Orientation(orientation_ ^ 2); }

  // Returns whether the vertical direction of this orientation is
  // top-to-bottom or bottom-to-top.
  VerticalDirection getVerticalDirection() const {
    return (orientation_ & 4) == 0 ? TOP_TO_BOTTOM : BOTTOM_TO_TOP;
  }

  // Returns whether the vertical direction of this orientation is
  // top-to-bottom.
  bool isTopToBottom() const { return getVerticalDirection() == TOP_TO_BOTTOM; }

  // Returns whether the vertical direction of this orientation is
  // bottom-to-top.
  bool isBottomToTop() const { return getVerticalDirection() == BOTTOM_TO_TOP; }

  // Returns an orientation that results from taking this orientation and
  // flipping its vertical direction, from top-to-bottom to bottom-to-top and
  // vice versa.
  Orientation flipVertically() const { return Orientation(orientation_ ^ 4); }

  // Returns whether the text rendered in this orientation would appear
  // mirrored.
  bool isMirrored() const {
    static const bool mirrored[8] = {0, 1, 1, 0, 1, 0, 0, 1};
    return mirrored[orientation_];
  }

  // Returns an orientation that results from taking this orientation and
  // rotating it clockwise by 90 degrees.
  Orientation rotateRight() const {
    static const uint8_t successors[8] = {3, 2, 7, 6, 1, 0, 5, 4};
    return Orientation(successors[orientation_]);
  }

  // Returns an orientation that results from taking this orientation and
  // rotating it counter-clockwise by 90 degrees.
  Orientation rotateLeft() const { return rotateRight().rotateUpsideDown(); }

  // Returns an orientation that results from taking this orientation and
  // rotating it by 180 degrees.
  Orientation rotateUpsideDown() const {
    // Combination of vertical and horizontal flip.
    return Orientation(orientation_ ^ 0x06);
  }

  // Returns an orientation that results from taking this orientation and
  // rotating it clockwise by turns * 90 degrees.
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
    }
  }

  // Returns an orientation that results from taking this orientation and
  // rotating it counter-clockwise by turns * 90 degrees.
  Orientation rotateCounterClockwise(int turns) const {
    return rotateClockwise(-turns);
  }

  // Returns how many 'right' rotations would need to be performed,
  // starting either from Default() (if not mirrored) or
  // Default().swapXY() (if mirrored), to reach this orientation.
  int getRotationCount() const {
    const uint8_t counts[] = {0, 1, 3, 2};
    return counts[orientation_ >> 1];
  }

  // Flips the orientation along the x dimension. Results in either
  // horizontal or vertical flip, depending on the current mapping
  // of (x, y) to physical directions, as determined by isXYswapped().
  Orientation flipX() const {
    return isXYswapped() ? flipVertically() : flipHorizontally();
  }

  // Flips the orientation along the y dimension. Results in either
  // vertical or horizontal flip, depending on the current mapping
  // of (x, y) to physical directions, as determined by isXYswapped().
  Orientation flipY() const {
    return isXYswapped() ? flipHorizontally() : flipVertically();
  }

  // Returns a common name of this orientation, for convenience.
  const char* asString() const {
    static const char* names[] = {"RightDown", "DownRight", "LeftDown",
                                  "DownLeft",  "RightUp",   "UpRight",
                                  "LeftUp",    "UpLeft"};
    return names[orientation_];
  }

 private:
  friend bool operator==(Orientation a, Orientation b);
  friend bool operator!=(Orientation a, Orientation b);

  Orientation(uint8_t orientation) : orientation_(orientation) {}

  uint8_t orientation_;
};

inline bool operator==(Orientation a, Orientation b) {
  return a.orientation_ == b.orientation_;
}

inline bool operator!=(Orientation a, Orientation b) {
  return a.orientation_ != b.orientation_;
}

}  // namespace roo_display
