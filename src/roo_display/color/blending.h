#pragma once

#include <inttypes.h>

#include <cmath>
#include <type_traits>
#include <utility>

#include "roo_display/color/color.h"
#include "roo_display/color/traits.h"
#include "roo_display/internal/color_io.h"
#include "roo_io/data/byte_order.h"
namespace roo_display {

/// Porter-Duff style blending modes.
enum class BlendingMode {

  /// The new ARGB8888 value completely replaces the old one.
  kSource,

  /// Source is placed (alpha-blended) over the destination. This is the
  /// default blending mode.
  kSourceOver,

  /// The source that overlaps the destination, replaces the destination.
  kSourceIn,

  /// Source which overlaps the destination, replaces the destination.
  /// Destination is placed elsewhere.
  kSourceAtop,

  /// Only the destination will be present.
  kDestination,

  /// Destination is placed over the source.
  kDestinationOver,

  /// Destination which overlaps the source, replaces the source.
  kDestinationIn,

  /// Destination which overlaps the source replaces the source. Source is
  /// placed elsewhere.
  kDestinationAtop,

  /// No regions are enabled.
  kClear,

  /// Source is placed, where it falls outside of the destination.
  kSourceOut,

  /// Destination is placed, where it falls outside of the source.
  kDestinationOut,

  /// The non-overlapping regions of source and destination are combined.
  kXor,

  /// Similar to kSourceOver, but assumes that the destination is
  /// opaque.
  ///
  /// Don't use it directly. It is used internally by the framework, as an
  /// optimization, when it is detected that source-over is performed over an
  /// opaque background.
  kSourceOverOpaque,

  /// Similar to kDestinationOver, but assumes that the source is
  /// opaque.
  kDestinationOverOpaque
};

/// @deprecated Use `BlendingMode::kSource` instead.
constexpr BlendingMode BLENDING_MODE_SOURCE = BlendingMode::kSource;
/// @deprecated Use `BlendingMode::kSourceOver` instead.
constexpr BlendingMode BLENDING_MODE_SOURCE_OVER = BlendingMode::kSourceOver;
/// @deprecated Use `BlendingMode::kSourceIn` instead.
constexpr BlendingMode BLENDING_MODE_SOURCE_IN = BlendingMode::kSourceIn;
/// @deprecated Use `BlendingMode::kSourceAtop` instead.
constexpr BlendingMode BLENDING_MODE_SOURCE_ATOP = BlendingMode::kSourceAtop;
/// @deprecated Use `BlendingMode::kDestination` instead.
constexpr BlendingMode BLENDING_MODE_DESTINATION = BlendingMode::kDestination;
/// @deprecated Use `BlendingMode::kDestinationOver` instead.
constexpr BlendingMode BLENDING_MODE_DESTINATION_OVER =
    BlendingMode::kDestinationOver;
/// @deprecated Use `BlendingMode::kDestinationIn` instead.
constexpr BlendingMode BLENDING_MODE_DESTINATION_IN =
    BlendingMode::kDestinationIn;
/// @deprecated Use `BlendingMode::kDestinationAtop` instead.
constexpr BlendingMode BLENDING_MODE_DESTINATION_ATOP =
    BlendingMode::kDestinationAtop;
/// @deprecated Use `BlendingMode::kClear` instead.
constexpr BlendingMode BLENDING_MODE_CLEAR = BlendingMode::kClear;
/// @deprecated Use `BlendingMode::kSourceOut` instead.
constexpr BlendingMode BLENDING_MODE_SOURCE_OUT = BlendingMode::kSourceOut;
/// @deprecated Use `BlendingMode::kDestinationOut` instead.
constexpr BlendingMode BLENDING_MODE_DESTINATION_OUT =
    BlendingMode::kDestinationOut;
/// @deprecated Use `BlendingMode::kXor` instead.
constexpr BlendingMode BLENDING_MODE_EXCLUSIVE_OR = BlendingMode::kXor;

/// Transparency information for a stream or color mode.
enum class TransparencyMode {
  /// All colors are fully opaque.
  kNone,
  /// Colors are either fully opaque or fully transparent.
  kCrude,
  /// Colors may include partial transparency (alpha channel).
  kFull,
};

/// @deprecated Use `TransparencyMode::kNone` instead.
constexpr TransparencyMode TRANSPARENCY_NONE = TransparencyMode::kNone;
/// @deprecated Use `TransparencyMode::kCrude` instead.
constexpr TransparencyMode TRANSPARENCY_BINARY = TransparencyMode::kCrude;
/// @deprecated Use `TransparencyMode::kFull` instead.
constexpr TransparencyMode TRANSPARENCY_GRADUAL = TransparencyMode::kFull;

namespace internal {

// In practice, seems to be measurably faster than actually dividing by 255
// and counting on the compiler to optimize.
inline constexpr uint8_t __div_255(uint16_t arg) { return (arg * 257) >> 16; }

inline constexpr uint8_t __div_255_rounded(uint16_t arg) {
  return __div_255(arg + 128);
}

inline constexpr uint8_t __div_65280(uint32_t arg) {
  return ((arg >> 8) + (arg >> 16)) >> 8;
}

inline constexpr uint8_t __div_65280_rounded(uint32_t arg) {
  return __div_65280(arg + 32640);
}

// Helper to minimize the number of similar switch statements in the code below.
template <typename Functor, typename... Args>
auto BlenderSpecialization(const BlendingMode blending_mode, Args&&... args)
    -> decltype(std::declval<Functor>()
                    .template operator()<BlendingMode::kSourceOver>(args...)) {
  Functor functor;
  if (blending_mode == BlendingMode::kSource) {
    return functor.template operator()<BlendingMode::kSource>(
        std::forward<Args>(args)...);
  } else if (blending_mode == BlendingMode::kSourceOver) {
    return functor.template operator()<BlendingMode::kSourceOver>(
        std::forward<Args>(args)...);
  } else if (blending_mode == BlendingMode::kSourceOverOpaque) {
    return functor.template operator()<BlendingMode::kSourceOverOpaque>(
        std::forward<Args>(args)...);
  } else {
    switch (blending_mode) {
      case BlendingMode::kSourceIn:
        return functor.template operator()<BlendingMode::kSourceIn>(
            std::forward<Args>(args)...);
      case BlendingMode::kSourceAtop:
        return functor.template operator()<BlendingMode::kSourceAtop>(
            std::forward<Args>(args)...);
      case BlendingMode::kDestination:
        return functor.template operator()<BlendingMode::kDestination>(
            std::forward<Args>(args)...);
      case BlendingMode::kDestinationOver:
        return functor.template operator()<BlendingMode::kDestinationOver>(
            std::forward<Args>(args)...);
      case BlendingMode::kDestinationOverOpaque:
        return functor
            .template operator()<BlendingMode::kDestinationOverOpaque>(
                std::forward<Args>(args)...);
      case BlendingMode::kDestinationIn:
        return functor.template operator()<BlendingMode::kDestinationIn>(
            std::forward<Args>(args)...);
      case BlendingMode::kDestinationAtop:
        return functor.template operator()<BlendingMode::kDestinationAtop>(
            std::forward<Args>(args)...);
      case BlendingMode::kClear:
        return functor.template operator()<BlendingMode::kClear>(
            std::forward<Args>(args)...);
      case BlendingMode::kSourceOut:
        return functor.template operator()<BlendingMode::kSourceOut>(
            std::forward<Args>(args)...);
      case BlendingMode::kDestinationOut:
        return functor.template operator()<BlendingMode::kDestinationOut>(
            std::forward<Args>(args)...);
      case BlendingMode::kXor:
        return functor.template operator()<BlendingMode::kXor>(
            std::forward<Args>(args)...);
      default:
        return functor.template operator()<BlendingMode::kSourceOver>(
            std::forward<Args>(args)...);
    }
  }
}

}  // namespace internal

template <BlendingMode blending_mode>
struct BlendOp;

template <>
struct BlendOp<BlendingMode::kSource> {
  inline Color operator()(Color dst, Color src) const { return src; }
};

template <>
struct BlendOp<BlendingMode::kSourceOverOpaque> {
  inline Color operator()(Color dst, Color src) const {
    uint16_t alpha = src.a();
    uint16_t inv_alpha = alpha ^ 0xFF;
    uint8_t r = (uint8_t)(internal::__div_255_rounded(alpha * src.r() +
                                                      inv_alpha * dst.r()));
    uint8_t g = (uint8_t)(internal::__div_255_rounded(alpha * src.g() +
                                                      inv_alpha * dst.g()));
    uint8_t b = (uint8_t)(internal::__div_255_rounded(alpha * src.b() +
                                                      inv_alpha * dst.b()));

    // https://stackoverflow.com/questions/12011081
    // The implementation commented out below is slightly faster (14ms vs 16ms
    // in a microbenchmark) than the implementation above, but it is one-off
    // for some important cases, like fg = 0x00000000 (transparency), and
    // bg = 0xFFFFFFFF (white), which should produce white but produces
    // 0xFFFEFEFE.
    //
    // uint16_t alpha = fgc.a() + 1;
    // uint16_t inv_alpha = 256 - alpha;
    // uint8_t r = (uint8_t)((alpha * fgc.r() + inv_alpha * bgc.r()) >> 8);
    // uint8_t g = (uint8_t)((alpha * fgc.g() + inv_alpha * bgc.g()) >> 8);
    // uint8_t b = (uint8_t)((alpha * fgc.b() + inv_alpha * bgc.b()) >> 8);
    //
    return Color(0xFF000000 | r << 16 | g << 8 | b);
  }
};

#ifndef ROO_DISPLAY_BLENDING_PRECITION
#define ROO_DISPLAY_BLENDING_PRECISION 1
#endif

template <>
struct BlendOp<BlendingMode::kSourceOver> {
  // Fa = 1; Fb = 1 – αs
  // co = αs x Cs + αb x Cb x (1 – αs)
  // αo = αs + αb x (1 – αs)

  inline Color operator()(Color dst, Color src) const {
    uint16_t src_alpha = src.a();
    if (src_alpha == 0xFF) {
      return src;
    }
    uint16_t dst_alpha = dst.a();
    if (dst_alpha == 0xFF) {
      return BlendOp<BlendingMode::kSourceOverOpaque>()(dst, src);
    }
    if (src_alpha == 0) {
      return dst;
    }
    if (dst_alpha == 0) {
      return src;
    }

    // Blends a+b so that, when later applied over c, the result is the same as
    // if they were applied in succession; e.g. c+(a+b) == (c+a)+b.

#if ROO_DISPLAY_BLENDING_PRECISION == 2
    // Microbenchmark timing: 24970 us
    uint32_t a16 =
        (uint16_t)dst_alpha * 255 + (uint16_t)src_alpha * (255 - dst_alpha);
    uint8_t alpha = internal::__div_255_rounded(a16);
    uint16_t cs = (256 * 255 * 255 * src_alpha + a16 / 2 + 1) / a16;
    uint16_t cd = 256 * 255 - cs;
    uint8_t r =
        (uint8_t)(internal::__div_65280_rounded(cs * src.r() + cd * dst.r()));
    uint8_t g =
        (uint8_t)(internal::__div_65280_rounded(cs * src.g() + cd * dst.g()));
    uint8_t b =
        (uint8_t)(internal::__div_65280_rounded(cs * src.b() + cd * dst.b()));

#elif ROO_DISPLAY_BLENDING_PRECISION == 1
    // Microbenchmark timing: 22856 us
    uint16_t tmp = dst_alpha * src_alpha;
    uint16_t alpha = dst_alpha + src_alpha - internal::__div_255_rounded(tmp);
    uint16_t cs = (src_alpha * 255) / alpha;
    uint16_t cd = 255 - cs;

    uint8_t r =
        (uint8_t)(internal::__div_255_rounded(cs * src.r() + cd * dst.r()));
    uint8_t g =
        (uint8_t)(internal::__div_255_rounded(cs * src.g() + cd * dst.g()));
    uint8_t b =
        (uint8_t)(internal::__div_255_rounded(cs * src.b() + cd * dst.b()));

#elif ROO_DISPLAY_BLENDING_PRECISION == 0
    // Microbenchmark timing: 21925 us
    uint16_t tmp = dst_alpha * src_alpha;
    uint16_t alpha = dst_alpha + src_alpha - internal::__div_255_rounded(tmp);
    uint16_t cs = ((src_alpha + 1) << 8) / (alpha + 1);
    uint16_t cd = 256 - cs;

    uint8_t r = (uint8_t)((cs * src.r() + cd * dst.r() + 128) >> 8);
    uint8_t g = (uint8_t)((cs * src.g() + cd * dst.g() + 128) >> 8);
    uint8_t b = (uint8_t)((cs * src.b() + cd * dst.b() + 128) >> 8);

#endif

    return Color(alpha << 24 | r << 16 | g << 8 | b);
  }
};

template <>
struct BlendOp<BlendingMode::kSourceAtop> {
  // Fa = αb; Fb = 1 – αs
  // co = αs x Cs x αb + αb x Cb x (1 – αs)
  // αo = αs x αb + αb x (1 – αs)

  inline Color operator()(Color dst, Color src) const {
    uint16_t src_alpha = src.a();
    uint16_t dst_alpha = dst.a();
    if (src_alpha == 0xFF) {
      return src.withA(dst_alpha);
    }
    if (src_alpha == 0) {
      return dst;
    }
    if (dst_alpha == 0) {
      return color::Background;
    }
    uint16_t src_multi = src_alpha;
    uint16_t dst_multi = 255 - src_alpha;

    uint8_t r = (uint8_t)((src_multi * src.r() + dst_multi * dst.r()) >> 8);
    uint8_t g = (uint8_t)((src_multi * src.g() + dst_multi * dst.g()) >> 8);
    uint8_t b = (uint8_t)((src_multi * src.b() + dst_multi * dst.b()) >> 8);
    return Color(dst_alpha << 24 | r << 16 | g << 8 | b);
  }
};

template <>
struct BlendOp<BlendingMode::kDestination> {
  inline Color operator()(Color dst, Color src) const { return dst; }
};

template <>
struct BlendOp<BlendingMode::kDestinationOverOpaque> {
  inline Color operator()(Color dst, Color src) const {
    return BlendOp<BlendingMode::kSourceOverOpaque>()(src, dst);
  }
};

template <>
struct BlendOp<BlendingMode::kDestinationOver> {
  inline Color operator()(Color dst, Color src) const {
    return BlendOp<BlendingMode::kSourceOver>()(src, dst);
  }
};

template <>
struct BlendOp<BlendingMode::kClear> {
  inline Color operator()(Color dst, Color src) const {
    return dst == color::Transparent && src == color::Transparent
               ? color::Transparent
               : color::Background;
  }
};

template <>
struct BlendOp<BlendingMode::kSourceIn> {
  // Fa = αb; Fb = 0
  // co = αs x Cs x αb
  // αo = αs x αb
  inline Color operator()(Color dst, Color src) const {
    uint16_t dst_alpha = dst.a();
    if (dst_alpha == 0xFF) {
      return src;
    }
    uint16_t src_alpha = src.a();
    if (src_alpha == 0xFF) {
      return src.withA(dst_alpha);
    }
    if (src_alpha == 0) {
      return color::Background;
    }
    return src.withA(internal::__div_255_rounded(src_alpha * dst_alpha));
  }
};

template <>
struct BlendOp<BlendingMode::kDestinationIn> {
  inline Color operator()(Color dst, Color src) const {
    return BlendOp<BlendingMode::kSourceIn>()(src, dst);
  }
};

template <>
struct BlendOp<BlendingMode::kDestinationAtop> {
  inline Color operator()(Color dst, Color src) const {
    return BlendOp<BlendingMode::kSourceAtop>()(src, dst);
  }
};

template <>
struct BlendOp<BlendingMode::kSourceOut> {
  // Fa = 1 – αb; Fb = 0
  // co = αs x Cs x (1 – αb)
  // αo = αs x (1 – αb)
  inline Color operator()(Color dst, Color src) const {
    uint8_t dst_alpha = dst.a();
    if (dst_alpha == 0) {
      return src;
    }
    uint8_t src_alpha = src.a();
    if (src_alpha == 0xFF) {
      return src.withA(dst_alpha ^ 0xFF);
    }
    if (src_alpha == 0) {
      return color::Background;
    }
    return src.withA(
        internal::__div_255_rounded(src_alpha * (dst_alpha ^ 0xFF)));
  }
};

template <>
struct BlendOp<BlendingMode::kDestinationOut> {
  inline Color operator()(Color dst, Color src) const {
    return BlendOp<BlendingMode::kSourceOut>()(src, dst);
  }
};

template <>
struct BlendOp<BlendingMode::kXor> {
  // Fa = 1 - αb; Fb = 1 – αs
  // co = αs x Cs x (1 - αb) + αb x Cb x (1 – αs)
  // αo = αs x (1 - αb) + αb x (1 – αs)
  inline Color operator()(Color dst, Color src) const {
    uint16_t src_alpha = src.a();
    uint16_t dst_alpha = dst.a();
    if (src_alpha == 0xFF) {
      return src.withA(255 - dst_alpha);
    } else if (src_alpha == 0) {
      return dst;
    } else if (dst_alpha == 0xFF) {
      return dst.withA(255 - src_alpha);
    } else if (dst_alpha == 0) {
      return src;
    }
    uint16_t alpha = internal::__div_255_rounded(src_alpha * (255 - dst_alpha) +
                                                 dst_alpha * (255 - src_alpha));
    uint16_t cs = (src_alpha * (255 - dst_alpha) + alpha / 2) / alpha;
    uint16_t cd = 255 - cs;

    uint8_t r =
        (uint8_t)(internal::__div_255_rounded(cs * src.r() + cd * dst.r()));
    uint8_t g =
        (uint8_t)(internal::__div_255_rounded(cs * src.g() + cd * dst.g()));
    uint8_t b =
        (uint8_t)(internal::__div_255_rounded(cs * src.b() + cd * dst.b()));
    return Color(alpha << 24 | r << 16 | g << 8 | b);
  }
};

// Utility function to calculates alpha-blending of the foreground color (fgc)
// over the background color (bgc), ignoring background color's alpha, as if it
// is fully opaque.
inline Color AlphaBlendOverOpaque(Color bgc, Color fgc) {
  return BlendOp<BlendingMode::kSourceOverOpaque>()(bgc, fgc);
}

template <BlendingMode mode>
struct Blender {
  inline Color apply(Color dst, Color src) { return BlendOp<mode>()(dst, src); }

  inline void applyInPlace(Color* dst, const Color* src, int16_t count) {
    BlendOp<mode> op;
    while (count-- > 0) {
      *dst = op(*dst, *src);
      ++dst;
      ++src;
    }
  }

  inline void applySingleSourceInPlace(Color* dst, Color src, int16_t count) {
    BlendOp<mode> op;
    while (count-- > 0) {
      *dst = op(*dst, src);
      ++dst;
    }
  }

  inline void applyInPlaceIndexed(Color* dst, const Color* src, int16_t count,
                                  const uint32_t* index) {
    BlendOp<mode> op;
    while (count-- > 0) {
      dst[*index] = op(dst[*index], *src);
      ++src;
      ++index;
    }
  }

  inline void applyOverBackground(Color bg, Color* src, int16_t count) {
    BlendOp<mode> op;
    while (count-- > 0) {
      *src = op(bg, *src);
      ++src;
    }
  }
};

namespace internal {

struct ApplyBlendingResolver {
  template <BlendingMode blending_mode>
  __attribute__((always_inline)) Color operator()(Color dst, Color src) const {
    return Blender<blending_mode>().apply(dst, src);
  }
};

struct ApplyBlendingInPlaceResolver {
  template <BlendingMode blending_mode>
  __attribute__((always_inline)) void operator()(Color* dst, const Color* src,
                                                 int16_t count) const {
    return Blender<blending_mode>().applyInPlace(dst, src, count);
  }
};

struct ApplyBlendingSingleSourceInPlaceResolver {
  template <BlendingMode blending_mode>
  __attribute__((always_inline)) void operator()(Color* dst, Color src,
                                                 int16_t count) const {
    return Blender<blending_mode>().applySingleSourceInPlace(dst, src, count);
  }
};

struct ApplyBlendingInPlaceIndexedResolver {
  template <BlendingMode blending_mode>
  __attribute__((always_inline)) void operator()(Color* dst, const Color* src,
                                                 int16_t count,
                                                 const uint32_t* index) const {
    return Blender<blending_mode>().applyInPlaceIndexed(dst, src, count, index);
  }
};

struct ApplyBlendingOverBackgroundResolver {
  template <BlendingMode blending_mode>
  __attribute__((always_inline)) void operator()(Color bg, Color* src,
                                                 int16_t count) const {
    return Blender<blending_mode>().applyOverBackground(bg, src, count);
  }
};

};  // namespace internal

// Returns the result of blending `src` over `dst` using the specified mode.
inline Color ApplyBlending(BlendingMode mode, Color dst, Color src) {
  return internal::BlenderSpecialization<internal::ApplyBlendingResolver>(
      mode, dst, src);
}

// Blends the `src` array over the `dst` array using the specified mode, writing
// back to `dst`.
inline void ApplyBlendingInPlace(BlendingMode mode, Color* dst,
                                 const Color* src, int16_t count) {
  return internal::BlenderSpecialization<
      internal::ApplyBlendingInPlaceResolver>(mode, dst, src, count);
}

// Blends a single `src` color over the `dst` array using the specified mode,
// writing back to `dst`.
inline void ApplyBlendingSingleSourceInPlace(BlendingMode mode, Color* dst,
                                             Color src, int16_t count) {
  return internal::BlenderSpecialization<
      internal::ApplyBlendingSingleSourceInPlaceResolver>(mode, dst, src,
                                                          count);
}

// Blends the `src` array over the indexed `dst` array using the specified mode,
// writing back to `dst`.
inline void ApplyBlendingInPlaceIndexed(BlendingMode mode, Color* dst,
                                        const Color* src, int16_t count,
                                        const uint32_t* index) {
  return internal::BlenderSpecialization<
      internal::ApplyBlendingInPlaceIndexedResolver>(mode, dst, src, count,
                                                     index);
}

// Blends the `src` array in place over the 'bg' color, using the specified
// mode.
inline void ApplyBlendingOverBackground(BlendingMode mode, Color bg, Color* src,
                                        int16_t count) {
  return internal::BlenderSpecialization<
      internal::ApplyBlendingOverBackgroundResolver>(mode, bg, src, count);
}

// Utility function to alpha-blend of the foreground color (fgc) over the
// background color (bgc), in a general case, where bgc might be
// semi-transparent. If the background is (or should be treated as) opaque, use
// AlphaBlendOverOpaque() instead.
inline Color AlphaBlend(Color bgc, Color fgc) {
  return BlendOp<BlendingMode::kSourceOver>()(bgc, fgc);
}

template <typename ColorMode, BlendingMode blending_mode,
          roo_io::ByteOrder byte_order>
struct RawFullByteBlender {
  void operator()(roo::byte* dst, Color src, const ColorMode& mode) const {
    ColorIo<ColorMode, byte_order> io;
    io.store(BlendOp<blending_mode>()(io.load(dst, mode), src), dst, mode);
  }

  void operator()(roo::byte* dst, Color src, ColorMode& mode) const {
    ColorIo<ColorMode, byte_order> io;
    io.store(BlendOp<blending_mode>()(io.load(dst, mode), src), dst, mode);
  }
};

template <typename ColorMode, roo_io::ByteOrder byte_order>
struct RawFullByteBlender<ColorMode, BlendingMode::kSource, byte_order> {
  void operator()(roo::byte* dst, Color src, const ColorMode& mode) const {
    ColorIo<ColorMode, byte_order> io;
    io.store(src, dst, mode);
  }

  void operator()(roo::byte* dst, Color src, ColorMode& mode) const {
    ColorIo<ColorMode, byte_order> io;
    io.store(src, dst, mode);
  }
};

template <typename ColorMode, roo_io::ByteOrder byte_order>
struct RawFullByteBlender<ColorMode, BlendingMode::kDestination, byte_order> {
  void operator()(roo::byte* dst, Color src, const ColorMode& mode) const {
    // No-op, leave dst as is.
  }
};

template <typename ColorMode, BlendingMode blending_mode>
struct RawSubByteBlender {
  uint8_t operator()(uint8_t dst, Color src,
                     const ColorMode& color_mode) const {
    return color_mode.fromArgbColor(
        BlendOp<blending_mode>()(color_mode.toArgbColor(dst), src));
  }

  uint8_t operator()(uint8_t dst, Color src, ColorMode& color_mode) const {
    return color_mode.fromArgbColor(
        BlendOp<blending_mode>()(color_mode.toArgbColor(dst), src));
  }
};

template <typename ColorMode>
struct RawSubByteBlender<ColorMode, BlendingMode::kSource> {
  uint8_t operator()(uint8_t dst, Color src,
                     const ColorMode& color_mode) const {
    return color_mode.fromArgbColor(src);
  }

  uint8_t operator()(uint8_t dst, Color src, ColorMode& color_mode) const {
    return color_mode.fromArgbColor(src);
  }
};

template <typename ColorMode>
struct RawSubByteBlender<ColorMode, BlendingMode::kDestination> {
  uint8_t operator()(uint8_t dst, Color src,
                     const ColorMode& color_mode) const {
    return dst;
  }
};

namespace internal {
template <typename ColorMode, roo_io::ByteOrder byte_order>
struct ApplyRawFullByteBlendingResolver {
  template <BlendingMode blending_mode>
  void operator()(roo::byte* dst, Color src,
                  const ColorMode& color_mode) const {
    RawFullByteBlender<ColorMode, blending_mode, byte_order>()(dst, src,
                                                               color_mode);
  }

  template <BlendingMode blending_mode>
  void operator()(roo::byte* dst, Color src, ColorMode& color_mode) const {
    RawFullByteBlender<ColorMode, blending_mode, byte_order>()(dst, src,
                                                               color_mode);
  }
};

template <typename ColorMode>
struct ApplyRawSubByteBlendingResolver {
  template <BlendingMode blending_mode>
  ColorStorageType<ColorMode> operator()(ColorStorageType<ColorMode> dst,
                                         Color src,
                                         const ColorMode& color_mode) const {
    return RawSubByteBlender<ColorMode, blending_mode>()(dst, src, color_mode);
  }

  template <BlendingMode blending_mode>
  ColorStorageType<ColorMode> operator()(ColorStorageType<ColorMode> dst,
                                         Color src,
                                         ColorMode& color_mode) const {
    return RawSubByteBlender<ColorMode, blending_mode>()(dst, src, color_mode);
  }
};

}  // namespace internal

// Returns the result of blending `src` over `dst` using the specified mode.
template <typename ColorMode, roo_io::ByteOrder byte_order>
inline void ApplyRawFullByteBlending(BlendingMode blending_mode, roo::byte* dst,
                                     Color src, const ColorMode& color_mode) {
  return internal::BlenderSpecialization<
      internal::ApplyRawFullByteBlendingResolver<ColorMode, byte_order>>(
      blending_mode, dst, src, color_mode);
}

// As above, for mutable color modes.
template <typename ColorMode, roo_io::ByteOrder byte_order>
inline void ApplyRawFullByteBlending(BlendingMode blending_mode, roo::byte* dst,
                                     Color src, ColorMode& color_mode) {
  return internal::BlenderSpecialization<
      internal::ApplyRawFullByteBlendingResolver<ColorMode, byte_order>>(
      blending_mode, dst, src, color_mode);
}

// As above, for sub-byte color modes.

// Returns the result of blending `src` over `dst` using the specified mode.
template <typename ColorMode>
inline ColorStorageType<ColorMode> ApplyRawSubByteBlending(
    BlendingMode blending_mode, uint8_t dst, Color src,
    const ColorMode& color_mode) {
  return internal::BlenderSpecialization<
      internal::ApplyRawSubByteBlendingResolver<ColorMode>>(blending_mode, dst,
                                                            src, color_mode);
}

// As above, for mutable color modes.
template <typename ColorMode>
inline ColorStorageType<ColorMode> ApplyRawSubByteBlending(
    BlendingMode blending_mode, uint8_t dst, Color src, ColorMode& color_mode) {
  return internal::BlenderSpecialization<
      internal::ApplyRawSubByteBlendingResolver<ColorMode>>(blending_mode, dst,
                                                            src, color_mode);
}

}  // namespace roo_display
