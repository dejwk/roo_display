# Clip Exclude Rects Scanline Skip Design

## Status

- Proposed design.
- No code changes are part of this document.
- Scope is limited to `roo_display::RectUnion` and the partial-exclusion slow
  path in `RectUnionFilter`.

## Goal

Reduce the cost of exclusion clipping in
`roo_display/filter/clip_exclude_rects.h` when `RectUnionFilter::write()` and
`RectUnionFilter::fill()` operate in the partial-exclusion slow path.

Today, that path evaluates `RectUnion::contains(x, y)` for every pixel in the
address window. With a non-trivial number of exclusion rectangles, that makes
the cost roughly proportional to:

- pixels written, times
- exclusion rectangles scanned per point query.

The goal is to preserve behavior while allowing callers to skip over horizontal
runs of pixels for which the exclusion answer is known not to change.

## Motivation

The current partial-exclusion path is correct but pessimistic. On a given
scanline, the exclusion answer often remains constant for many consecutive
pixels:

- a visible run before the next excluded rectangle begins,
- a covered run inside a rectangle that spans a horizontal interval,
- or a long suffix of the row when no future exclusion begins on that `y`.

Per-pixel `contains()` queries throw away that regularity.

This matters in `roo_windows`, where the clipper can accumulate multiple
rectangular exclusions and then route address-window rendering through
`RectUnionFilter`. Even if the output side remains buffered, reducing the
number of `RectUnion` scans should lower CPU cost materially when the address
window covers large spans with only a few exclusion transitions.

## Current Behavior

`RectUnion` is currently a flat range of boxes with linear-scan geometry
queries:

- `contains(int16_t x, int16_t y)` returns `true` on the first containing box,
- `intersects(const Box&)` returns `true` on the first intersecting box,
- `contains(const Box&)` returns `true` if any single box fully contains the
  input rectangle.

`RectUnionFilter::setAddress()` already has a fast classification step:

- no exclusion intersects the address window: pass through,
- one exclusion fully contains the address window: drop the whole write,
- otherwise: use the partial-exclusion slow path.

The slow path in `write()` and `fill()` currently advances pixel-by-pixel,
calling `contains(cursor_x_, cursor_y_)` for each location.

## Proposed API Change

Extend only the point-query overload of `contains()`:

```cpp
bool contains(int16_t x, int16_t y, size_t* same_count = nullptr) const;
```

Semantics:

- The boolean result remains exactly the current point-membership answer.
- When `same_count` is non-null, the callee writes a best-effort weak lower
  bound on the number of consecutive pixels starting at `(x, y)` and extending
  in `+x` for which the same boolean answer is guaranteed to hold.
- The reported count is inclusive of the starting pixel, so it is always at
  least `1` for non-empty input state.
- The bound may be conservative. Larger values are better, but correctness only
  requires that the answer not change within the reported run.

This proposal intentionally does not extend the rectangle overloads of
`contains()` or `intersects()`. Their current semantics are generic geometry
queries, while the new hint is specifically a scanline run-length property.

## One-Pass Algorithm

The implementation should stay single-pass over the rectangle range.

For a query `(x, y)`:

1. Initialize:
   - `contains = false`
   - `count = 1`
   - `next_xmin = +infinity`
2. For each box in the union:
   - Ignore boxes whose `y` range does not contain `y`.
   - If the box contains `(x, y)`, return `true` immediately and, if requested,
     set `count` to `box.xMax() - x + 1`.
   - Otherwise, if `box.xMin() > x`, update `next_xmin` to the minimum such
     value seen so far.
3. If no containing box is found, return `false` and set:
   - `count = next_xmin - x` when a future start on this scanline exists,
   - otherwise `count` to a conventional large sentinel representing "no known
     transition on this scanline".

This matches the intended interpretation of "track the nearest `box.xMin()` in
that same pass".

### Early Return Constraint

The `true` case should respect the current early-return behavior.

That means once a containing box is found, the method does not continue looking
for overlapping or adjacent boxes that could extend the covered run farther to
the right. As a result:

- the returned `true` count may be smaller than the maximal covered run,
- but it remains correct as a lower bound,
- and the algorithm retains the same short-circuit structure as today's point
  query.

The `false` case does not have an early return today, so it can use the same
pass to track the nearest future `xMin()` on the queried scanline.

In practice, the implementation does not need a mathematically infinite value
for the no-future-transition case. The exclusion rectangles are clipped to
device resolution, with `1024x600` being the largest device used so far, and
callers clamp the reported run to the remaining row width and source span.
Any conventional high value comfortably above the maximum scanline width is
sufficient.

## Caller Integration

`RectUnionFilter::write()` and `RectUnionFilter::fill()` should continue to use
the current fast-path classification from `setAddress()`. The change only
affects the partial-exclusion slow path.

For each loop iteration:

1. Query `contains(cursor_x_, cursor_y_, &same_count)`.
2. Clamp `same_count` to:
   - the remaining pixels in the current scanline,
   - and the remaining pixels in the source span.
3. If the answer is excluded:
   - skip source pixels in `write()`,
   - or skip emitted pixels in `fill()`,
   - then advance the cursor by the clamped run length.
4. If the answer is visible:
   - process that whole run without re-checking exclusion membership,
   - then advance the cursor by the same run length.

This converts the work from per-pixel classification to per-run
classification.

The implementation may also carry forward any excess run length returned by the
previous `contains()` call. For example, if the caller opens an address window
that extends to the end of the current scanline but only consumes part of the
reported visible run in the current loop iteration, the unused suffix can be
cached and consumed by subsequent iterations without re-querying `contains()`.

## Integration Options For Visible Runs

There are two viable implementation levels.

### Option A: classification-only optimization

Keep the existing buffered pixel writer / filler path and simply avoid calling
`contains()` more than once per run.

Pros:

- minimal change to data flow,
- low correctness risk,
- easy to compare against the current implementation.

Cons:

- visible pixels still route through arbitrary-pixel buffering,
- so the improvement is limited to exclusion-query cost.

### Option B: address-window run forwarding

For visible horizontal runs, open an address window that extends optimistically
to the end of the current scanline and forward visible pixels with `write()` /
`fill()` directly to the wrapped output.

This is safe even when the caller does not ultimately write all pixels covered
by the window. The wrapped output may receive a larger row-bounded address
window than is consumed immediately, as long as the caller tracks how many
pixels were actually written or skipped.

The intended control flow is:

1. when `contains()` returns `false`, open or keep an address window that runs
   to the end of the scanline,
2. consume the currently available visible run,
3. cache any excess visible-run length returned by the previous `contains()`
   call,
4. in subsequent iterations, keep writing or skipping against the same window
   while cached visible-run length remains or while new `contains()` calls keep
   returning `false`,
5. only break and restart when the cached run is exhausted, the scanline ends,
   or `contains()` reports an excluded segment.

This lets the caller amortize both `contains()` queries and address-window
setup across long visible prefixes and suffixes of the row.

Pros:

- preserves contiguous-address semantics for visible runs,
- can keep a single optimistic address window alive across multiple visible
  runs on the same scanline,
- can reuse excess run-length information from earlier `contains()` results,
- may reduce both exclusion-query cost and output overhead,
- likely gives the larger real-world win for `fill()` and sequential `write()`.

Cons:

- more control-flow complexity,
- requires careful accounting of how much of the optimistic window has actually
  been consumed by writes versus skips,
- requires care to restart cleanly at row boundaries and exclusion-state
  transitions.

The recommended implementation order is:

1. land Option A first,
2. measure,
3. then decide whether Option B is justified.

## Non-Goals

- Reordering, merging, or normalizing the exclusion rectangle set.
- Changing the semantics of `RectUnion::contains(const Box&)` or
  `RectUnion::intersects(const Box&)`.
- Adding caching across separate `contains()` calls.
- Optimizing `writePixels()`, `fillPixels()`, `writeRects()`, or `fillRects()`
  in this change.

## Correctness Considerations

### Conservative lower bound only

The returned run length must be treated only as a guaranteed minimum region of
equal answer. It must not be interpreted as the maximal run length.

### Row-boundary clamping

The caller must clamp the returned run to the remaining width of the current
address-window row. A valid same-answer run on the scanline does not imply the
same answer after wrapping to the next row.

### Source advancement in `write()`

Excluded runs still consume input colors. The implementation must advance the
source pointer or index by the skipped run length even when no pixels are
emitted.

### Overlap and adjacency

The exclusion set is only lightly folded by the `roo_windows` clipper. It must
therefore be treated as potentially:

- overlapping,
- adjacent,
- unordered except for insertion order,
- and not merged into maximal intervals.

The weak-lower-bound contract is chosen specifically so the optimization stays
correct under those conditions.

## Testing Plan

Add focused unit tests for both the query primitive and the filter behavior.

### `RectUnion` point-query tests

Create or extend tests around `clip_exclude_rects.h` to verify:

- empty union returns `false` with a large or row-clamped usable count,
- single box, point outside before the box on the same row,
- single box, point inside the box,
- two disjoint boxes on the same row,
- overlapping boxes where the first hit returns a conservative `true` count,
- adjacent boxes where the first hit may stop before the next box begins,
- boxes on other rows do not affect the count for the queried `y`,
- count is always at least `1`.

These tests should validate the lower-bound contract rather than require a
maximal run length.

### `RectUnionFilter::write()` / `fill()` behavior tests

Add tests that render through `RectUnionFilter` and assert identical output to
the current behavior for:

- partially excluded single-row writes,
- partially excluded multi-row writes,
- row transitions where a run must be clamped at the row end,
- exclusion runs at the start, middle, and end of the row,
- full visibility and full exclusion still taking the existing fast paths,
- `write()` consuming the correct source colors after skipped excluded runs.

The tests should compare rendered output, not internal iteration counts.

### Optional instrumentation tests

If useful during development, add temporary test-only counters to show that the
optimized path reduces the number of `contains()` calls on representative
patterns. This is useful for bring-up but should not be required for the final
correctness suite.

## Benchmark Plan

Add a dedicated benchmark under `lib/roo_display/benchmarks`, rather than
folding this into an unrelated benchmark.

Suggested file:

- `lib/roo_display/benchmarks/clip_exclude_rects.ino`

The benchmark should exercise the exact optimization target: address-window
`write()` and `fill()` under `RectUnionFilter` with partial exclusions.

### Benchmark scenarios

Include deterministic scenarios such as:

- long visible row with one small excluded interval,
- alternating visible and excluded bands on the same scanline,
- multiple rows with identical exclusion layout,
- dense small exclusions that force many transitions,
- a pessimistic case where almost every few pixels change state,
- text rendered at several font sizes using `kExtents`, so glyph drawing is
  forced through the address-window `write()` pattern that this optimization is
  targeting.

Each scenario should measure both:

- `fill()` performance,
- `write()` performance with a source color buffer,
- and for text scenarios, end-to-end time for drawing the same strings at
  multiple representative font sizes.

### Measurement strategy

For each scenario, report:

- total elapsed microseconds for a fixed iteration count,
- average microseconds per frame or per operation,
- and optionally derived throughput such as pixels per microsecond.

### Baseline comparison

The benchmark should be implemented first and then used to measure the current
code before any optimization lands.

After the optimization is implemented, the same benchmark should be rerun with
the same seed, strings, font sizes, and device setup.

Re-measuring the pre-optimization baseline remains possible later by checking
out an older git snapshot and rerunning the same benchmark unchanged.

### Environments

Run the benchmark in at least two environments:

- the existing `ROO_TESTING` emulator / offscreen-friendly setup for quick
  iteration,
- and a representative hardware target where CPU overhead is visible.

The emulator will be useful for deterministic comparisons. Hardware numbers are
needed because the visible gain may differ depending on whether the bottleneck
is CPU-side clipping or device-side transfer. This is especially relevant for
text scenarios, where font size changes may shift the balance between clipping
cost and device-transfer cost.

## Expected Outcome

The primary expected benefit is fewer `RectUnion` scans in the partial-exclusion
slow path. That should improve workloads where:

- address-window rendering covers large horizontal spans,
- only a few exclusion transitions occur per row,
- and exclusion count is large enough that per-pixel scanning is noticeable.

The improvement may be modest in workloads dominated by output transfer cost or
in adversarial cases with extremely frequent visibility transitions. That is why
the benchmark plan includes both favorable and pessimistic layouts.

## Open Questions

- Whether Option A already captures most of the practical gain.
- Whether a later row-forwarding optimization should be added for visible runs.