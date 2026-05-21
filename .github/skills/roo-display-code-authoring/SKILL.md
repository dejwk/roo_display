---
name: roo-display-code-authoring
description: 'Reference for authoring and validating roo_display code changes. Use when editing public APIs, adding tests, documenting behavior, running Bazel tests, or checking compile coverage.'
argument-hint: 'Describe the code change you are making and what you need to validate.'
user-invocable: true
---

# roo_display Code Authoring

Use this skill for general `roo_display` code changes, including refactors,
API work, tests, documentation updates, and validation.

## Core Conventions

- Follow Google-style C++, except instance methods use `camelCase()`.
  Trivial accessors and mutators (one-line field getters/setters and
  STL-mimicking container methods) may keep `snake_case()` when that reads
  more naturally, matching the spelling of the underlying field.
- Favor readability. Avoid redundant branches, repeated explanations, and
  unnecessary line count when the code can stay clear without them.
- When extending an existing implementation in a file, prefer the prevailing
  local style for branch ordering, early returns, helper factoring, naming,
  and comment density unless there is a clear reason to refactor the broader
  surrounding code.
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
- When a change affects user-visible behavior or recommended usage, update
  `doc/programming_guide.md` or other relevant user-facing docs.
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
  that stage, not the whole feature. Name the API, helper, tests, docs, or
  validation added by the change.
- Reference the relevant design doc path or title so the message preserves the
  stage context.
- When the design doc includes a `Proposed commit message` hint for that
  stage, treat it as the starting point and keep its intent unless the
  implemented slice differs. If it differs, adjust the message to match the
  actual code.

## Validation Commands

- `roo_display` is often used from a parent workspace under `lib/roo_display`.
  Run tests from there, for example:
  `(cd lib/roo_display; bazel test //:color_test)`
- Prefer the narrowest relevant Bazel target first, then widen only if needed.
- To verify broader compile coverage for product headers and integrations, run:
  `(cd lib/roo_display; bazel test //:products_compile_test)`
- Full-repo validation matching CI is:
  `(cd lib/roo_display; bazel build //...)`
  `(cd lib/roo_display; bazel test //... --test_output=errors)`

## Checklist

- Public API declarations have Doxygen comments.
- The code change includes focused unit tests.
- Non-trivial tests have short `Verifies ...` comments immediately before the
  test declaration, covering the whole test case.
- Validation uses the narrowest relevant Bazel target first.
- Compile-coverage checks use `products_compile_test` when relevant.
- User-facing docs are updated when behavior or recommended usage changes.
- Complex implementation comments explain intent, not mechanics.
- Complex algorithms explain their main strategy and important branches.
- Non-trivial helper functions and methods are documented.
- Extensions stay consistent with the established local style in the same
  file unless the change intentionally refactors that surrounding code.
- The change does not add avoidable per-instance RAM cost.
- If the change implements a design-doc stage, the response includes a
  proposed commit message with a standalone summary sentence followed by a
  descriptive paragraph, references the design doc, and reflects any
  stage-specific commit-message hint.