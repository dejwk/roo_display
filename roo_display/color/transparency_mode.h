#pragma once

namespace roo_display {

enum TransparencyMode {
  TRANSPARENCY_NONE,    // All colors are fully opaque.
  TRANSPARENCY_BINARY,  // Colors are either fully opaque or fully transparent.
  TRANSPARENCY_GRADUAL  // Colors may include partial transparency (alpha
                        // channel).
};

}  // namespace roo_display
