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

The unequal-radius path should share low-level geometry utilities where doing
so does not introduce extra branching into the current equal-radius fast path.
At a minimum, construction-time normalization and some corner-distance math can
be shared. The top-level streaming, readback, and drawing helpers should remain
separate.

The unequal-radius variant should prioritize compact stored state over cached
micro-optimizations, but it does not need to use the absolute minimum storage.
There is likely modest headroom under the current `SmoothShape` union budget,
so a small amount of coarse cached state is acceptable if profiling suggests a
real win.

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
implementation should treat the fields by name rather than by any geometric
ordering assumption.

The public overloads should take the radii by `const RoundRectRadii&`.
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

The fold should happen after normalization rather than on raw input so cases
that start unequal but clamp to the same effective radii still use the
existing implementation.

### New Internal Shape Kind

Add a new `SmoothShape` kind for unequal corner radii, for example
`ROUND_RECT_CORNERS`, together with a separate payload struct.

The new payload should store:

- outer centerline extents,
- 4 outer radii,
- 4 inner radii,
- outline and interior colors,
- and 5 precomputed maximal interior helper boxes.

One practical layout is:

- 4 floats for extents: 16 bytes,
- 4 floats for outer radii: 16 bytes,
- 4 floats for inner radii: 16 bytes,
- 2 colors: 8 bytes,
- 5 helper boxes (`inner_core`, `inner_top`, `inner_bottom`, `inner_left`,
  `inner_right`): 40 bytes,
- optional coarse caches and flags: about 4 to 12 bytes.

Estimated total: about 100 to 108 bytes.

That is about 36 bytes larger than the current equal-radius `RoundRect`
payload, but it still fits under the current `Arc`-driven union ceiling.

### Footprint Delta Discussion

There are two relevant footprint questions.

First, the payload itself grows for the unequal-radius variant:

- current equal-radius round-rect payload: about 64 bytes,
- proposed unequal-radius payload: about 100 to 108 bytes,
- payload delta: about 36 to 44 bytes.

Second, the actual object footprint of `SmoothShape` should remain unchanged:

- current `SmoothShape`: about 128 bytes,
- proposed `SmoothShape`: still about 128 bytes,
- object-size delta: 0 bytes.

This works only if the new payload stays below the existing `Arc` member size.
If the implementation adds too many cached values, such as full per-corner
squared-radius thresholds together with stream-specific boundary trackers, it
can cross the current budget and force `sizeof(SmoothShape)` upward.

For that reason, the design should spend storage selectively rather than avoid
it categorically.

Reasonable candidates that may fit inside the current headroom are coarse
global caches such as:

- maximum outer adjusted squared radius,
- minimum non-zero inner adjusted squared radius,
- or a small number of precomputed slab boundaries.

These would support earlier transparent or interior classification in shared
helpers. They should only be included if measurement or profiling suggests that
they materially reduce work in rectangle classification, draw subdivision, or
stream emission.

The implementation should include a compile-time guard such as:

```cpp
static_assert(sizeof(SmoothShape::RoundRectCorners) <=
              sizeof(SmoothShape::Arc));
```

### Interior Slabs And Helper Boxes

Precompute 5 axis-aligned boxes that are guaranteed to lie entirely inside the
inner shape:

- `top_slab`,
- `bottom_slab`,
- `left_slab`,
- `right_slab`,
- `inner_core`.

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

### Rendering Strategy

The unequal-radius implementation should use a separate helper family from the
current equal-radius round-rect helpers.

Recommended structure:

- fast rejection and acceptance based on helper boxes and corner-local bounds,
- a per-pixel evaluator that selects the relevant corner or straight edge,
- conservative rectangle classification for `readUniformColorRect()` and draw
  subdivision,
- and a dedicated draw helper for the new kind.

Geometrically, the shape is treated as:

- a guaranteed interior core,
- four maximal overlapping helper slabs,
- four straight edge regions between adjacent corner centers,
- and four quarter-annulus corners with independent radii.

Only the AA transition zones need expensive per-pixel evaluation.

### Streaming Strategy

The equal-radius `RoundRectStream` should remain unchanged and exclusive to the
existing equal-radius kind.

For the new unequal-radius kind, the target design includes a dedicated stream
override rather than relying permanently on the generic
`Rasterizable::createStream()` fallback.

Reasons:

- it keeps the common equal-radius stream path untouched,
- it avoids regressing a common background and tiling use case,
- and it keeps stream behavior aligned with the specialized equal-radius path.

The unequal-radius stream does not need to match the current equal-radius
streaming optimization exactly. It should, however, provide a shape-specific
row-major emission path that can classify obvious interior, outline, and
transparent runs without per-pixel virtual fallback for the whole surface.

The initial version can be simpler than the equal-radius stream and may accept
more slow segments, but stream specialization is still part of the intended
implementation.

### Helper Reuse Boundaries

The design should reuse internals selectively.

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
- add the new unequal-radius `SmoothShape` kind and payload skeleton,
- decide whether to include coarse caches such as max outer and min inner
  adjusted squared radii,
- and add compile-time size guards so the new payload cannot grow past the
  current union ceiling unnoticed.

Validation:

- a unit test that exercises unequal input collapsing to equal effective radii
  and confirms it behaves like the current implementation,
- and a compile-time size check guarding `sizeof(SmoothShape)` indirectly via
  the new payload limit.

### Phase 3: Helper Boxes, Slabs, And Pixel Evaluation

Work:

- compute the corner centers, support functions, and maximal helper boxes,
- implement the unequal-radius per-pixel color evaluator,
- define how the evaluator chooses between helper-box accept, straight-edge
  evaluation, and corner quarter-annulus evaluation,
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
- implement conservative rectangle classification using helper boxes plus
  corner-local checks,
- and use any approved coarse caches only where they materially simplify or
  speed these classifiers.

Validation:

- `//:read_uniform_color_rect_test` for guaranteed interior, guaranteed
  transparent, and boundary-adjacent regions,
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
  segments using the new helper boxes and coarse thresholds,
- and route `createStream()` to the new stream for the unequal-radius kind.

Validation:

- stream-backed rendering checks through existing rasterizable conversion test
  helpers,
- correctness checks against the non-stream draw/readback path for the same
  shapes,
- and a focused benchmark or profiling comparison, if practical, to confirm the
  override is at least directionally better than generic rasterizable fallback.

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
stream correctness, uniform-rect classification, and clipping behavior.

Primary targets:

- `//:smooth_shapes_test`
- `//:read_uniform_color_rect_test`
- `//:background_fill_optimizer_test`
- `//:products_compile_test`

If the programming guide or examples are updated, they should also be built and
visually reviewed in the usual documentation workflow.

## Caveats

- The unequal-radius path is still expected to be slower than the tuned equal-
  radius path, even with a dedicated stream override, because the geometry is
  inherently less regular.
- The equal-radius fold must happen after normalization; otherwise some cases
  that become equal only after clamping would miss the fast path.
- Designated initializer syntax is toolchain-dependent. The portable form is
  positional aggregate initialization.
- This proposal covers smooth shapes only. It does not extend the basic
  non-smooth round-rect API or shadow helpers.
- Any extra cached geometry should be justified by profiling, because the
  remaining union headroom is real but limited.