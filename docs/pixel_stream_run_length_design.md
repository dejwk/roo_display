# Pixel Stream Run-Length Design

Primary references:
[streamable.h](../src/roo_display/core/streamable.h)
[device.h](../src/roo_display/core/device.h)
[device.cpp](../src/roo_display/core/device.cpp)
[rasterizable.cpp](../src/roo_display/core/rasterizable.cpp)
[raster.h](../src/roo_display/core/raster.h)
[image_stream.h](../src/roo_display/image/image_stream.h)
[smooth_round_rect.cpp](../src/roo_display/shape/impl/smooth_round_rect.cpp)
[streamable_stack.cpp](../src/roo_display/composition/streamable_stack.cpp)
[testing.h](../test/testing.h)
[streamable_test.cpp](../test/streamable_test.cpp)
[streamable_stack_test.cpp](../test/streamable_stack_test.cpp)
[image_test.cpp](../test/image_test.cpp)
[smooth_shapes_test.cpp](../test/smooth_shapes_test.cpp)
[offscreen_test.cpp](../test/offscreen_test.cpp)
[BUILD](../BUILD)
[programming_guide.md](../doc/programming_guide.md)

## Objective

Add exact uniform-prefix run metadata to `PixelStream::read()` so that
`roo_display` can reduce repeated alpha-blend work and route long uniform spans
to `DisplayOutput::fill()` without adding an extra virtual probe for streams
that do not support run detection.

The final design keeps the current dense-buffer semantics, preserves the
existing layering around `Streamable`, and allows the library to adopt the new
information incrementally across stream producers and consumers.

## Motivation

Several important `roo_display` stream producers naturally generate long
uniform runs:

- run-length encoded image streams in [image_stream.h](../src/roo_display/image/image_stream.h),
- segment-based smooth shape streams in [smooth_round_rect.cpp](../src/roo_display/shape/impl/smooth_round_rect.cpp),
- synthetic constant-color streams such as filled rectangles in [basic.cpp](../src/roo_display/shape/basic.cpp),
- and rasterizable-backed streams that can already prove uniform same-row
  rectangles in [rasterizable.cpp](../src/roo_display/core/rasterizable.cpp).

Today that information is discarded at the `PixelStream` boundary. The common
extents-path helpers in [streamable.h](../src/roo_display/core/streamable.h)
therefore:

- blend each pixel individually against the background, even when a whole run
  shares one source color,
- and forward repeated colors through `write()` instead of `fill()`, even
  though `DisplayOutput::fill()` already maps to efficient device-specific fill
  paths in [device.cpp](../src/roo_display/core/device.cpp).

The design must improve those hot paths without making run-less inputs pay an
extra virtual call just to learn that no run is available.

## Background

### Current `PixelStream` Contract

`PixelStream` in [streamable.h](../src/roo_display/core/streamable.h) exposes a
single virtual `read(Color* buf, uint16_t size)` method. The contract is simple
and strong: the callee fully initializes the requested buffer with row-major
pixels.

That strong buffer contract is important because several layers consume
`PixelStream` output as ordinary dense buffers:

- `FillRectFromStream()` and its helpers in
  [streamable.h](../src/roo_display/core/streamable.h),
- `BufferingStream` and `SubRectangleStream` in
  [streamable.h](../src/roo_display/core/streamable.h),
- `StreamableComboStream` in
  [streamable_stack.cpp](../src/roo_display/composition/streamable_stack.cpp),
- and direct tests in [image_test.cpp](../test/image_test.cpp),
  [offscreen_test.cpp](../test/offscreen_test.cpp), and
  [testing.h](../test/testing.h).

The new design keeps that dense-buffer guarantee.

### Existing Uniformity Hooks Above The Stream Layer

`roo_display` already has one higher-level uniformity contract:
`Rasterizable::readColorRect()` may return `true` and only require `result[0]`
to be inspected when a rectangle is uniform.

That hook already helps the default rasterizable-backed stream in
[rasterizable.cpp](../src/roo_display/core/rasterizable.cpp): when the current
read stays on one row, it asks `readColorRect()` for a same-row uniform result
and expands the returned color locally.

That mechanism is useful but incomplete:

- it only exists for rasterizable-backed streams,
- it only helps when the stream implementation knows the original geometry,
- and it does not help RLE streams or procedural segment streams that already
  know run boundaries directly.

### Existing Output-Side Fill Acceleration

`DisplayOutput::fill()` already exists in
[device.h](../src/roo_display/core/device.h) and
[device.cpp](../src/roo_display/core/device.cpp). The default implementation
falls back to chunked `write()`, while many device and filter layers override it
with more efficient behavior.

That means the missing optimization is not a new output primitive. The missing
piece is exposing source-side run structure early enough that stream consumers
can select `fill()` and reduce repeated blending work.

## Requirements

- Preserve the current dense-buffer contract: every successful read fully
  initializes `buf[0..size-1]`.
- Avoid an additional virtual capability probe for run-less streams.
- Allow streams that know exact uniform runs to report them precisely.
- Allow streams that only know the current returned prefix to report that
  prefix without inventing a longer exact run.
- Keep per-instance RAM cost at zero for generic run-less `PixelStream`
  implementations.
- Keep API naming and ownership close to the current `PixelStream` contract.
- Make the common extents-path helpers in
  [streamable.h](../src/roo_display/core/streamable.h) the first optimization
  target, because they are the common background-blending path.
- Use a 32-pixel minimum threshold before replacing `write()` with `fill()` in
  stream consumers, while still allowing background-blending helpers to
  collapse uniform-prefix blend loops for any non-zero reported run.
- Roll out incrementally: phase 1 must compile and preserve behavior even when
  every stream reports `run_length = 0`.
- Preserve correctness for clipped streams, row boundaries, and `skip()`.
- Validate the public-header change with
  `products_compile_test` in addition to focused unit tests.
- Do not require an update to
  [programming_guide.md](../doc/programming_guide.md) in the initial rollout,
  because it documents `Streamable` usage but not custom `PixelStream`
  subclassing. The updated Doxygen comments in
  [streamable.h](../src/roo_display/core/streamable.h) are the canonical user
  contract.

## Design Overview

The chosen design is:

1. Change the primary virtual `PixelStream::read()` contract to accept an
   output `run_length` parameter.
2. Keep a non-virtual two-argument `read()` helper that forwards to the new
   virtual method for callers that do not care about run metadata.
3. Define `run_length` as the exact uniform-color prefix length that starts at
   the current stream position, with one deliberate concession for low-state
   producers: when a stream cannot prove the exact tail beyond the returned
   buffer, it may clamp the exact prefix to `size`.
4. Require the buffer to remain fully initialized even when a uniform prefix is
   reported.
5. Integrate the run-aware fast path first in the extents-path stream helpers
  in [streamable.h](../src/roo_display/core/streamable.h), using a 32-pixel
  threshold for `write()` to `fill()` substitution and unconditional
  uniform-prefix blend-loop collapse in the background-blending helpers.
6. Roll run production into streams that already know uniform runs cheaply:
   constant-color streams, rasterizable-backed row reads, RLE image streams,
   clipped sub-rectangle streams, and segment-based smooth round-rect streams.
7. Propagate run information through `StreamableStack` only for the cases that
   stay uniform without additional composition work: `BLANK` and
   `WRITE_SINGLE`. The multi-input `WRITE` path stays buffer-based in this
   design.

This design deliberately does not add a separate `supportsRuns()` or
`peekRun()` virtual method. A single `read()` dispatch remains the only
mandatory call on the hot path.

## Design Details

### `run_length` Semantics

The new `run_length` value has three cases:

- `run_length == 0`: the stream provides no run information for this read.
  The caller must treat the buffer exactly as it does today.
- `1 <= run_length <= size`: the first `run_length` pixels in the returned
  buffer are exactly uniform and equal to `buf[0]`. No guarantee is made about
  pixel `buf[run_length]` or any tail beyond the returned buffer.
- `run_length > size`: the stream guarantees that the exact uniform run from
  the current stream position has length `run_length`, so the caller may handle
  the first `size` pixels from the returned buffer, then optionally process the
  remaining `run_length - size` pixels as the same color and advance the source
  with `skip(run_length - size)`.

This contract keeps the common low-state cases cheap:

- a constant-color stream that does not track remaining pixels can return
  `run_length = size`,
- an RLE stream with explicit remaining-run state can return the exact full run
  length,
- and a stream that has no cheap run information can return `0`.

### Buffer Contract

The buffer contract does not change.

Every implementation must still fully initialize `buf[0..size-1]`. Uniform
prefix information is advisory metadata layered on top of the existing dense
buffer semantics.

This is the key decision that keeps the rest of the stack manageable. It lets
existing buffering layers, clipped streams, and tests continue to operate on
ordinary buffers while new consumers opt into run-aware handling only where it
is profitable.

### `skip()` Interaction

`skip()` keeps its current meaning: advance the stream by the requested number
of pixels.

When a caller receives `run_length > size`, it may treat the whole exact run as
one span. If the chosen emission strategy is `fill()`, it should prefer one
`fill(run_length)` call over separate prefix and tail calls, then call
`skip(run_length - size)` to keep the stream state aligned.

When a caller receives `run_length <= size`, it must not skip beyond the
already-consumed buffer tail, because the stream has already advanced through
all `size` returned pixels.

### Consumer Algorithm In `streamable.h`

The first optimized consumers are the three extents-path helpers in
[streamable.h](../src/roo_display/core/streamable.h):

- `fillReplaceRect()`,
- `fillPaintRectOverOpaqueBg()`,
- `fillPaintRectOverBg()`.

The visible-path helpers stay logically unchanged in this design. They may call
the two-argument convenience wrapper or ignore the returned `run_length`.

For each batch, the extents-path helpers should:

1. Read up to `kPixelWritingBufferSize` pixels and capture `run_length`.
2. If `run_length == 0`, preserve the current behavior.
3. Otherwise, treat the first `prefix = min(run_length, size)` pixels as one
   uniform run.
4. In `fillReplaceRect()`, replace `write()` with `fill()` only when the exact
  emitted run length is at least 32 pixels.
5. In `fillPaintRectOverOpaqueBg()` and `fillPaintRectOverBg()`, collapse the
  background blend loop for any non-zero uniform prefix, regardless of length.
6. If `run_length > size` and the chosen emission strategy is `fill()`, emit
  the whole exact run with one `fill(run_length)` call and then
  `skip(run_length - size)`.
7. Process any already-returned non-uniform suffix in the buffer with the
  existing logic.

The chosen threshold for `write()` to `fill()` substitution is `32` pixels.

Reasoning:

- a `fill()` call is another virtual dispatch, so the threshold should be high
  enough to avoid turning short repeated spans into many tiny virtual calls,
- 32 pixels is half of the typical 64-pixel read buffer, so it still catches
  the long runs that matter for device-side fill acceleration,
- and any exact run with `run_length > size` during a full-buffer read is
  already longer than the threshold, so the multi-batch case still collapses to
  one `fill(run_length)` call.

For the background-blending helpers, no threshold applies to reducing blend
work. Once the `run_length == 0` branch is already taken, replacing a uniform
prefix blend loop with one blended color is a win even for short prefixes.

### `fillReplaceRect()`

For `fillReplaceRect()`, a uniform run emits:

- `output.fill(buf[0], run_length)` plus `stream->skip(...)` when
  `run_length > size`,
- `output.fill(buf[0], prefix)` when `run_length <= size` and
  `prefix >= 32`,
- otherwise the helper keeps the ordinary buffer-based `write()` path for the
  returned pixels.

No additional color math is needed.

### `fillPaintRectOverOpaqueBg()` And `fillPaintRectOverBg()`

For the background-blending helpers, a uniform run emits:

- one blended color,
- one `output.fill(blended, run_length)` plus `stream->skip(...)` when
  `run_length > size`,
- one `output.fill(blended, prefix)` when `run_length <= size` and
  `prefix >= 32`,
- or, for shorter prefixes, one `FillColor()` over the already-returned prefix
  before the helper continues with its ordinary buffer-based write path.

This changes the blend cost for each reported uniform prefix from
$O(\text{prefix})$ to $O(1)$, independent of whether the emitted pixels end up
flowing through `fill()` or `write()`.

### Run Producers

#### Generic Run-Less Streams

Most existing streams can migrate in phase 1 by preserving their current
buffer-filling logic and adding:

```cpp
run_length = 0;
```

That gives a behavior-preserving baseline with no per-instance RAM increase.

#### Constant-Color Streams

Constant-color streams such as `FilledRectStream` in
[basic.cpp](../src/roo_display/shape/basic.cpp) should return:

- `run_length = size` when they do not track an exact remaining tail,
- or the exact remaining pixel count when that count is already available.

They should keep using `FillColor()` to initialize the full returned buffer.

#### `Rasterizable::Stream`

The stream adapter in [rasterizable.cpp](../src/roo_display/core/rasterizable.cpp)
already asks `readColorRect()` for same-row uniform rectangles.

It should now:

- set `run_length` to the exact same-row prefix length when
  `readColorRect()` reports a uniform row span,
- fill the returned prefix with `buf[0]` as it already does,
- and return `0` for multi-row reads or non-uniform same-row reads.

This preserves the current optimization and makes the run visible to the
consumer.

#### RLE Image Streams

The RLE streams in [image_stream.h](../src/roo_display/image/image_stream.h)
already carry run state in their existing fields.

Phase 3 should implement exact run reporting only where the current decoder
already knows one source color for a contiguous span:

- `RleStreamUniform<..., false>`: exact full run length when `run_` is true.
- `RleStreamUniform<..., true>`: exact run reporting only when the decoded
  repeated sub-byte pattern yields one repeated ARGB color for the queried
  prefix; otherwise `0`.
- `RleStreamRgb565Alpha4`: exact run reporting only when the current group has
  one repeated final ARGB color for the queried prefix, such as repeated RGB
  with uniform alpha or fully transparent / fully opaque groups.
- `RleStream4bppxBiased`: exact full run length when `run_` is true.

Arbitrary-value groups remain `run_length = 0`.

#### `SubRectangleStream`

`SubRectangleStream` in [streamable.h](../src/roo_display/core/streamable.h)
must propagate prefix runs, otherwise clipping would erase most of the new
value for images and procedural shapes.

The clipped run length is:

$$
\text{clipped\_prefix} = \min(\text{delegate\_prefix},\; \text{remaining\_pixels\_in\_row})
$$

where `delegate_prefix` is the underlying stream's reported prefix starting at
the current clipped position.

If the delegate reports no run information, `SubRectangleStream` reports `0`.

If the delegate reports an exact tail longer than the clipped row remainder,
the clipped stream reports only the row-bounded exact prefix. The row skip at
the sub-rectangle boundary breaks the clipped run, even when the underlying
delegate continues uniformly.

#### Smooth Round-Rect Streams

The segment-based streams in
[smooth_round_rect.cpp](../src/roo_display/shape/impl/smooth_round_rect.cpp)
already classify scanline spans into solid-color and slow-evaluation segments.

They should report:

- the exact remaining segment length for transparent, interior, outline, or
  other solid-color segments,
- `0` for `kSlow` segments,
- and the full returned buffer should still be initialized with the existing
  `FillColor()` or per-pixel fallback logic.

This reuses current scanline segmentation and does not require any new geometry
state.

### `StreamableStack` Propagation

`StreamableStack` in
[streamable_stack.cpp](../src/roo_display/composition/streamable_stack.cpp)
should propagate run information only for the cases that remain uniform without
additional composition:

- `BLANK`: the transparent prefix is exact for the current instruction span.
- `WRITE_SINGLE`: the prefix is whatever the one contributing input reports,
  clamped to the instruction's remaining pixel count.

The multi-input `WRITE` instruction stays buffer-based and reports
`run_length = 0` in this design.

To support `WRITE_SINGLE`, `BufferingStream` in
[streamable.h](../src/roo_display/core/streamable.h) should store one
additional `uint32_t` describing the exact prefix length reported by the most
recent fetch. That is the only persistent RAM added by this design, and it is
limited to `StreamableStack`'s internal buffering adapter.

The added RAM cost is 4 bytes per active buffered input.

That is acceptable because:

- it is not charged to generic run-less stream implementations,
- it is paid only by `StreamableStack`, which already allocates one
  `BufferingStream` per active input,
- and it enables exact run propagation through `BLANK` and `WRITE_SINGLE`
  without a second virtual call.

### RAM And Rendering-Cost Impact

For the common direct stream-drawing path:

- run-less streams pay no per-instance RAM cost,
- consumers pay one additional local `uint32_t` and one branch per batch,
- the 64-pixel buffer size remains unchanged,
- and uniform runs collapse per-pixel background blending to one blended color
  for every reported prefix while only routing sufficiently long spans through
  `fill()`.

For `StreamableStack`:

- each active `BufferingStream` gains 4 bytes of RAM,
- `BLANK` and `WRITE_SINGLE` spans can preserve exact uniform prefixes,
- multi-input `WRITE` remains unchanged.

The design therefore spends RAM only where run propagation through a buffering
adapter requires it, and keeps generic stream implementations RAM-neutral.

## Proposed API

The public `PixelStream` declaration in
[streamable.h](../src/roo_display/core/streamable.h) should become:

```cpp
class PixelStream {
 public:
  /// Read up to `size` pixels into `buf`, optionally reporting a uniform
  /// prefix run starting at the current stream position.
  ///
  /// The implementation must fully initialize `buf[0..size-1]`.
  ///
  /// `run_length == 0` means that no run information is provided.
  ///
  /// `1 <= run_length <= size` means that the returned buffer starts with an
  /// exact uniform-color prefix of length `run_length`, equal to `buf[0]`.
  /// No guarantee is made beyond the returned buffer.
  ///
  /// `run_length > size` means that the exact uniform run from the current
  /// stream position has length `run_length`. The caller may process the
  /// returned `size` pixels from `buf`, then optionally handle the remaining
  /// tail as the same color and call `skip(run_length - size)`.
  virtual void read(Color* buf, uint16_t size, uint32_t& run_length) = 0;

  /// Convenience overload for callers that do not use run metadata.
  inline void read(Color* buf, uint16_t size) {
    uint32_t ignored_run_length = 0;
    read(buf, size, ignored_run_length);
  }

  virtual void skip(uint32_t count) {
    Color buf[kPixelWritingBufferSize];
    uint32_t ignored_run_length = 0;
    while (count > kPixelWritingBufferSize) {
      read(buf, kPixelWritingBufferSize, ignored_run_length);
      count -= kPixelWritingBufferSize;
    }
    read(buf, count, ignored_run_length);
  }
};
```

The deprecated camel-case compatibility wrapper should forward to the
two-argument convenience overload.

## Implementation Plan

Authoring reference: [roo_display code authoring](../.github/skills/roo-display-code-authoring/SKILL.md) on top of [embedded C++ code authoring](../.github/instructions/embedded-cpp-code-authoring.instructions.md).

### Phase 1: Migrate `PixelStream` To The Run-Aware Contract

Work:

- change the primary virtual `PixelStream::read()` declaration in
  [streamable.h](../src/roo_display/core/streamable.h),
- keep the two-argument convenience wrapper and update `skip()` to call the new
  virtual method,
- update every `PixelStream` implementation and direct test stream to compile
  with `run_length = 0` and no behavioral change,
- update direct call sites that must invoke the new virtual form,
- update Doxygen comments to describe the new contract.

Proposed commit message:

```text
pixel_stream_run_length_design phase 1: migrate PixelStream to the run-aware read contract

Replace the PixelStream virtual read entry point with the three-argument form,
keep the two-argument helper for callers that do not use run metadata, and
update existing stream implementations and direct tests to return
run_length = 0 with no behavioral change.
```

Validation:

- `(cd lib/roo_display; bazel test //:image_test)`
- `(cd lib/roo_display; bazel test //:offscreen_test)`
- `(cd lib/roo_display; bazel test //:streamable_test)`
- `(cd lib/roo_display; bazel test //:streamable_stack_test)`
- `(cd lib/roo_display; bazel test //:products_compile_test)`

### Phase 2: Optimize The Extents-Path Helpers In `streamable.h`

Work:

- add a 32-pixel internal threshold constant for `write()` to `fill()`
  substitution,
- teach `fillReplaceRect()` to use `run_length` for thresholded fill
  substitution,
- teach `fillPaintRectOverOpaqueBg()` and `fillPaintRectOverBg()` to use
  `run_length` for unconditional prefix blend-loop collapse and thresholded
  fill substitution, using one `fill(run_length)` call when an exact run spans
  beyond the returned buffer,
- keep visible-path helpers behaviorally unchanged,
- add focused regression coverage in
  [streamable_test.cpp](../test/streamable_test.cpp) using a small counting
  output or test double that verifies the new path-selection behavior without
  duplicating the broad pixel-output coverage already exercised by existing
  streamable tests.

Proposed commit message:

```text
pixel_stream_run_length_design phase 2: exploit uniform prefixes in the extents-path helpers

Teach the direct stream drawing helpers in streamable.h to use run_length for
background blending and thresholded fill forwarding so uniform prefixes always
blend once and long exact spans emit through DisplayOutput::fill() while
preserving existing visible-path behavior.
```

Validation:

- `(cd lib/roo_display; bazel test //:streamable_test)`

### Phase 3: Add Run Reporting To Low-Cost Producers And Clipping Adapters

Work:

- update constant-color streams such as `FilledRectStream`,
- update `Rasterizable::Stream` to surface same-row uniform rectangles,
- update the RLE image streams in
  [image_stream.h](../src/roo_display/image/image_stream.h) for the exact run
  cases described above,
- update `SubRectangleStream` so clipped stream creation preserves prefix runs
  across row-bounded clips,
- add focused tests in [image_test.cpp](../test/image_test.cpp),
  [rasterizable_test.cpp](../test/rasterizable_test.cpp), and
  [streamable_test.cpp](../test/streamable_test.cpp).

Proposed commit message:

```text
pixel_stream_run_length_design phase 3: surface exact runs from cheap stream producers

Implement run_length reporting for constant-color streams, rasterizable-backed
same-row reads, RLE image streams, and clipped sub-rectangle adapters so the
new PixelStream contract carries uniform-prefix information through the common
direct drawing paths.
```

Validation:

- `(cd lib/roo_display; bazel test //:image_test)`
- `(cd lib/roo_display; bazel test //:rasterizable_test)`
- `(cd lib/roo_display; bazel test //:streamable_test)`

### Phase 4: Add Run Reporting To Segment-Based Smooth Shape Streams

Work:

- update the segment-based smooth round-rect streams in
  [smooth_round_rect.cpp](../src/roo_display/shape/impl/smooth_round_rect.cpp)
  to report exact solid-segment prefixes,
- keep `kSlow` spans at `run_length = 0`,
- add regression coverage in
  [smooth_shapes_test.cpp](../test/smooth_shapes_test.cpp) and any direct
  stream tests needed for the segment-prefix contract.

Proposed commit message:

```text
pixel_stream_run_length_design phase 4: expose exact solid segments from smooth round-rect streams

Teach the segment-based smooth round-rect stream implementations to report the
exact remaining length of their solid-color scanline segments while leaving the
anti-aliased slow spans on the ordinary dense-buffer path.
```

Validation:

- `(cd lib/roo_display; bazel test //:smooth_shapes_test)`
- `(cd lib/roo_display; bazel test //:streamable_test)`

### Phase 5: Propagate Runs Through `StreamableStack` For `BLANK` And `WRITE_SINGLE`

Work:

- extend `BufferingStream` to retain the current fetched prefix run length,
- update `StreamableComboStream` to implement the three-argument `read()` and
  propagate exact runs for `BLANK` and `WRITE_SINGLE`,
- keep the multi-input `WRITE` instruction buffer-based and return
  `run_length = 0`,
- add focused coverage in
  [streamable_stack_test.cpp](../test/streamable_stack_test.cpp) for blank
  spans and disjoint single-input spans.

Proposed commit message:

```text
pixel_stream_run_length_design phase 5: propagate simple uniform prefixes through StreamableStack

Preserve exact run metadata through BufferingStream and StreamableComboStream
for blank spans and single-input instruction ranges while deliberately leaving
the multi-input composition path on the existing dense-buffer implementation.
```

Validation:

- `(cd lib/roo_display; bazel test //:streamable_stack_test)`

## Testing Plan

The rollout should validate three layers of behavior.

Focused contract coverage:

- direct stream tests in [image_test.cpp](../test/image_test.cpp),
  [streamable_test.cpp](../test/streamable_test.cpp),
  [rasterizable_test.cpp](../test/rasterizable_test.cpp), and
  [smooth_shapes_test.cpp](../test/smooth_shapes_test.cpp) should verify the
  `run_length` contract for exact prefixes, exact tails, clipped prefixes, and
  `run_length = 0` fallbacks.
- those focused additions should cover only new contract and path-selection
  cases that existing broad rendering and stress tests do not already cover.

Behavior-preserving drawing coverage:

- [streamable_test.cpp](../test/streamable_test.cpp) should continue to prove
  the existing pixel output for visible and extents fill modes,
- [streamable_stack_test.cpp](../test/streamable_stack_test.cpp) should prove
  that propagation through composition preserves rendered output.

Compile and integration coverage:

- `(cd lib/roo_display; bazel test //:products_compile_test)` after the API
  phase,
- `(cd lib/roo_display; bazel build //...)` after the final phase,
- `(cd lib/roo_display; bazel test //... --test_output=errors)` before merging
  the full rollout.

## Caveats

### Prefix-Only Metadata

The chosen contract exposes only the uniform prefix that begins at the current
stream position. It does not describe later runs that may already have been
consumed into the remainder of the same returned buffer.

This is intentional. The prefix-only contract keeps the API compact and cheap.
It also aligns with the current fixed 64-pixel buffer size, where the highest
value runs are usually the ones that begin exactly at the current read cursor.

### Visible-Path Helpers Stay Buffer-Based

The visible-path helpers in
[streamable.h](../src/roo_display/core/streamable.h) are not made run-aware in
this design.

Those paths write sparse visible pixels through `BufferedPixelWriter`, and the
main direct benefit from uniform prefixes there is smaller than in the extents
path. Keeping them unchanged avoids reopening their coordinate buffering and
per-pixel emission strategy in the initial rollout.

### `StreamableStack` Multi-Input `WRITE` Remains Unoptimized

The design intentionally leaves the multi-input `WRITE` instruction in
[streamable_stack.cpp](../src/roo_display/composition/streamable_stack.cpp) on
the existing dense-buffer path.

Reducing blend calls there requires tracking a uniform prefix of the composed
result and shrinking it as each additional input contributes. That is a real
optimization opportunity, but it is a separate problem from exposing source
stream runs and it would materially widen the first rollout.

### Rejected Alternatives

#### Boolean `all_same`

A boolean uniformity signal is too weak.

It tells the consumer only that the requested batch happened to be uniform. It
does not expose the run length the caller needs for `fill()` and reduced
blending, and it loses all value whenever the requested batch straddles the end
of a longer run.

#### Sparse-Buffer Contract For Uniform Reads

Leaving only `buf[0]` initialized on uniform reads was rejected.

That would force buffering layers, clipped adapters, and tests to carry extra
side state or eagerly expand the color again, which throws away much of the
value and makes the rest of the stream stack more fragile.

#### Separate Run-Probe Virtual Method

Adding a second virtual method such as `supportsRuns()`, `peekRun()`, or
`readRun()` was rejected.

The common case must not pay an extra virtual call merely to discover that no
run optimization is available. The new design keeps a single mandatory dispatch
and lets run-less producers report `run_length = 0` cheaply.

## Future Work

- Extend `StreamableStack`'s multi-input `WRITE` instruction to reduce
  `ApplyBlendingInPlace()` calls when all contributing inputs remain uniform
  over the same prefix.
- Revisit `WindowedPixelStream` and `BlendingFilter` in
  [blender.h](../src/roo_display/filter/blender.h) if profiling shows that
  stream-window synthesis and raster-overlay blending need the same prefix-run
  propagation.
- Consider a corresponding uniform-prefix hook for the raw-stream drawing path
  in [raw_streamable.h](../src/roo_display/internal/raw_streamable.h) if the
  raw-stream path becomes a measured bottleneck.