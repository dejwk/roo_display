name: "Embedded C++ Code Authoring"
description: "Use when editing embedded C++ library code, public APIs, tests, or validation targets in this repository. Shared baseline across roo libraries."
applyTo:
  - "**/*.c"
  - "**/*.cc"
  - "**/*.cpp"
  - "**/*.h"
  - "**/*.hh"
  - "**/*.hpp"
  - "**/*.ino"
  - "**/*.bzl"
  - "BUILD"
  - "MODULE.bazel"
---
# Embedded C++ Code Authoring

Use this instruction for shared code-authoring expectations across roo
repositories. Repo-local skills should add repository-specific validation and
policy on top of this baseline.

## Core Conventions

- Follow Google-style C++, except instance methods use `camelCase()`.
  Trivial accessors and mutators (one-line field getters/setters and
  STL-mimicking container methods) may keep `snake_case()` when that reads
  more naturally, matching the spelling of the underlying field.
- Favor readability. Avoid redundant branches, repeated explanations, and
  unnecessary line count when the code can stay clear without them.
- Avoid long lambdas. When logic is substantial, prefer an unnamed-namespace
  helper over a large local lambda, and define that helper close to the place
  where it is used.
- Avoid `auto` unless the type is obvious from the initializer context, such
  as `std::make_unique<...>()`, or the spelled-out type would be excessively
  complex.
- Be conservative about RAM. Flash is usually cheaper than per-instance state,
  so prefer shared data, existing ownership points, and zero-cost hooks when
  possible.
- All public classes and public methods should have Doxygen comments at the
  declaration site.
- Doxygen comments should summarize implemented behavior, or intended behavior
  for pure-virtual and otherwise contract-defining declarations.
- Every code change must ship with focused unit tests.
- Non-trivial test cases should carry brief `Verifies ...` comments stating the
  contract or regression being checked. The comment should apply to the whole
  test case and appear immediately before the test declaration.
- Code comments should be sparse, but complex algorithms should include brief
  comments that explain the main concepts, major decisions, and why key
  branches exist, not just the mechanics line by line.
- Non-trivial helper functions and methods should carry a short comment or
  Doxygen summary stating what they compute, classify, or guarantee.

## Design-Stage Commits

- When the implemented change maps to a single stage or phase from a design
  doc, include a proposed commit message in the completion note even if no
  commit is created.
- Use a two-part structure: one summary sentence followed by one descriptive
  paragraph.
- The summary sentence must be clear without additional context. Start it with
  the design doc and stage or phase, then state what landed in that stage.
- The descriptive paragraph should explain the concrete slice that landed in
  that stage, not the whole feature. Name the API, helper, widget behavior,
  tests, docs, or validation added by the change.
- Reference the relevant design doc path or title so the message preserves the
  stage context.
- When the design doc includes a `Proposed commit message` hint for that
  stage, treat it as the starting point and keep its intent unless the
  implemented slice differs. If it differs, adjust the message to match the
  actual code.

## Validation

- Prefer the narrowest relevant test, build, or typecheck target first, then
  widen only if needed.
- Use repo-local code-authoring skills or guidance to find repository-specific
  validation commands, compile-coverage checks, and integration builds.

## Checklist

- Public API declarations have Doxygen comments.
- The code change includes focused unit tests.
- Non-trivial tests have short `Verifies ...` comments immediately before the
  test declaration, covering the whole test case.
- Validation uses the narrowest relevant target first.
- Complex implementation comments explain intent, not mechanics.
- Complex algorithms explain their main strategy and important branches.
- Non-trivial helper functions and methods are documented.
- The change does not add avoidable per-instance RAM cost.
- If the change implements a design-doc stage, the response includes a
  proposed commit message with a standalone summary sentence followed by a
  descriptive paragraph, references the design doc, and reflects any
  stage-specific commit-message hint.
