# Value-owned raw streams for glyph rendering

Status: deferred proposal, 2026-09-12. No renderer or stream API changes are
part of this note. Target stack measurements are required before committing an
implementation.

## Objective

Remove transient heap allocations from synchronous glyph rendering without
introducing an unmeasured increase in embedded stack usage or changing pixels.

## Motivation

Allocation instrumentation during the Material 3 text-field implementation in
`roo_windows` found allocations during warmed paint. The first traced allocation
was `Raster::createRawStream()`, reached through `StringViewLabel`,
`SmoothFontV2::drawBordered()`, and `DrawableRawStreamable::drawTo()`.
Editor state changes and repaint allocations must be measured separately;
removing temporary masked strings alone does not make glyph painting allocation
free. The optimization is deferred so the text-field work can ship independently.

## Background

A streamable describes an image; its raw stream is the mutable reader that
produces pixels. The reader holds decoding and traversal state, not a copy of
the image's pixel buffer.

- [Raster](../src/roo_display/core/raster.h) and
  [SimpleRawStreamable](../src/roo_display/internal/raw_streamable.h) allocate
  readers and return `std::unique_ptr`.
- `SubRectangleRawStream` owns its child through `unique_ptr` and adds a heap
  wrapper for clipping.
- [Overlay adapters](../src/roo_display/internal/raw_streamable_overlay.h)
  use owning pointers too: `SuperRectangleStream` owns one aligned child and
  `UnionStream` owns two children. A normal two-leaf overlay creates five
  allocations: two leaf readers, two alignment readers, and the union reader.
  Clipping adds further reader state and allocations.
- Both [SmoothFontV2](../src/roo_display/font/smooth_font_v2.cpp) and
  [SmoothFont](../src/roo_display/font/smooth_font.cpp) call
  `drawKernedGlyphsModeFill()` when adjacent glyph bounding spans overlap.
  The gap includes side bearings, kerning and tracking. A negative gap uses
  `Overlay(...)` for raw/raw, raw/RLE, RLE/raw, and RLE/RLE pairs. Not every
  kerned pair needs an overlay. Visible-mode glyphs are positioned separately.

The text-field tiled fill path exercises this overlay path. Changing only the
leaf reader factory would leave overlay and clipping allocations in place.

## Requirements

- Preserve pixel output, alpha blending, tracking, kerning, clipping and skip
  semantics for raw and RLE glyphs.
- Preserve existing callers' owning-pointer contracts.
- Do not retain pointers into moved objects or expired temporaries.
- Report actual target object sizes and before/after compiler stack usage.
  Do not infer call-chain stack cost from `sizeof` alone.
- Keep generic deeply nested overlays from acquiring an unbounded new stack
  requirement by default.

## Design Overview

Prototype an additive value-returning reader path. Its adapters own concrete
child readers directly. The complete reader tree lives for one synchronous draw;
only the final drawing call borrows its address.

```text
local union reader
  +-- aligned left reader
  |     +-- raw or RLE left reader
  +-- aligned right reader
        +-- raw or RLE right reader
```

Clipping adapters follow the same ownership rule. Moving an outer reader moves
its children; no pointer must be repaired afterward. Image data remains borrowed
under the existing streamable lifetime contract.

## Design Details

Provide a separate value factory and type trait alongside `createRawStream()`
and `RawStreamTypeOf`, whose current definitions assume a `unique_ptr` return.
The final API names and adapter sharing mechanism are prototype decisions, not
an approved replacement of the existing API.

Implement adapters with direct child storage or a storage policy shared with
legacy pointer adapters. Do not wrap stack addresses in `unique_ptr`; that would
attempt to delete stack objects. Do not return adapter trees that borrow local
child readers. Audit raw/RLE iterator moves for internal self-references too.

Opt the bounded glyph path into value readers only after its measurements pass
review. Leave generic overlay callers on the existing heap path. A later generic
migration requires its own nesting limits or explicit storage policy; it is not
part of the initial optimization.

## Implementation Plan

Follow the [embedded C++ authoring guidance](../.github/instructions/embedded-cpp-code-authoring.instructions.md).

1. **Measure the current path.** Add reproducible target size/stack probes and
   clipped/overlapping glyph fixtures. Proposed commit: `Characterize glyph
   stream allocation and target stack costs`. Validate all raw/RLE combinations
   and record compiler, flags, ABI and fixture parameters.
2. **Prototype value composition, uncommitted until measured.** Add leaf,
   alignment, union and clipping readers with value ownership. Proposed commit:
   `Add opt-in value-owned glyph streams`. Validate moves, skip/transparency,
   pixel equivalence, allocations, and the stack acceptance checks below before
   committing. Record the reviewed budget in the results, not a guessed limit.
3. **Enable the bounded glyph path.** Proposed commit: `Use measured value
   streams for synchronous glyph draws`. Verify normal and tiled text, secure
   mask/reveal, clipping and overlap regressions; preserve the legacy API.

## Testing Plan

Measure target `sizeof` for leaf readers, aligned readers, unions, and clipped
unions across raw/raw, raw/RLE, RLE/raw and RLE/RLE. Use the ESP32-C3 release
compiler and production flags, with `-fstack-usage` and disassembly to inspect
inlining, return slots, temporaries and writer buffers. Compare before/after
stack along the complete text-to-writer call chain; per-function frame sizes
alone do not establish peak usage.

Record allocation counts after warm-up separately from activation/cache growth.
Require zero reader allocations in the opted-in glyph path and exact pixel
comparison, including negative tracking, overlapping pairs, empty clips and
clips cutting through either glyph. Run existing renderer tests and sanitizers.

If hardware is available, also record runtime task stack high-water under the
same fixtures. Clearly label compiler-derived estimates versus runtime
measurements. There are no value-reader target measurements yet.

## Caveats

Direct child ownership relocates decoding state from heap to stack; it does not
eliminate that memory. Nested overlays multiply reader state. Return-value
optimization is not evidence of a small peak frame, and host ABI sizes are not
substitutes for embedded measurements. Until the prototype passes those checks,
keep the current renderer and document its paint allocations as a known cost.
