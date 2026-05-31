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

Phases 3 and 4 remain pending.

## Objective

Bring `roo_windows` overlay composition closer to the general `roo_display`
stack abstractions so that `roo_windows::OverlayStack` can eventually be
replaced with `roo_display::RasterizableStack` without losing current behavior
or important hot-path optimizations.

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
  concerns: bounds filtering, overlay ownership, and clip translation.

The remaining differences are real, but they are narrow:

- clipper wants earlier-added overlays to stay above later-added overlays,
- clipper clips in device space after translation,
- and clipper currently compacts active overlays into a contiguous buffer only
  because `OverlayStack` rebinds by range.

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

That means it can populate a reusable `RasterizableStack` in reverse order and
keep the default `kSourceOver` blending mode:

- push `bounded_overlays_` from back to front,
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

## `ClippedOverlay` API Gap Analysis

Signed offsets are handled by the companion design
`stack_signed_offsets_design.md`. After that change, the main semantic gap is
the clip space.

### Current `ClippedOverlay` Semantics

`ClippedOverlay` computes:

`intersect(delegate.extents().translate(dx, dy), device_clip)`

and then samples the delegate in source space by subtracting `dx` and `dy`.

That means the clip box is expressed in destination coordinates after the
translation.

### Current `RasterizableStack` Semantics

The existing clipped-offset overload effectively clips the source extents before
the translation.

For clipper, that is not a drop-in replacement.

### Options

#### Option A: Translate The Clip Back At The Call Site

Clipper can convert its device-space clip into source-space stack coordinates by
applying `clip_box.translate(-dx, -dy)` before calling `addInput()`.

Benefits:

- no new general-purpose stack API,
- keeps the translation math local to the only known caller that needs this
  exact semantic.

Costs:

- the conversion is easy to get wrong,
- the call site is less self-explanatory than today's `ClippedOverlay`.

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

For the first clipper migration attempt, let clipper translate the device-space
clip back into source-space stack coordinates before `addInput()`.

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

### Optimization 2: Zero-Offset Wrapper Fast Path

`ClippedOverlay::readColors()` has a tiny special case for `dx == 0 && dy == 0`
that avoids per-sample subtraction.

This is not worth elevating into public `RasterizableStack` API design.

Benefit:

- saves a subtraction-by-zero path.

Cost:

- more special casing in a general stack abstraction.

Recommendation: do not design around this. If profiling later shows a real
problem, add a local micro-optimization in the implementation, not a new public
concept.

### Optimization 3: Range Rebinding, Compaction Buffer, And Direct Rebuild

`OverlayStack` can rebind to a contiguous range of existing overlays with
`reset(begin, end)`.

That is why `ClipperOutput::sync()` currently compacts the active subset into
`bounded_overlays_`: after bounds filtering, the live overlays are not
generally contiguous inside `overlays_`, and `OverlayStack` cannot bind a
sparse subset.

With `RasterizableStack`, that extra overlay compaction buffer is not
inherently required. `sync()` can iterate `overlays_` once, test
`extents().intersects(bounds_)`, and add each surviving overlay directly to a
reusable stack in reverse order.

That keeps the same asymptotic filtering work while removing the mirrored
`bounded_overlays_` storage and the copy into it.

A practical incremental path exists even before clipper translates device-space
clips back into source-space stack coordinates: reuse the existing
`ClippedOverlay` wrappers as the stack inputs. That would let clipper replace
`OverlayStack` directly and drop `bounded_overlays_` first, while leaving
`ClippedOverlay` removal as a later step.

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
- and keeping `ClippedOverlay` as the first migration step preserves its local
  wrapper behavior, so that step is convergent but not yet the final form.

Recommendation: during Phase 3, prefer rebuilding `RasterizableStack`
directly from `overlays_` and treat `bounded_overlays_` as removable migration
scaffolding. `clear()` and `reserve()` are useful enabling additions, but a
range-view stack type is not necessary.

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
- rebuild a reusable `RasterizableStack` directly from `overlays_` in
  `ClipperOutput::sync()`,
- add active overlays in reverse order with default `kSourceOver`,
- drop `bounded_overlays_` as part of that direct rebuild when the stack reuse
  API is in place,
- and either translate device-space clip boxes back into source-space stack
  coordinates when removing `ClippedOverlay`, or use existing
  `ClippedOverlay` wrappers as a short-lived first migration step.

Current status: not started.

### Phase 4: Reassess Whether `ClippedOverlay` Still Adds Value

If the migrated clipper is clear and fast enough, remove `OverlayStack` and
possibly `ClippedOverlay`.

If the translated-clip call sites remain noisy or error-prone, then add a small
public helper API to the stack classes and remove the extra wrapper after that.

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

That test should verify the documented clipper rule that earlier-added overlays
stay above later-added overlays.

## Benefit And Cost Summary

### Benefits

- removes duplicate composition logic,
- lets clipper reuse the general stack stream path,
- keeps overlay-order handling explicit,
- can remove the extra `bounded_overlays_` mirror during the direct-rebuild
  migration,
- ports one genuinely useful optimization into the general stack,
- reduces the number of special-purpose composition types maintained across the
  two repositories.

### Costs

- fully removing `ClippedOverlay` still needs call-site clip translation unless
  a helper API is added,
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

`rasterizable_stack_overlay_convergence_design.md` Phase 2: port clipper-useful transparent partial-layer skipping into RasterizableStack.

Teach `RasterizableStack::readColorRect()` to keep its uniform fast path when a
partially covering input proves uniformly transparent, but only for blend modes
where transparent source preserves destination, matching the one overlay-
specific optimization in `roo_windows::OverlayStack` that is worth
generalizing. The follow-up clipper migration should then rebuild a reusable
`RasterizableStack` directly from `overlays_` in reverse input order with
default `kSourceOver`, dropping `bounded_overlays_` instead of encoding clipper
ordering through `kDestinationOver` on every layer.