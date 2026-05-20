#pragma once

namespace roo_display {
namespace internal {

enum RectColor {
  NON_UNIFORM = 0,
  TRANSPARENT = 1,
  INTERIOR = 2,
  OUTLINE_ACTIVE = 3,
  OUTLINE_INACTIVE = 4,
};

}  // namespace internal
}  // namespace roo_display