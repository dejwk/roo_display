---
name: "roo_display Rendering Design Authoring"
description: "Additional illustration requirements for geometry and rendering design documents in roo_display."
applyTo:
  - "docs/**/*.md"
  - "doc/**/*.md"
---
# Rendering Design Additions

Apply the [general design instructions](general-design-authoring-instructions.md)
first. The following requirements supplement that baseline for rendering designs.

- Use inline math for short expressions and display math for longer derivations.
- When Design Overview or Design Details discuss geometry, layout, clipping,
  paint order, rasterization, or other rendering-related issues, include an
  illustration unless the point is genuinely obvious without one. Prefer
  hand-authored SVG over vectorized graphics when the important thing to
  communicate is geometry, layout, render ordering, clip regions, or formula-
  derived coordinates. Use a white background and a sans font. Make geometry
  precise: derive coordinates from the formulas in the design, or simulate the
  algorithm used by the design doc to calculate them. Write a small helper
  script when that prevents hand-calculation drift. Check the SVG viewBox and
  bounds so labels, strokes, and content are not clipped.
- When the important thing to communicate is raster output or pixel color,
  prefer PNG. Use PNG for expected raster output of drawing algorithms,
  antialiasing behavior, per-pixel coverage, or any case where exact pixel
  colors matter more than geometric construction, and include one when that
  output is part of the design argument.
