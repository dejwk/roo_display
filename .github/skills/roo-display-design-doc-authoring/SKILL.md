---
name: roo-display-design-doc-authoring
description: 'roo_display-specific addendum for design docs under docs/ or related user-facing documentation under doc/. Use with the local embedded-design-doc-authoring instruction when documenting a rendering change, performance work, driver update, or API proposal.'
argument-hint: 'Describe the design topic and whether the doc is new or an update.'
user-invocable: true
---

# roo_display Design Doc Authoring

Use this skill when writing or revising design docs under `roo_display/docs`.
Apply the local
[embedded-design-doc-authoring instruction](../../instructions/embedded-design-doc-authoring.instructions.md)
first. Use the same standards when a design change also requires updates to the
user-facing docs under `roo_display/doc`.

Primary references:
[docs/README.md](../../../docs/README.md)
[docs/clip_exclude_rects_scanline_skip_design.md](../../../docs/clip_exclude_rects_scanline_skip_design.md)
[doc/programming_guide.md](../../../doc/programming_guide.md)

## roo_display-Specific Rules

- Discuss RAM and buffer-footprint impact whenever the proposal touches
  offscreens, caches, images, fonts, filters, or other stored render state.
- Call out rendering-cost consequences for drawing-path changes, especially
  when the proposal affects clipping, blending, rasterization, transformations,
  or address-window writes.
- If the change affects device drivers, transports, touch handling, or product
  wrappers, say what compile or test coverage should prove that integration.
- Keep API proposals close to existing naming, ownership, and layering unless
  there is a strong reason to move them.
- When the design changes user-visible behavior or recommended usage, note what
  should also be updated in `doc/programming_guide.md` or other user-facing
  docs.

## Checklist

- Primary references reflect the relevant design and user-facing docs.
- RAM and rendering-cost impact are discussed when relevant.
- Driver, product, or integration coverage is called out when relevant.
- User-facing documentation follow-up is identified when behavior changes.
- API naming, ownership, and layering stay close to existing patterns unless a
  deviation is justified.