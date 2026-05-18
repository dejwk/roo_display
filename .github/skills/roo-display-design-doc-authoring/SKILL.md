---
name: roo-display-design-doc-authoring
description: 'Reference for writing or updating roo_display design docs under docs/ or related user-facing documentation under doc/. Use when documenting a rendering change, performance work, driver update, or API proposal.'
argument-hint: 'Describe the design topic and whether the doc is new or an update.'
user-invocable: true
---

# roo_display Design Doc Authoring

Use this skill when writing or revising design docs under `roo_display/docs`.
Use the same standards when a design change also requires updates to the
user-facing docs under `roo_display/doc`.

Primary references:
[docs/README.md](../../../docs/README.md)
[docs/clip_exclude_rects_scanline_skip_design.md](../../../docs/clip_exclude_rects_scanline_skip_design.md)
[doc/programming_guide.md](../../../doc/programming_guide.md)

## Required Structure

Use this section order unless a narrower document genuinely needs less:

1. Objective
2. Motivation
3. Background
4. Requirements
5. Design Overview
6. Design Details
7. Proposed API
8. Implementation Plan
9. Testing Plan
10. Caveats

## Writing Rules

- Be succinct.
- Do not repeat the same content across sections.
- Keep Motivation brief; it explains why, not the full requirement set.
- Use Background only for current-state context needed to understand the
  proposal.
- Put detailed enumeration in Requirements, not in Motivation.
- Put major decisions in Design Overview and leave mechanics for Design
  Details.
- Split Implementation Plan into small incremental subsections or phases.
- Each implementation step should describe the intended code change slice and
  the narrow validation that makes that slice complete.
- Keep each implementation step reasonably sized so it can be implemented and
  tested before moving on.
- Keep Testing Plan as a summary of validation scope, targets, and coverage.
- Do not repeat detailed per-step test cases in Testing Plan when they are
  already described under Implementation Plan.
- Put rejected alternatives in Caveats, not scattered through the main design.

## Closing On Decisions

A design doc exists to close on decisions, not to enumerate them. Every
open question in the doc should either be resolved, or be moved into the
Implementation Plan as an explicit experiment with success criteria.

Rules:

- Do not use hedged phrasing such as "the design should decide whether to
  include X", "may include Y if profiling shows a win", or "define how the
  evaluator chooses between A and B". Decide, and state the chosen option
  with the reasoning.
- When a decision depends on quantitative tradeoffs (RAM versus cycles,
  branch cost versus cache footprint, etc.), include the analysis: ballpark
  per-pixel or per-row cost estimates, payload-size deltas, and the
  reasoning that selects the chosen option over the rejected ones.
- If the analysis genuinely cannot resolve the choice on paper, add a
  numbered phase to the Implementation Plan that runs a targeted micro-
  benchmark or measurement, specifies the input shapes, the metric, and the
  threshold that selects between the candidates. The doc is not done until
  that phase has a defined exit criterion.
- Record rejected alternatives in Caveats with a one-line reason and a
  pointer back to the section that made the call.
- Re-read the finished doc and remove every "if", "may", "could", "should
  consider", or "depending on" that hides an unresolved choice; replace each
  with a decision or a planned experiment.

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

- Section order matches the required structure.
- No repeated requirements across Objective, Motivation, and Requirements.
- Implementation Plan is split into incremental, testable steps.
- Each implementation step states both the work and the intended validation.
- RAM and rendering-cost impact are discussed when relevant.
- Driver, product, or integration coverage is called out when relevant.
- User-facing documentation follow-up is identified when behavior changes.
- Every design decision in the doc is closed: chosen option, rejected
  alternatives, and the analysis or planned experiment that selects between
  them are all present.
- No hedged language ("may", "could", "should consider", "depending on")
  hides an unresolved choice.
- Testing Plan summarizes validation coverage without repeating per-step test
  case detail from Implementation Plan.