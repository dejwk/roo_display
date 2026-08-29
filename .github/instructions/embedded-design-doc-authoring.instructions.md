---
name: "Embedded Design Doc Authoring"
description: "Use when writing or updating design docs, implementation plans, rendering docs, or API proposals in this repository. Shared baseline across roo libraries."
applyTo:
  - "docs/**/*.md"
  - "doc/**/*.md"
---
# Embedded Design Doc Authoring

Use this instruction for shared design-doc expectations across roo
repositories. Repo-local skills should add project-specific references,
validation, and constraints on top of this baseline.

## Audience and Purpose

A design document serves three audiences at once:

- agents and humans who will implement the design;
- human reviewers who must evaluate its correctness and tradeoffs; and
- future maintainers who use it as durable project documentation.

Write so a human reader can understand the problem, vocabulary, decisions, and
rationale without reconstructing them from the proposed API or implementation
plan. Implementation precision does not replace explanatory clarity.

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
11. Future Work (optional)

## Writing Rules

- Be succinct.
- Keep Objective succinct and limited to the intended outcome. Do not put term
  definitions there; it is acceptable for full understanding of the Objective
  to require the Background that follows.
- Define pre-existing concepts needed by the proposal in Background. When a
  concept already has a project-wide definition, link the project glossary
  instead of redefining it.
- Define concepts introduced by the proposal in Design Overview, before the
  later design sections rely on them. State what each document-local term
  represents, what state or lifetime it has, and how it differs from nearby
  concepts. Do not rely on an API name or metaphor such as "armed",
  "connected", "host", or "owner" to explain itself.
- State Requirements in problem-domain language. Avoid making them depend on
  names or mechanics introduced only by the proposed solution.
- References and cross-references should generally be Markdown hyperlinks.
  Prefer linked file paths, doc titles, issues, PRs, and APIs over bare text
  mentions when a stable target exists.
- Do not repeat the same content across sections.
- Keep Motivation brief; it explains why, not the full requirement set.
- Use Background only for current-state context needed to understand the
  proposal.
- Put detailed enumeration in Requirements, not in Motivation.
- Put major decisions in Design Overview and leave mechanics for Design
  Details.
- In Design Overview, show how each major part of the proposed solution satisfies
  the Requirements. When the connection is not obvious, add an explicit mapping
  paragraph, table, or annotated diagram. A reviewer should not have to infer why
  a class, list, cache, callback, state field, or ownership link is necessary.
- Explain purpose before mechanics. Before describing how a data structure is
  linked, traversed, or updated, state which behavior or lifetime requirement
  needs it and why a simpler one-way relationship is insufficient.
- Split Implementation Plan into small incremental subsections or phases that
  each map to a single commit.
- Start Implementation Plan with a short authoring-reference line that links
  the corresponding repo-local code-authoring guidance.
- Each implementation step should describe the intended code change slice,
  include a proposed commit message, and state the narrow validation that
  makes that slice complete.
- Keep each implementation step reasonably sized so it can be implemented and
  tested before moving on.
- When a phase adds new functionality, include the incremental test coverage
  and example or documentation updates for that functionality in the same
  phase rather than deferring them to later cleanup.
- Keep Testing Plan as a summary of validation scope, targets, and coverage.
- Do not repeat detailed per-step test cases in Testing Plan when they are
  already described under Implementation Plan.
- Put rejected alternatives in Caveats, not scattered through the main design.
  When there are substantial rejected alternatives, use a dedicated
  `### Rejected Alternatives` subsection under Caveats, with one `####`
  subsection for each rejected alternative.
- Add an optional Future Work section after Caveats for potential improvements
  that are intentionally left out of scope. Do not use Future Work to defer a
  decision required by the current proposal.
- LaTeX math is acceptable for formulas when it makes geometric, rendering,
  or cost analysis clearer. Use inline math for short expressions and display
  math for longer derivations.
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
- When Proposed API introduces entry points that will land before full support
  is implemented, specify the interim behavior explicitly: if the API can
  return an error, prefer returning an error; otherwise emit
  `LOG(WARNING) << "Unimplemented: <details>"` and fall back to a degenerate
  behavior when possible; if no safe fallback exists, emit
  `LOG(FATAL) << "Unimplemented: <details>"`.

## Closing On Decisions

A design doc exists to close on decisions, not to enumerate them. Every open
question in the doc should either be resolved, or be moved into the
Implementation Plan as an explicit experiment with success criteria.

Rules:

- Do not use hedged phrasing such as "the design should decide whether to
  include X", "may include Y if profiling shows a win", or "define how the
  evaluator chooses between A and B". Decide, and state the chosen option
  with the reasoning.
- When a decision depends on quantitative tradeoffs (RAM versus cycles,
  branch cost versus cache footprint, etc.), include the analysis: ballpark
  per-pixel or per-row cost estimates, payload-size deltas, and the reasoning
  that selects the chosen option over the rejected ones.
- If the analysis genuinely cannot resolve the choice on paper, add a
  numbered phase to the Implementation Plan that runs a targeted micro-
  benchmark or measurement, specifies the input shapes, the metric, and the
  threshold that selects between the candidates. The doc is not done until
  that phase has a defined exit criterion.
- Record rejected alternatives in Caveats with the reason they were rejected
  and a pointer back to the section that made the call. If the alternatives
  are substantial, put them in `### Rejected Alternatives` and give each one a
  `####` subsection.
- Use Future Work only for intentionally out-of-scope improvements. If an item
  is necessary for the design to be correct, make it part of the chosen design
  or an Implementation Plan phase with validation.
- Re-read the finished doc and remove every "if", "may", "could", "should
  consider", or "depending on" that hides an unresolved choice; replace each
  with a decision or a planned experiment.

## Checklist

- Section order matches the required structure.
- References are generally hyperlinks when a stable link target exists.
- The intended implementation, review, and long-term documentation audiences can
  understand the document without reverse-engineering terminology from code.
- Objective states the intended outcome succinctly and contains no term
  definitions.
- Background defines the pre-existing concepts needed to understand the design,
  linking project-wide glossary definitions where available.
- Design Overview defines concepts introduced by the proposal before later
  design sections rely on them.
- Requirements use problem-domain language rather than unexplained names or
  mechanics from the proposed solution.
- No repeated requirements across Objective, Motivation, and Requirements.
- Design Overview explicitly connects major solution elements to requirements,
  with a mapping table, paragraph, or diagram where the relationship is not
  obvious.
- Every nontrivial data structure or reverse link is motivated before its
  mechanics are described.
- Implementation Plan is split into incremental, testable steps.
- Implementation Plan includes a hyperlink to the corresponding code-authoring
  guidance.
- Each implementation step maps to a single commit and includes a proposed
  commit message.
- Each implementation step states both the work and the intended validation.
- Phases that add new functionality include incremental test coverage and
  example or documentation updates.
- Every design decision in the doc is closed: chosen option, rejected
  alternatives, and the analysis or planned experiment that selects between
  them are all present.
- Substantial rejected alternatives are grouped under Caveats in a dedicated
  `### Rejected Alternatives` subsection, with one `####` subsection per
  alternative.
- Future Work, when present, appears after Caveats and contains only
  intentionally out-of-scope improvements.
- LaTeX formulas are used when they clarify the design.
- Geometry-driven illustrations, when present, prefer hand-authored SVG over
  vectorized graphics and use a white background, sans-font labels, exact or
  algorithm-derived coordinates, and no clipped content.
- Geometry- or rendering-related discussions include illustrations unless the
  point is genuinely obvious without one.
- Pixel-output illustrations, when present, prefer PNG when exact raster color
  or antialiasing behavior is the point of the figure.
- Partially implemented new APIs prefer returning an error when the API
  supports it; otherwise they define temporary `LOG(WARNING)` plus degenerate
  fallback behavior, or `LOG(FATAL)` when no safe fallback exists.
- No hedged language ("may", "could", "should consider", "depending on")
  hides an unresolved choice.
- Testing Plan summarizes validation coverage without repeating per-step test
  case detail from Implementation Plan.
