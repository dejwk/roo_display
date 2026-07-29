# Integer Letter Tracking Design

## Objective

Add signed, integer-pixel letter tracking to horizontal text measurement and
rasterization in `roo_display`, using the in-progress
[`Font::Options`](../src/roo_display/font/font.h) API as the public
configuration point.

The option adds a fixed number of pixels to every inter-glyph advance. It
preserves kerning, does not add space before the first or after the last glyph,
and produces identical measurement and drawing results.

**Implementation status: phase 2 complete.** `Font::Options` stores signed
`int16_t` tracking, and all built-in fonts implement the option-aware
measurement and rasterization contracts. Text-drawable propagation remains
for phase 3.

## Motivation

Typography systems such as Material Design specify tracking independently from
font family, size, and line height. `roo_display` raster fonts use integer
glyph positions, so an integer adjustment is the smallest faithful primitive:
it can express the visible one-pixel expansion or contraction needed at larger
UI scales without introducing unsupported sub-pixel state.

## Background

[`Font`](../src/roo_display/font/font.h) currently exposes:

- per-glyph metrics and kerning,
- whole-string and per-glyph string measurement,
- horizontal string drawing, and
- three built-in implementations:
  [`SmoothFont`](../src/roo_display/font/smooth_font.h),
  [`SmoothFontV2`](../src/roo_display/font/smooth_font_v2.h), and
  [`FontAdafruitFixed5x7`](../src/roo_display/font/font_adafruit_fixed_5x7.h).

The smooth-font draw loops already calculate a kerning-adjusted advance between
adjacent decoded code points. Measurement duplicates that traversal. Tracking
belongs at this boundary: after the existing advance and kerning calculation,
before positioning the next glyph.

Android's text paint API is useful precedent for treating letter spacing as a
paint-time property distinct from a font file, although Android expresses the
value in `em` and retains fractional precision. See
[`Paint::setLetterSpacing`](https://developer.android.com/reference/android/graphics/Paint#setLetterSpacing(float)).
This proposal deliberately exposes pixels because every `roo_display` glyph
origin and advance is integral.

The font importer can independently rasterize a typeface at a fractional
nominal size such as 8.25, 10.5, or 16.5 px. The generated `Font` still
contains integral bitmap dimensions, metrics, advances, and glyph origins.
Fractional nominal generation is therefore compatible with this design but
does not make `tracking_px` fractional: callers quantize tracking for each
generated font exactly as they do for an integer nominal size.

## Requirements

1. `Font::Options` shall accept positive, zero, and negative integer tracking.
2. Tracking shall change only the advance between adjacent decoded glyphs.
   Empty and one-glyph strings shall be unchanged.
3. Existing font kerning shall remain active. Tracking shall adjust the result
   of the existing kerning-aware advance calculation rather than replace
   kerning.
4. Whole-string measurement, per-glyph measurement, drawing, drawable extents,
   and drawable anchor extents shall agree for the same options.
5. The existing no-options API shall retain byte-for-byte raster behavior and
   current metrics.
6. All built-in fonts shall support tracking in `kVisible` and `kExtents` fill
   modes, including positive gaps and negative overlap.
7. UTF-8 decoding and missing-glyph behavior shall remain unchanged. Tracking
   applies to decoded glyph emissions, not bytes.
8. The implementation shall not allocate, add font payload, add persistent
   font state, or require regenerated font files.
9. The API shall document that it is not a shaping engine: it does not perform
   grapheme clustering, ligature formation, bidi reordering, or
   script-sensitive tracking suppression.
10. Existing callers shall remain source compatible. Implementers of custom
    `Font` subclasses shall implement the new option-aware pure virtual
    overloads.

## Design Overview

`Font::Options` remains a small value object owned by the caller. The font
receives it for every operation whose horizontal result depends on tracking:

```text
UTF-8 decode -> glyph metrics -> existing kerning-aware advance -> + tracking
                       |                                      |
                       +----------- ink extents --------------+
                                                              |
                                                     next glyph origin
```

For glyph origins \(p_i\), existing inter-glyph advances \(a_i\), signed
tracking \(t\), and \(n\) emitted glyphs:

\[
p_0 = 0,\qquad p_{i+1} = p_i + a_i + t \quad (0 \le i < n-1)
\]

The string advance therefore changes by:

\[
\Delta advance =
\begin{cases}
0 & n < 2 \\
(n-1)t & n \ge 2
\end{cases}
\]

![Tracking changes the inter-glyph advance without changing the glyphs or end
edges.](images/integer_letter_tracking.svg)

The existing overloads remain the zero-tracking entry points. Built-in font
implementations make those overloads delegate to the option-aware core with a
default-constructed `Font::Options`, so there is one algorithm per
implementation.

## Design Details

### Option semantics

`tracking_px` is a signed pixel count. A value of `1` moves every glyph after
the first one pixel farther along the horizontal layout direction. A value of
`-1` moves it one pixel closer. Tracking is applied to spaces and missing-glyph
fallbacks in the same way as any other emitted glyph.

No tracking is added:

- before the first glyph,
- after the last glyph,
- around an empty string, or
- to `drawGlyph()`, which has no adjacent-glyph boundary.

The setter does not clamp. The same coordinate-range limits that already apply
to long strings continue to apply. A sufficiently negative value can make an
effective advance zero or negative; that behavior is intentional and is
reported by measurement exactly as drawn.

`Options` gains a public `trackingPx()` accessor. The enclosing `Font` class
and its derived implementations cannot otherwise read the nested class's
private member.

### Measurement

Both string measurement families gain option-aware overloads:

- `getHorizontalStringMetrics()` translates every glyph's ink box to its
  tracked origin, and returns the tracked final advance.
- `getHorizontalStringGlyphMetrics()` returns each glyph at its tracked origin.
  The `advance()` in a non-final glyph result includes the following tracking
  boundary; the final glyph does not.

Offsets and `max_count` limit returned entries, not layout context. The
implementation still decodes preceding glyphs when needed to establish the
correct origin and still knows whether a returned glyph is final in the full
input string.

The option-aware overloads are virtual and pure, matching the in-progress draw
API. The no-options overloads remain virtual for source compatibility with
callers and delegate to the option-aware implementation inside each built-in
font. This does require custom `Font` subclasses to add the new virtual
overrides; silently falling back to untracked measurement would be a more
dangerous compatibility failure because layout and paint would disagree.

### Smooth-font rasterization

[`SmoothFont`](../src/roo_display/font/smooth_font.cpp) and
[`SmoothFontV2`](../src/roo_display/font/smooth_font_v2.cpp) add
`tracking_px` to their already computed inter-glyph `advance` and `gap`.

For `FillMode::kVisible`, this only changes the next glyph origin. For
`FillMode::kExtents`:

- positive tracking extends the background-filled gap by the same amount;
- negative tracking moves the pair into the existing overlap compositor; and
- the final glyph retains its current right-side-bearing treatment because no
  trailing tracking exists.

Adding tracking to both `advance` and `gap` preserves the current
`preadvanced` invariant: every output pixel is still settled once even when
adjacent glyph boxes overlap.

### Fixed 5x7 rasterization

[`FontAdafruitFixed5x7`](../src/roo_display/font/font_adafruit_fixed_5x7.cpp)
increments the next origin by its existing advance plus tracking. In
`kExtents`, a positive addition explicitly fills the extra columns with the
surface background. A negative addition lets the later glyph overwrite the
overlapped columns, matching left-to-right paint order.

### Text drawables

[`TextLabel`](../src/roo_display/ui/text_label.h),
[`StringViewLabel`](../src/roo_display/ui/text_label.h), and their clipped
variants gain constructor overloads with a trailing `const Font::Options&`.
They copy the options, measure with them during construction, and draw with the
same copy. Existing constructors delegate with default options.

The retained cost is `sizeof(Font::Options)`, one `int16_t`, for each
options-aware label drawable. These drawables are normally short-lived paint
objects. Font instances and generated font payloads do not grow.

### Cost

The option adds:

- one signed addition per inter-glyph boundary in drawing and measurement;
- one zero-value branch at the public no-options delegation boundary, which
  compilers can inline away for direct calls; and
- background writes for newly exposed positive tracking columns in
  `kExtents`.

There are no new buffers or allocations. `Font::Options` occupies one `int16_t`
on the stack or in an option-aware drawable. Raster font flash payload is
unchanged. The dominant rendering cost remains glyph decoding and pixel
output; the added arithmetic is constant per glyph rather than per pixel.

## Proposed API

The completed public surface in
[`font.h`](../src/roo_display/font/font.h) is:

```cpp
class Font {
 public:
  class Options {
   public:
    Options() : tracking_px_(0) {}

    /// Sets the signed pixel adjustment between adjacent glyphs.
    Options& setTrackingPx(int16_t tracking_px) {
      tracking_px_ = tracking_px;
      return *this;
    }

    /// Returns the signed pixel adjustment between adjacent glyphs.
    int16_t trackingPx() const { return tracking_px_; }

   private:
    int16_t tracking_px_;
  };

  void drawHorizontalString(const Surface& s, roo::string_view text,
                            Color color, const Options& options) const;

  virtual void drawHorizontalString(const Surface& s, const char* utf8_data,
                                    uint32_t size, Color color,
                                    const Options& options) const = 0;

  GlyphMetrics getHorizontalStringMetrics(
      roo::string_view text, const Options& options) const;

  virtual GlyphMetrics getHorizontalStringMetrics(
      const char* utf8_data, uint32_t size,
      const Options& options) const = 0;

  uint32_t getHorizontalStringGlyphMetrics(
      roo::string_view text, GlyphMetrics* result, uint32_t offset,
      uint32_t max_count, const Options& options) const;

  virtual uint32_t getHorizontalStringGlyphMetrics(
      const char* utf8_data, uint32_t size, GlyphMetrics* result,
      uint32_t offset, uint32_t max_count,
      const Options& options) const = 0;
};
```

The existing overloads without `Options` remain unchanged. Option-aware text
drawable constructors use a trailing parameter so existing call sites and
default fill-mode arguments remain unambiguous:

```cpp
StringViewLabel(roo::string_view label, const Font& font, Color color,
                FillMode fill_mode, const Font::Options& options);
```

## Implementation Plan

Implementation follows the
[`roo_display` embedded C++ authoring guidance](../.github/instructions/embedded-cpp-code-authoring.instructions.md).

### Phase 1: Complete the font contract and measurement

Add the options accessor and option-aware whole-string and per-glyph
measurement overloads. Update all three built-in font declarations and
implement measurement using the tracked-origin formula. Add focused tests for
empty, one-glyph, kerned, whitespace, positive, and negative cases.

**Proposed commit message:**

> Integer letter tracking design phase 1: Complete tracked font measurement.
>
> Completes `Font::Options`, adds option-aware whole-string and per-glyph
> measurement, and covers tracked origins, advances, kerning, and edge cases.

Validate with `bazel test //:smooth_font_test //:text_label_test` from the
`roo_display` repository.

### Phase 2: Implement tracked rasterization

Implement the option-aware draw core in both smooth-font formats and the fixed
5x7 font. Make legacy drawing delegate with default options. Add raster tests
for `kVisible`, `kExtents`, clipping, positive gaps, negative overlap, and
kerning plus tracking.

**Proposed commit message:**

> Integer letter tracking design phase 2: Render tracked glyph advances.
>
> Implements positive and negative tracking in both smooth-font formats and
> the fixed 5x7 font, including visible, extents-filled, and clipped raster
> coverage.

Validate with the focused smooth-font and text-label tests, then
`bazel test //...`.

### Phase 3: Propagate options through text drawables and documentation

Add option-aware constructors to the owned, borrowed, and clipped label
drawables. Test that cached extents, anchor extents, and raster output use the
same options. Document tracking and the no-shaping limitation in
[`doc/programming_guide.md`](../doc/programming_guide.md).

**Proposed commit message:**

> Integer letter tracking design phase 3: Expose tracking in text drawables.
>
> Propagates matching options through owned and borrowed label measurement and
> paint, adds focused drawable coverage, and documents the public behavior and
> shaping boundary.

Validate with `bazel test //:text_label_test` and compile at least one
downstream `roo_windows` target that uses an option-aware label.

## Testing Plan

Unit coverage shall verify formulas and pixels across:

- zero, positive, and negative tracking;
- zero, one, and multiple glyphs;
- kerned and unkerned pairs;
- spaces, UTF-8 multi-byte code points, and fallback glyphs;
- full-string and offset per-glyph measurement;
- visible, extents-filled, and clipped drawing; and
- both smooth-font encodings plus the fixed 5x7 font.

The full `roo_display` suite proves that default options preserve current
rendering. A downstream `roo_windows` build proves the abstract interface and
drawable overloads integrate without driver or transport changes. No
device-specific test is needed because tracking changes font-space coordinates
before the existing `DisplayOutput` path.

## Caveats

Integer tracking is intentionally coarse. At small sizes, many Material token
values quantize to zero; callers must choose an integer per rendered font size
rather than repeatedly round a fractional accumulator.

Tracking follows decoded glyph emissions. Scripts that require shaping or
forbid spacing inside grapheme clusters remain outside `roo_display`'s current
text model.

### Rejected Alternatives

#### Fractional accumulators

Keeping fixed-point tracking and occasionally emitting an extra pixel would
produce uneven patterns such as `0, 1, 0, 1` between successive glyphs. It
would also add accumulator state to every measurement and draw path while
glyph origins remain integral. The fixed integer adjustment is predictable and
matches the target renderer. This rejection concerns runtime spacing only; it
does not reject offline rasterization at fractional nominal font sizes.

#### Baking tracked copies of font files

Duplicating font data for each tracking value would multiply generated source,
build time, and flash payload even though glyph bitmaps are identical.
Paint-time advance adjustment provides the same geometry with no font-data
growth.

#### Applying tracking after the final glyph

A trailing adjustment makes text width depend on an invisible edge and differs
from established line-edge behavior. Inter-glyph-only tracking keeps the first
and last line edges stable and makes the \((n-1)t\) formula explicit.

#### A non-virtual base fallback for custom fonts

A generic base implementation built from `drawGlyph()` cannot preserve every
font's optimized `kExtents` overlap and background-settling behavior. Requiring
option-aware overrides makes correctness explicit and prevents measured and
painted widths from silently diverging.

## Future Work

Fractional glyph positioning, shaping, bidi layout, and grapheme-aware
tracking suppression can be introduced together as a separate shaped-text
API. They do not change the integer contract defined here.
