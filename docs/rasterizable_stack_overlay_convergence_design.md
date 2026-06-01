# Rasterizable Stack Overlay Convergence Design

Primary references:
[rasterizable_stack.h](../src/roo_display/composition/rasterizable_stack.h)
[rasterizable_stack.cpp](../src/roo_display/composition/rasterizable_stack.cpp)
[streamable_stack.h](../src/roo_display/composition/streamable_stack.h)
[foreground.h](../src/roo_display/filter/foreground.h)
[blending.h](../src/roo_display/color/blending.h)
[clipper.h](../../roo_windows/src/roo_windows/core/clipper.h)
[clipper.cpp](../../roo_windows/src/roo_windows/core/clipper.cpp)
[paint_context.cpp](../../roo_windows/src/roo_windows/core/paint_context.cpp)
[dynamic_composition.ino](../examples/dynamic_composition/dynamic_composition.ino)

## Status

Phase 1 complete.

Phase 2 complete in `roo_display`: `RasterizableStack::readColorRect()` now
skips partial layers that prove uniformly transparent through
`readUniformColorRect()` only for blend modes where transparent source
preserves destination, and `readUniformColorRect()` applies the same rule.
Focused regression coverage in `rasterizable_stack_test` now covers both the
source-over fast path and a non-no-op blend mode (`kClear`).

Phase 3 remains pending. Phase 4 is now only optional API follow-up if the
descriptor-driven clipper migration exposes a reusable helper worth
generalizing.

## Objective

Bring `roo_windows` overlay composition closer to the general `roo_display`
stack abstractions so that `roo_windows::OverlayStack` can eventually be
replaced with `roo_display::RasterizableStack` without losing current behavior
or important hot-path optimizations, and without preserving
`bounded_overlays_` or a wrapper-style `ClippedOverlay` as migration baggage.

This design answers three questions:

1. Can clipper overlay order be preserved without a custom reverse-order stack?
2. Is `kDestinationOver` a valid ordering tool?
3. If `OverlayStack` or `ClippedOverlay` has a meaningful optimization that the
   generic stack lacks, should that optimization move into `roo_display`?

## Motivation

`roo_windows::OverlayStack` and `roo_display::RasterizableStack` solve almost
the same composition problem:

- maintain a set of rasterizable inputs,
- compute the composed extents,
- answer `readColors()`, `readColorRect()`, and `readUniformColorRect()`,
- and participate in a filtering pipeline.

The main reasons to converge on `RasterizableStack` are:

- less duplicate composition logic,
- one shared set of tests for generic stack behavior,
- access to `RasterizableStack::createStream()`, which `OverlayStack` does not
  provide,
- and a clearer split where `roo_windows` keeps only the widget-specific
  concerns: overlay ownership, bounds filtering, and device-clip translation
  onto generic stack inputs.

The remaining differences are real, but they are narrow:

- clipper wants earlier-added overlays to stay above later-added overlays,
- clipper clips in device space after translation,
- and clipper currently models each overlay as another `Rasterizable` even
  though the converged path only needs source pointer, clip, and offset
  metadata.

## Background

### Current `roo_windows` Overlay Path

Today clipper uses two helper layers:

- `ClippedOverlay`, which wraps a `Rasterizable` with a device-space clip and a
  signed translation,
- `OverlayStack`, which composes a contiguous range of `ClippedOverlay`
  objects.

`ClipperOutput::sync()` filters the full overlay list against the active bounds,
stores the intersecting subset in `bounded_overlays_`, and then rebinds
`OverlayStack` to that contiguous range.

That extra compacted vector exists to satisfy `OverlayStack`'s range-based
`reset(begin, end)` API. It is an implementation detail of the current helper,
not a semantic requirement of clipper ordering or clipping.

For convergence, the design should not preserve either artifact as-is.
`bounded_overlays_` exists only because of `OverlayStack`, and
`ClippedOverlay` should shrink to plain per-overlay metadata that clipper uses
when rebuilding `RasterizableStack`.

The stack reads overlays in reverse vector order. For overlays added in the
sequence `o0`, `o1`, `o2`, the final visual order is:

`o0 over o1 over o2`

That matches the clipper contract that newly added overlays are layered under
previously added overlays.

### Current `roo_display` Stack Path

`RasterizableStack` stores inputs internally and composes them in insertion
order using a per-input blending mode.

With the default `kSourceOver` mode, inputs `o0`, `o1`, `o2` compose as:

`o2 over o1 over o0`

which is the opposite of clipper's overlay contract.

Unlike `OverlayStack`, `RasterizableStack` can also create a stream by lowering
it to `StreamableStack`. That is relevant because clipper overlays feed a
`ForegroundFilter`, and that filter can exploit `createStream()` for address-
window drawing.

## Requirements

- Preserve clipper's visible overlay order.
- Preserve signed translations.
- Preserve device-space clipping semantics.
- Avoid unnecessary per-frame allocations after warmup.
- Keep the general `roo_display` stack API understandable for non-clipper use.
- Only port optimizations whose value extends beyond this one caller.

## Ordering Analysis

There are two viable ways to preserve clipper ordering with
`RasterizableStack`.

### Option A: Reverse The Inputs During `sync()`

Clipper already rebuilds the active overlay set dynamically in `sync()`.

That means it can walk `overlays_` once, filter by translated extents, and add
the surviving descriptors to a reusable `RasterizableStack` in reverse order
while keeping the default `kSourceOver` blending mode:

- walk the surviving overlays from back to front,
- compute each source-space clip from the descriptor's device-space clip and
  signed offset,
- add the source rasterizable directly to the stack,
- keep each input in `kSourceOver`,
- get the current clipper order directly.

This is simple and obvious when reading the code.

### Option B: Keep Input Order And Use `kDestinationOver`

The reverse order can also be expressed through blending.

`BlendOp<kDestinationOver>::blend(dst, src)` computes `dst over src`.

So if a stack starts from transparent and every input uses
`kDestinationOver`, then insertion order `o0`, `o1`, `o2` produces:

- first input: `transparent over o0` -> `o0`
- second input: `o0 over o1`
- third input: `(o0 over o1) over o2`

which is exactly the clipper order.

So yes: `kDestinationOver` can preserve the current overlay order without a
reverse traversal, as long as every clipper overlay continues to mean "ordinary
source-over under the previously accumulated overlays".

### Why `kDestinationOver` Is Not The Best Primary Migration Strategy

Even though the math works, using `kDestinationOver` everywhere is not the best
default migration plan.

Costs:

- it hides ordering policy inside a per-input blend mode instead of list order,
- it makes the resulting stack harder to read during debugging,
- it couples clipper ordering to a non-default blend mode on every input,
- and it becomes awkward if clipper ever wants per-layer blend modes that are
  not just ordinary source-over layering.

Benefits:

- no reverse iteration needed,
- no additional stack flag or special-case API needed.

Because clipper already rebuilds the active stack dynamically in `sync()`, the
reverse traversal is cheap and explicit. There is no need to spend semantic
clarity just to avoid iterating a filtered vector backwards.

### Recommendation

For clipper migration, prefer:

- reverse input order during `sync()`,
- default `kSourceOver` inputs.

Treat `kDestinationOver` as a valid fallback or proof that a reverse-order flag
is not required, not as the primary strategy.

## `ClippedOverlay` Refactor And Clip Translation

Signed offsets are handled by the companion design
`stack_signed_offsets_design.md`. After that change, the main semantic gap is
not another wrapper rasterizable. It is just the mapping from clipper's
device-space clip to `RasterizableStack`'s source-space clip.

A converged `ClippedOverlay` should be a simple clipper-owned descriptor that
stores:

- the source `Rasterizable`,
- the device-space clip box,
- and the signed offset.

It may also expose a convenience helper for translated visible extents, but it
should not implement `Rasterizable` itself.

### Current `ClippedOverlay` Semantics

`ClippedOverlay` computes:

`intersect(delegate.extents().translate(dx, dy), device_clip)`

and then samples the delegate in source space by subtracting `dx` and `dy`.

That means the clip box is expressed in destination coordinates after the
translation.

Those are the semantics that matter. They can move into `sync()` without
keeping the wrapper as a read-time composition layer.

### Current `RasterizableStack` Semantics

The existing clipped-offset overload already represents the same data once
clipper supplies the clip in source coordinates:

`source_clip = intersect(delegate.extents(), device_clip.translate(-dx, -dy))`

`stack.addInput(delegate, source_clip, dx, dy)`

That preserves destination-space clipping semantics while letting
`RasterizableStack` apply the offset during its own input compaction pass.

### Options

#### Option A: Translate The Clip Back During `sync()`

Clipper can compute the source-space clip from each overlay descriptor before
calling `addInput()`.

Benefits:

- no new general-purpose stack API,
- removes the wrapper rasterizable dispatch and `bounded_overlays_` in the same
  migration,
- keeps the translation math local to the only known caller that needs this
  exact semantic.
- lets `RasterizableStack::readColors()` write translated coordinates directly
  into the per-input buffers it already builds, instead of having
  `ClippedOverlay::readColors()` rewrite an already compacted batch.

Costs:

- the conversion is easy to get wrong,
- `sync()` owns slightly more geometry math than it does today.

#### Option B: Add A Helper API To `RasterizableStack`

Add a helper that explicitly accepts a destination-space clip plus signed
translation.

Benefits:

- makes the clipper call site direct and less error-prone,
- documents the translated-then-clipped model explicitly.

Costs:

- grows the public composition API for a use case that may remain clipper-only,
- needs matching thought for `StreamableStack` to keep the APIs aligned.

### Recommendation

Do not add a new public clip-space API up front.

For the primary clipper migration, refactor `ClippedOverlay` into a plain
descriptor and let clipper translate the device-space clip back into
source-space stack coordinates during `sync()` before `addInput()`.

If a second caller appears, or if the clipper code becomes hard to reason
about, then add a dedicated helper in both stack types.

## Optimization Analysis

### Optimization 1: Partial Transparent Layer Skip In `readColorRect()`

This was the one meaningful optimization in the current overlay path that the
generic rasterizable stack did not replicate. It is now implemented in
`RasterizableStack`, with one important caveat: the skip is only valid for
blend modes where a transparent source preserves the destination.

Current `OverlayStack::readColorRect()` behavior:

- while the accumulated result is still uniform,
- if a layer only partially covers the queried rectangle,
- ask `readUniformColorRect()` for the clipped part,
- and if that clipped part is uniformly transparent, skip the layer entirely.

Current `RasterizableStack::readColorRect()` behavior:

- while the accumulated result is still uniform,
- if a layer only partially covers the queried rectangle,
- ask `readUniformColorRect()` for the clipped part,
- and skip that layer only when it is uniformly transparent and its blending
  mode treats transparent source as a no-op.

`RasterizableStack::readUniformColorRect()` now applies the same mode-aware
rule for partial transparent layers, so the two paths stay semantically
aligned.

#### Why This Matters For Clipper

Clipper overlays are often things like:

- shadows,
- press overlays,
- smooth shapes with large transparent margins,
- or clipped decorations whose extents intersect the queried rectangle only at
  the edge.

For those cases, a transparent partial layer is common. Skipping it preserves
the uniform fast path longer and avoids unnecessary layer readback and per-pixel
blending work.

Clipper itself still composes overlays with ordinary source-over semantics, so
this mode gate does not reduce the value of the optimization for the current
overlay path. The caveat matters because the generic stack supports broader
Porter-Duff blend modes such as `kClear` and `kSourceIn`, where a transparent
source can still change the destination.

This does not remove the local buffer allocation in `readColorRect()`, because
the buffer is allocated before the loop in both implementations. The benefit is
not stack memory. The benefit is avoiding needless composition work once a
transparent partial layer has already proven that it cannot affect the result.

#### Benefit

- Better behavior for shadow-like and halo-like overlays.
- Better small-rectangle behavior for `ForegroundFilter` consumers.
- The optimization is still generic for the common source-over-like modes used
  by clipper and many other callers.

#### Cost

- One extra virtual `readUniformColorRect()` call for partial layers while the
  stack is still on the uniform fast path.
- Slightly more branching in `RasterizableStack::readColorRect()`.
- A small mode gate is required to avoid regressions for blend modes where a
  transparent source is not a no-op.
- Benefit depends on inputs implementing useful uniform-rect checks.

#### Recommendation

Port this optimization into `RasterizableStack::readColorRect()`, but gate it
by blending mode and keep `readUniformColorRect()` consistent.

This is a good fit for `roo_display` because it improves the general stack,
not just a single downstream caller.

### Optimization 2: Wrapper-Specific Zero-Offset Fast Path

`ClippedOverlay::readColors()` has a tiny special case for `dx == 0 && dy == 0`
that avoids per-sample subtraction.

Once `ClippedOverlay` becomes plain metadata, that special case disappears with
the wrapper. It is not a reason to keep the wrapper layer.

Benefit:

- saves a subtraction-by-zero path inside a wrapper we plan to remove.

Cost:

- keeping an extra composition layer alive for a micro-optimization that the
  stack can handle itself.

Recommendation: do not design around this. If profiling later shows a real
problem, optimize inside `RasterizableStack`, not by preserving a wrapper
`Rasterizable` around each clipper overlay.

### Optimization 3: Range Rebinding, Compaction Buffer, And Direct Rebuild

`OverlayStack` can rebind to a contiguous range of existing overlays with
`reset(begin, end)`.

That is why `ClipperOutput::sync()` currently compacts the active subset into
`bounded_overlays_`: after bounds filtering, the live overlays are not
generally contiguous inside `overlays_`, and `OverlayStack` cannot bind a
sparse subset.

With `RasterizableStack`, neither that extra overlay compaction buffer nor a
wrapper `Rasterizable` is inherently required. `sync()` can iterate
`overlays_` once, test the translated extents against `bounds_`, compute the
source-space clip for each surviving descriptor, and add the source directly to
a reusable stack in reverse order.

That keeps the same asymptotic filtering work while removing the mirrored
`bounded_overlays_` storage, the copy into it, and the extra wrapper dispatch
layer.

Potential benefit of extra stack API such as `clear()` and `reserve()`:

- lets clipper rebuild a reusable `RasterizableStack` directly from
  `overlays_`,
- lets clipper reuse capacity cleanly,
- and avoids allocation churn once the active overlay count stabilizes.

Potential cost:

- small public API expansion in both `RasterizableStack` and `StreamableStack`,
- direct rebuild only removes the overlay compaction buffer; the exclusion path
  still keeps `bounded_exclusions_` unless `RectUnion` also gains a direct
  rebuild API,
- and descriptor-driven rebuild shifts a little more clip math into
  `ClipperOutput::sync()`, which needs clear naming and tests.

Recommendation: during Phase 3, prefer rebuilding `RasterizableStack`
directly from `overlays_`, remove `bounded_overlays_`, and downgrade
`ClippedOverlay` to plain metadata in the same step. `clear()` and `reserve()`
are useful enabling additions, but a range-view stack type is not necessary.

## Recommended Plan

### Phase 1: Signed Offsets

Land `stack_signed_offsets_design.md` first so the generic stack APIs match the
signed coordinate system already used by clipper and the examples.

Current status: complete.

### Phase 2: Port The Transparent Partial-Layer Optimization

Update `RasterizableStack::readColorRect()` so it can skip uniformly
transparent partial layers while it is still on the uniform fast path, but only
for blend modes where transparent source preserves destination.

This should land with focused unit coverage in `roo_display`.

Current status: complete in `roo_display`, with matching mode-aware behavior in
`readUniformColorRect()` and focused coverage in `rasterizable_stack_test`.

### Phase 3: Migrate Clipper To `RasterizableStack`

- keep clipper's ownership and bounds filtering in `roo_windows`,
- refactor `ClippedOverlay` into simple metadata holding source, device-space
  clip, and signed offset,
- rebuild a reusable `RasterizableStack` directly from `overlays_` in
  `ClipperOutput::sync()`,
- compute each input's source-space clip during that rebuild,
- add active overlays in reverse order with default `kSourceOver`,
- drop `bounded_overlays_`,
- and remove `OverlayStack` as part of that same direct rebuild once the stack
  reuse API is in place.

Current status: not started.

### Phase 4: Reassess Whether Clip Translation Should Become Public API

If the migrated clipper is clear and fast enough, stop there.

If the translated-clip rebuild remains noisy or error-prone, then add a small
public helper API to the stack classes. That would generalize the descriptor-to-
stack bridge, not restore a wrapper `Rasterizable`.

Current status: not started.

## Test Plan

For the remaining implementation work, add or extend tests in two places.

### `roo_display`

- keep `rasterizable_stack_test` and `streamable_stack_test` as the core stack
  behavior tests,
- keep focused regression tests for both the transparent-partial-layer fast path
  and the non-no-op blend-mode cases where that skip must not apply.

The cleanest optimization test is a fake rasterizable that:

- reports a uniformly transparent clipped sub-rectangle through
  `readUniformColorRect()`,
- but would fail or count an unexpected call if `readColorRect()` were invoked
  for that same partial transparent case.

That validates the optimization behavior directly rather than inferring it from
rendered pixels alone.

Add one complementary regression for a blend mode such as `kClear`, where a
uniformly transparent partial layer must still be applied instead of skipped.

### `roo_windows`

- add a focused clipper or paint-context test with at least two overlapping
  overlays so the current order is pinned explicitly before migration.
- add one case with a translated and clipped overlay so the descriptor-to-stack
  clip translation is pinned explicitly.

That test should verify the documented clipper rule that earlier-added overlays
stay above later-added overlays.

## Benefit And Cost Summary

### Benefits

- removes duplicate composition logic,
- lets clipper reuse the general stack stream path,
- keeps overlay-order handling explicit,
- removes `bounded_overlays_` and the wrapper rasterizable layer during the
  direct-rebuild migration,
- ports one genuinely useful optimization into the general stack,
- reduces the number of special-purpose composition types maintained across the
  two repositories.

### Costs

- the descriptor-driven rebuild still needs clip translation in
  `ClipperOutput::sync()` unless a helper API is added,
- `RasterizableStack::readColorRect()` becomes slightly more complex,
- public API may grow modestly if reusable `clear()` and `reserve()` helpers
  are added to support direct stack rebuilds cleanly,
- migration needs new cross-repo regression coverage because the behavior spans
  `roo_display` and `roo_windows`.

## Validation Plan

For the remaining implementation work, validate in this order:

- `bazel test :rasterizable_stack_test`
- `bazel test :read_uniform_color_rect_test`
- `bazel test :foreground_filter_test`
- `bazel test :overlay_test :paint_context_test` in `roo_windows`

If the public stack headers change again during the migration, also run:

- `bazel test :products_compile_test`

## Proposed Commit Message

`rasterizable_stack_overlay_convergence_design.md`: target direct clipper
rebuilds from overlay metadata.

Update the convergence design so Phase 3 removes `bounded_overlays_` and
`OverlayStack` by rebuilding a reusable `RasterizableStack` directly from
clipper-owned overlay descriptors in reverse order. Refactor `ClippedOverlay`
into plain metadata carrying source, device-space clip, and signed offset, then
translate each surviving overlay into `RasterizableStack::addInput()` during
`sync()` instead of preserving a wrapper `Rasterizable` layer.