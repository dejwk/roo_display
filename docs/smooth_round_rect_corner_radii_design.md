# Smooth Round Rect Corner Radii Design

## Objective

Add a new smooth rounded-rectangle API in `roo_display/shape/smooth.h` that
allows the four corner radii to be specified individually while preserving the
current equal-radius behavior, performance, and `SmoothShape` object size.

## Motivation

The current smooth round-rect API only supports one radius shared by all four
corners. That is sufficient for many controls, but it does not cover shapes
such as cards, callouts, sheets, pills with one squared edge, or transitions
between components with different top and bottom curvature.

The new API should fit naturally into the existing smooth-shape family. It
should look like a round-rect call, but take a small radii struct instead of a
single scalar.

## Background

Smooth round rects are currently implemented as a dedicated `SmoothShape`
variant in `roo_display/shape/smooth.h` and `roo_display/shape/smooth.cpp`.
The equal-radius path has specialized support for:

- construction and geometry normalization,
- per-pixel color evaluation,
- rectangle classification for readback and drawing,
- a dedicated `PixelStream` fast path,
- and a dedicated draw path.

That specialization matters because round rects are common, and the equal-
radius version already has a tuned path that should not become slower just
because a more general form exists.

### Existing Footprint Estimate On ESP32

Assuming ESP32-class targets with 32-bit `float`, 32-bit `Color`, 8-byte
`Box`, 4-byte enum storage, and normal 4-byte struct alignment:

| Type | Estimated size |
| --- | ---: |
| `Color` | 4 bytes |
| `Box` | 8 bytes |
| `SmoothShape::RoundRect` | 64 bytes |
| `SmoothShape::Arc` | 116 bytes |
| `SmoothShape` | 128 bytes |

The current equal-radius `RoundRect` estimate is:

- 8 floats for geometry and cached squared radii: 32 bytes,
- 2 colors: 8 bytes,
- 3 helper boxes: 24 bytes.

Total: about 64 bytes.

`SmoothShape` itself is currently dominated by the `Arc` union member rather
than by `RoundRect`. That is important because it means there is some storage
headroom for a new round-rect payload, but not enough to add arbitrary cached
state without checking the union ceiling.

## Requirements

- The public API must remain aligned with the smooth-shape family and return
  `SmoothShape`.
- The new API must accept a struct rather than four separate radius arguments.
- The radii struct should support positional aggregate use such as
  `RoundRectRadii{2, 3, 4, 5}`.
- The struct should expose named fields so designated initialization is
  possible on toolchains that support it, for example
  `RoundRectRadii{.tl = 2, .tr = 3, .bl = 4, .br = 5}`.
- The existing equal-radius overloads must remain available.
- When the effective radii are equal after normalization, the implementation
  must reuse the current equal-radius path.
- The unequal-radius implementation must be separate so the common equal-
  radius case does not pay additional per-pixel or per-stream overhead.
- Internal helper reuse is desirable, but not at the cost of slowing down the
  equal-radius path.
- The design should avoid increasing `sizeof(SmoothShape)`.
- The design should document RAM impact and rendering-cost tradeoffs.
- Public documentation should be updated if the feature is implemented.

## Design Overview

Introduce a new public value type, `RoundRectRadii`, and add overloads of the
smooth round-rect factory functions that accept it by `const&`.

Implementation splits into two paths:

- equal effective radii: reuse the existing `SmoothShape::RoundRect` path,
- unequal effective radii: create a new `SmoothShape` kind with its own
  payload and helper functions.

The unequal-radius path will share low-level geometry utilities where doing
so does not introduce extra branching into the current equal-radius fast path.
At a minimum, construction-time normalization and some corner-distance math can
be shared. The top-level streaming, readback, and drawing helpers remain
separate.

The unequal-radius variant uses the available union headroom for values that
are used by the hot geometry tests. It stores outer boundary extents, four
outer radii, the outline width, per-corner adjusted squared radii for both the
outer and derived inner boundaries, two colors, and five guaranteed-interior
helper boxes. It intentionally does not store the four inner radii: each inner
radius is always `max(0, outer_radius - thickness)`, and deriving that value is
cheaper than recomputing the squared-radius thresholds in every evaluator,
classifier, and stream row.

The unequal-radius payload therefore remains at the current `Arc` union
ceiling. There is no additional coarse cache in the selected layout. Empty
helper boxes represent degenerate cases, and the adjusted-square caches are the
only extra per-corner cache beyond the public geometry.

## Design Details

### Public Radii Type

Add a small aggregate type in `roo_display/shape/smooth.h`:

```cpp
struct RoundRectRadii {
  float tl;
  float tr;
  float bl;
  float br;
};
```

The field order follows the intended aggregate call style shown above. The
implementation will treat the fields by name rather than by any geometric
ordering assumption.

The public overloads take the radii by `const RoundRectRadii&`.
This keeps aggregate-call syntax, binds cleanly to temporaries, and avoids an
unconditional 16-byte by-value copy at each public API entry point.

### Geometry Semantics

The new overloads keep the current smooth round-rect semantics:

- `x0`, `y0`, `x1`, and `y1` describe the outline centerline extents,
- `thickness` expands the outer boundary by `thickness / 2`,
- the inner boundary shrinks by `thickness / 2`,
- and filled shapes are represented as the zero-thickness special case.

Corner radii apply to the outer boundary after the same extents and thickness
normalization that the current equal-radius code uses.

### Radius Normalization

Normalize the input as follows:

1. Clamp all input radii to be non-negative.
2. Normalize rectangle ordering so `x0 <= x1` and `y0 <= y1`.
3. Apply the same thickness expansion that the current implementation uses.
4. Compute the outer width and height.
5. If the four corner radii do not fit, scale them by a single factor:

```text
scale = min(
  1,
  width  / (tl + tr),
  width  / (bl + br),
  height / (tl + bl),
  height / (tr + br))
```

6. Derive inner radii as `max(0, outer_radius - thickness)` for each corner.

Using one global scale factor preserves proportions and guarantees that all
adjacent pairs fit simultaneously.

### Equal-Radius Fold

After normalization, if the effective outer radii are all equal and the
effective inner radii are all equal, dispatch to the existing
`SmoothThickRoundRect()` builder.

This keeps:

- the current equal-radius payload,
- the current dedicated `RoundRectStream`,
- the current readback helpers,
- and the current tuned draw path.

The fold must happen after normalization rather than on raw input so cases
that start unequal but clamp to the same effective radii still use the
existing implementation.

### New Internal Shape Kind

Add a new `SmoothShape` kind for unequal corner radii, for example
`ROUND_RECT_CORNERS`, together with a separate payload struct.

The new payload stores:

- outer boundary extents after thickness expansion,
- 4 outer radii,
- outline width,
- 4 precomputed outer adjusted squared radii,
- 4 precomputed inner adjusted squared radii,
- outline and interior colors,
- and 5 precomputed maximal interior helper boxes.

One practical layout is:

- 4 floats for outer boundary extents: 16 bytes,
- 4 floats for outer radii: 16 bytes,
- 1 float for thickness: 4 bytes,
- 4 floats for outer adjusted squared radii: 16 bytes,
- 4 floats for inner adjusted squared radii: 16 bytes,
- 2 colors: 8 bytes,
- 5 helper boxes (`inner_core`, `inner_top`, `inner_bottom`, `inner_left`,
  `inner_right`): 40 bytes.

Estimated total: 116 bytes, matching the current `SmoothShape::Arc` estimate.

That is about 52 bytes larger than the current equal-radius `RoundRect`
payload, but it does not increase `sizeof(SmoothShape)` as long as `Arc`
continues to define the union ceiling.

For each corner `q`, construction computes:

```text
ro_q = normalized outer radius
ri_q = max(0, ro_q - thickness)
ro_sq_adj_q = ro_q * ro_q + 0.25
ri_sq_adj_q = ri_q * ri_q + 0.25
```

Only `ro_q`, `thickness`, `ro_sq_adj_q`, and `ri_sq_adj_q` are stored. The
derived `ri_q` is reconstructed as a local when a corner or row needs it.

### Footprint Delta Discussion

There are two relevant footprint questions.

First, the payload itself grows for the unequal-radius variant:

- current equal-radius round-rect payload: about 64 bytes,
- proposed unequal-radius payload: about 116 bytes,
- payload delta: about 52 bytes.

Second, the actual object footprint of `SmoothShape` remains unchanged:

- current `SmoothShape`: about 128 bytes,
- proposed `SmoothShape`: still about 128 bytes,
- object-size delta: 0 bytes.

This works only if the new payload does not grow past the existing `Arc`
member size. The selected layout spends all of the useful headroom on the
per-corner adjusted-square caches, because those values are read in every
corner pixel test, corner-rectangle classifier, and stream row. The layout does
not reserve space for stream-specific boundary trackers, precomputed per-row
state, maximum/minimum coarse squared-radius caches, or flags. Those values are
either derivable from the stored fields or local to a stream instance, where
they do not affect `SmoothShape` size.

The implementation must include a compile-time guard such as:

```cpp
static_assert(sizeof(SmoothShape::RoundRectCorners) <=
              sizeof(SmoothShape::Arc));
```

The storage decision does not require a microbenchmark gate. The chosen cache
values feed the same threshold comparisons used by the current equal-radius
implementation, and the derived inner radii are reconstructed locally from the
stored outer radii and thickness. Caveats records the rejected storage
alternatives.

### Interior Slabs And Helper Boxes

Precompute 5 axis-aligned boxes that are guaranteed to lie entirely inside the
inner shape:

- `top_slab`,
- `bottom_slab`,
- `left_slab`,
- `right_slab`,
- `inner_core`.

Construction computes derived inner radii for the slab calculations; these are
construction locals and are not stored in the payload.

Let the normalized inner rectangle after thickness inset be:

- `L`, `T`, `R`, `B` for the inset edges,
- `ctl = (L + ri_tl, T + ri_tl)`,
- `ctr = (R - ri_tr, T + ri_tr)`,
- `cbl = (L + ri_bl, B - ri_bl)`,
- `cbr = (R - ri_br, B - ri_br)`.

Define the inner-boundary support functions:

```text
top_inside(x) = max(
  T,
  x < ctl.x ? ctl.y - sqrt(max(0, ri_tl^2 - (x - ctl.x)^2)) : T,
  x > ctr.x ? ctr.y - sqrt(max(0, ri_tr^2 - (x - ctr.x)^2)) : T)

bottom_inside(x) = min(
  B,
  x < cbl.x ? cbl.y + sqrt(max(0, ri_bl^2 - (x - cbl.x)^2)) : B,
  x > cbr.x ? cbr.y + sqrt(max(0, ri_br^2 - (x - cbr.x)^2)) : B)

left_inside(y) = max(
  L,
  y < ctl.y ? ctl.x - sqrt(max(0, ri_tl^2 - (y - ctl.y)^2)) : L,
  y > cbl.y ? cbl.x - sqrt(max(0, ri_bl^2 - (y - cbl.y)^2)) : L)

right_inside(y) = min(
  R,
  y < ctr.y ? ctr.x + sqrt(max(0, ri_tr^2 - (y - ctr.y)^2)) : R,
  y > cbr.y ? cbr.x + sqrt(max(0, ri_br^2 - (y - cbr.y)^2)) : R)
```

Then define the helper boxes as the maximal guaranteed-interior boxes for the
chosen anchor intervals:

- `top_slab = [ctl.x, ctr.x] x [T,
  min(bottom_inside(ctl.x), bottom_inside(ctr.x))]`,
- `bottom_slab = [cbl.x, cbr.x] x
  [max(top_inside(cbl.x), top_inside(cbr.x)), B]`,
- `left_slab = [L, min(right_inside(ctl.y), right_inside(cbl.y))] x
  [ctl.y, cbl.y]`,
- `right_slab = [max(left_inside(ctr.y), left_inside(cbr.y)), R] x
  [ctr.y, cbr.y]`.

Define:

```text
diag(r) = r - r / sqrt(2)
```

and then:

- `inner_core = [L + max(diag(ri_tl), diag(ri_bl)),
  R - max(diag(ri_tr), diag(ri_br))] x
  [T + max(diag(ri_tl), diag(ri_tr)),
  B - max(diag(ri_bl), diag(ri_br))]`.

These boxes are intentionally large and they are expected to overlap heavily.
That overlap is beneficial: each box is a cheap guaranteed-interior accept, and
their union covers most of the non-AA interior for many asymmetric shapes.

Any helper box may still become empty for thin or degenerate geometries. In
that case it is simply omitted.

The intended layout is illustrated here:

![Helper slab layout](images/smooth_round_rect_corner_slabs.svg)

### Pixel Evaluator And Rendering Strategy

The unequal-radius implementation uses a separate helper family from the
current equal-radius round-rect helpers.

The per-pixel evaluator uses this fixed decision order:

1. If the pixel is in any precomputed interior helper box, return the interior
   color.
2. If the pixel is outside the outer boundary extents by at least the AA margin,
   return transparent.
3. If the pixel lies in exactly one corner quadrant, evaluate that corner's
   quarter-annulus using the selected corner center, stored outer radius,
   derived inner radius, and cached adjusted-square values.
4. Otherwise evaluate the nearest straight edge band using the corresponding
   outer edge and the inner edge offset by `thickness`.

The corner path mirrors the current equal-radius tests:

```text
d_sq = dx * dx + dy * dy
ri = max(0, ro - thickness)

fully_inside_inner = ri >= 0.5 && d_sq <= ri_sq_adj - ri
fully_outside_outer = d_sq >= ro_sq_adj + ro
fully_inside_outer = d_sq <= ro_sq_adj - ro
fully_outside_inner = ro == ri || d_sq >= ri_sq_adj + ri
```

Only pixels that are not classified by these comparisons compute `sqrt(d_sq)`
for anti-aliasing. The straight-edge path uses signed distance to the outer and
inner horizontal or vertical edge, so it does not need a square root.

The draw helper follows the current round-rect structure: classify the requested
box, fill uniform boxes directly, and subdivide unresolved boxes into 8x8 tiles
before falling back to per-pixel evaluation. The equal-radius draw helper stays
unchanged and remains exclusive to the existing `ROUND_RECT` kind.

Geometrically, the shape is treated as:

- a guaranteed interior core,
- four maximal overlapping helper slabs,
- four straight edge regions between adjacent corner centers,
- and four quarter-annulus corners with independent radii.

Only the AA transition zones need expensive per-pixel evaluation.

### Readback And Rectangle Classification

The unequal-radius kind must implement all three readback paths:

- `readColors()` checks the helper boxes first, then calls the same per-pixel
  evaluator used by drawing.
- `readUniformColorRect()` uses a conservative classifier and never scans the
  rectangle. It returns a color only when the rectangle is guaranteed to be
  fully transparent, fully interior, or fully outline.
- `readColorRect()` uses the same classifier, but when a rectangle is mixed it
  fills the output by subdividing into slabs and 8x8 tiles, so only unresolved
  boundary tiles degrade to per-pixel evaluation.

This matters for composition. `RasterizableStack::readColorRect()` asks each
layer for a rectangle, blends uniform layers cheaply, and otherwise allocates a
full rectangle buffer. A good unequal-radius implementation must therefore
return uniform results for the common transparent, interior, and outline
rectangles, and keep mixed buffers limited to the true edge area.

The rectangle classifier uses this order:

1. Test containment in the five precomputed interior helper boxes.
2. Test transparent rectangles outside the outer edge or fully outside one
   corner quadrant.
3. Test rectangles wholly inside a straight edge band.
4. Test rectangles wholly inside one corner quadrant with corner-local squared
   distance bounds.
5. Return non-uniform for rectangles that cross multiple curved boundaries or
   intersect an AA transition.

For a rectangle wholly in one corner quadrant, classification is constant-time.
Let `nearest_sq` be the squared distance from the corner center to the closest
point of the rectangle and `farthest_sq` be the squared distance to the farthest
rectangle corner. Then:

- `nearest_sq >= ro_sq_adj + ro` means fully transparent,
- `farthest_sq <= ri_sq_adj - ri` means fully interior,
- `farthest_sq <= ro_sq_adj - ro` and
  `nearest_sq >= ri_sq_adj + ri` means fully outline,
- otherwise the rectangle is mixed.

No square root is required for these rectangle decisions. The cached
`ro_sq_adj` and `ri_sq_adj` values are what keep the corner classifier cheap
enough for repeated stack composition and draw subdivision calls.

For `readColorRect()`, mixed rectangles are split first by the precomputed
interior slabs and by the corner-center lines that separate top, bottom, left,
right, and corner regions. Fully classified subrectangles are filled directly.
Remaining subrectangles are subdivided into 8x8 tiles, matching the existing
round-rect draw strategy; only tiles whose classifier remains non-uniform are
evaluated pixel by pixel.

Approximate per-pixel degradation is proportional to boundary length, not
rectangle area. For a query rectangle `Q` and 8x8 tile size `S = 8`, the number
of pixels that fall back to per-pixel evaluation is approximately:

```text
slow_pixels(Q) ~= min(area(Q), S * (length(Q intersect outer_boundary) +
                                   length(Q intersect inner_boundary)))
```

plus a small constant number of tiles where a boundary enters or exits a slab.
For a full outlined unequal round rect with outer width `W`, outer height `H`,
outer radii `ro_i`, and inner radii `ri_i`, the boundary lengths are
approximately:

```text
P_outer = 2W + 2H - (2 - pi / 2) * sum(ro_i)
P_inner = 2(W - 2t) + 2(H - 2t) - (2 - pi / 2) * sum(ri_i)
```

The full-shape readback slow-pixel budget is therefore roughly
`8 * (P_outer + P_inner)` for outlined shapes, and roughly `8 * P_outer` for
filled shapes. For small query rectangles the `min(area(Q), ...)` term keeps
the estimate bounded by the requested rectangle size.

### Streaming Strategy

The equal-radius `RoundRectStream` remains unchanged and exclusive to the
existing equal-radius kind.

For the new unequal-radius kind, the design includes a dedicated stream
override rather than relying on the generic `Rasterizable::createStream()`
fallback.

Reasons:

- it keeps the common equal-radius stream path untouched,
- it avoids regressing a common background and tiling use case,
- and it keeps stream behavior aligned with the specialized equal-radius path.

The unequal-radius stream provides a shape-specific row-major emission path
that classifies transparent, interior, outline, and slow runs without per-pixel
virtual fallback for the whole surface. Each scanline is split at the relevant
corner-center x positions for that row, then each segment is classified with
the same straight-edge and corner-threshold logic used by the rectangle
classifier. The stream keeps row-local boundary trackers in the stream object,
not in `SmoothShape`, so stream optimization does not consume persistent shape
payload.

The implementation target is the best shape-specific stream the stored geometry
supports: obvious runs should be emitted as fills, and per-pixel work should be
limited to AA boundary segments and genuinely mixed corner spans.

### Helper Reuse Boundaries

The design reuses internals selectively.

Good reuse candidates:

- construction-time normalization,
- common outline/interior alpha blending logic,
- corner-local annulus evaluation helpers,
- and shared draw-subdivision scaffolding.

Poor reuse candidates:

- the equal-radius rectangle classifier,
- the equal-radius dedicated stream,
- and any helper that would inject extra branches into the current equal-
  radius fast path.

## Proposed API

Add overloads like the following:

```cpp
struct RoundRectRadii {
  float tl;
  float tr;
  float bl;
  float br;
};

SmoothShape SmoothRoundRect(float x0, float y0, float x1, float y1,
             const RoundRectRadii& radii, Color color,
                            Color interior_color = color::Transparent);

SmoothShape SmoothThickRoundRect(float x0, float y0, float x1, float y1,
               const RoundRectRadii& radii,
               float thickness,
                                 Color color,
                                 Color interior_color = color::Transparent);

SmoothShape SmoothFilledRoundRect(float x0, float y0, float x1, float y1,
                const RoundRectRadii& radii, Color color);
```

The existing single-radius overloads remain unchanged.

Example use:

```cpp
dc.draw(SmoothFilledRoundRect(20, 20, 120, 80,
                              RoundRectRadii{8, 8, 20, 20},
                              color::Blue));

dc.draw(SmoothThickRoundRect(20, 100, 120, 160,
                             RoundRectRadii{.tl = 4, .tr = 12,
                                            .bl = 20, .br = 6},
                             3, color::Black, color::White));
```

The designated-initializer form depends on toolchain support. The API itself
only requires that the struct be a simple aggregate with named fields.

## Implementation Plan

### Phase 1: Public API And Shared Radii Normalization

Work:

- add `RoundRectRadii` to `roo_display/shape/smooth.h`,
- add the three public overloads taking `const RoundRectRadii&`,
- factor the current equal-radius normalization into shared helpers,
- add four-radius normalization, including non-negative clamp, thickness
  expansion, and global-fit scaling,
- and keep the existing single-radius overloads intact.

Validation:

- `//:products_compile_test` for public API compile coverage,
- and a focused unit test that normalized equal radii still produce the same
  rendered result through old and new overloads.

### Phase 2: Equal-Radius Fold And Storage Guardrails

Work:

- implement post-normalization equal-radius detection,
- dispatch equal effective radii back into the existing round-rect builder,
- add the new unequal-radius `SmoothShape` kind and payload with outer boundary
  extents, four outer radii, thickness, per-corner adjusted-square caches,
  colors, and five helper boxes,
- and add compile-time size guards so the new payload cannot grow past the
  current union ceiling unnoticed.

Validation:

- a unit test that exercises unequal input collapsing to equal effective radii
  and confirms it behaves like the current implementation,
- and a compile-time size check guarding `sizeof(SmoothShape)` indirectly via
  the new payload limit and directly checking that the new payload fits within
  `SmoothShape::Arc`.

### Phase 3: Helper Boxes, Slabs, And Pixel Evaluation

Work:

- compute the corner centers, support functions, and maximal helper boxes,
- implement the unequal-radius per-pixel color evaluator,
- implement the fixed evaluator order: helper-box accept, outer-extents
  rejection, corner quarter-annulus evaluation, then straight-edge evaluation,
- and use the precomputed overlapping boxes to keep the common interior queries
  cheap.

Validation:

- `//:smooth_shapes_test` cases for filled, outlined, and thick asymmetric
  round rects,
- and focused render checks that cover empty or degenerate helper-box cases.

### Phase 4: Readback And Uniform-Rect Classification

Work:

- add unequal-radius helpers for `readColors()`, `readColorRect()`, and
  `readUniformColorRect()`,
- implement conservative rectangle classification using helper boxes,
  straight-edge checks, and corner-local squared-distance checks,
- optimize `readColorRect()` for composition by filling classified slabs and
  subdividing only mixed boundary regions into 8x8 tiles,
- and keep `readUniformColorRect()` conservative and allocation-free.

Validation:

- `//:read_uniform_color_rect_test` for guaranteed interior, guaranteed
  transparent, and boundary-adjacent regions,
- add a focused `//:read_color_rect_test` target for stack-sized rectangles
  that hit slabs, corner quadrants, and mixed AA tiles,
- and regression checks that mixed-color regions still correctly report
  non-uniform.

### Phase 5: Draw Path Integration

Work:

- add the new unequal-radius draw helper,
- wire `drawTo()` through the new shape kind,
- preserve the current equal-radius draw path unchanged,
- and use subdivision logic analogous to the current round-rect path, but with
  the new helper-box classification and corner evaluation.

Validation:

- `//:smooth_shapes_test` render snapshots or pixel-content expectations for
  representative asymmetric shapes,
- and `//:background_fill_optimizer_test` coverage for clipping near viewport
  edges.

### Phase 6: Dedicated Unequal-Radius Stream Override

Work:

- add a shape-specific `PixelStream` for the unequal-radius kind,
- keep the current equal-radius stream intact,
- classify each row into obvious transparent, interior, outline, and slow
  segments using the new helper boxes, straight-edge thresholds, and
  corner-local adjusted-square thresholds,
- and route `createStream()` to the new stream for the unequal-radius kind.

Validation:

- stream-backed rendering checks through existing rasterizable conversion test
  helpers,
- correctness checks against the non-stream draw/readback path for the same
  shapes,
- and a focused benchmark or profiling comparison that confirms the dedicated
  stream reduces per-pixel fallback versus generic `Rasterizable::createStream()`
  on representative filled and outlined asymmetric shapes.

### Phase 7: Documentation And Examples

Work:

- update `doc/programming_guide.md` with the new struct-based smooth round-rect
  API,
- update or add the mirrored programming-guide example sketch,
- and document any designated-initializer caveats for toolchain portability.

Validation:

- `//:products_compile_test` for compile coverage,
- and a quick review that the guide examples match the final signatures.

## Testing Plan

Validation should cover public API compilation, rendered output correctness,
stream correctness, `readColorRect()` behavior, uniform-rect classification,
stack composition behavior, and clipping behavior.

Primary targets:

- `//:smooth_shapes_test`
- `//:read_color_rect_test`
- `//:read_uniform_color_rect_test`
- `//:background_fill_optimizer_test`
- `//:products_compile_test`

When the programming guide or examples are updated, they are also built and
visually reviewed in the usual documentation workflow.

## Caveats

- The equal-radius fold must happen after normalization; otherwise some cases
  that become equal only after clamping would miss the fast path.
- Designated initializer syntax is toolchain-dependent. The portable form is
  positional aggregate initialization.
- This proposal covers smooth shapes only. It does not extend the basic
  non-smooth round-rect API or shadow helpers.
- Rejected alternative: storing four inner radii in the payload. The inner
  radii are always derivable as `max(0, outer_radius - thickness)`. Storing
  them saves at most one subtract and one clamp after a corner is selected,
  while adjusted-square caches avoid repeated radius multiplication and feed
  the geometry tests described in New Internal Shape Kind and Readback And
  Rectangle Classification.
- Rejected alternative: adding coarse global caches such as maximum outer or
  minimum inner adjusted squared radius. The selected payload already reaches
  the `Arc` ceiling, and the helper boxes plus per-corner adjusted-square
  caches provide the needed fast accepts without increasing `sizeof(SmoothShape)`.
- Rejected alternative: using only the generic rasterizable stream for the
  unequal-radius kind. The streaming API is important for backgrounds, tiling,
  and composition, so the design includes a dedicated stream from the first
  complete implementation.