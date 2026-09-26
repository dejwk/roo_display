---
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
repositories. The authoritative source is
`roo-registry/template/push/.github/instructions/general-cpp-code-authoring-instructions.md`.
Make shared changes there and sync this file to consuming repositories.
Repo-local guidance can add repository-specific validation and policy on top
of this baseline.

## Core Conventions

- Follow Google C++ Style as the baseline. Format changed C++ files with the
  repository's Google-based `.clang-format` configuration. Namespace-level
  functions (including unnamed-namespace helpers) and static methods use
  `CapitalizedNames()`, while instance methods use `camelCase()`. Trivial
  accessors and mutators (one-line field getters/setters and STL-mimicking
  container methods) may keep `snake_case()` when that reads more naturally,
  matching the spelling of the underlying field. Language- and
  framework-mandated names such as allocation operators and Arduino `setup()`
  and `loop()` retain their required spelling.
- Keep algorithms short by simplifying the logic, removing redundant work and
  branches, and using cohesive helpers when they clarify the computation.
  Keep their contents readable: do not reduce line count by removing useful
  comments, blank lines, or documentation, or by packing declarations and
  control flow together. Brevity should come from simpler logic.
- Prefer cohesive semantic groupings, descriptive names, explicit ownership,
  visible dependency boundaries, and enough whitespace to make related
  concepts easy to scan. Avoid unnecessary abstraction, cleverness, and
  indirection.
- Preserve existing documentation and explanatory comments during refactors
  unless they are obsolete. Relocate them with the declarations or behavior
  they describe.
- When splitting declarations from implementations, use matching basenames:
  `<filename>.h` and `<filename>.cpp`. Put out-of-line implementations in the
  corresponding `.cpp`, not in unrelated implementation files.
- Use braces for control-flow bodies unless the entire construct, including
  its condition and body, fits on one line within the style's column limit
  (for example, `if (ptr == nullptr) return;`). Multiline constructs require
  braces. Do not force `InsertBraces: true`, since single-line unbraced
  constructs are permitted.
- Declare one variable or data member per declaration statement. Do not combine
  same-typed names with commas.
- Keep `CHECK` and related assertion macros at their point of use so failures
  report the source line that expresses the violated contract.
- Embedded-target code must build with exceptions disabled (`-fno-exceptions`);
  do not use `throw`, `try`, `catch`, or exception-dependent behavior.
- Avoid `const_cast` as a way to bridge const/non-const API mismatches,
  especially for borrowed inputs where callers may rely on immutability.
  Only use it when the target is provably non-mutating for that call path;
  otherwise, fix the interface to be const-correct.
- Avoid RTTI-dependent constructs such as `dynamic_cast` and `typeid` in
  embedded-target library code. Many embedded builds disable RTTI; prefer
  compile-time type constraints, typed ownership APIs, virtual hooks, or
  explicit lightweight tags when a runtime distinction is truly needed.
- Avoid long lambdas. When logic is substantial, prefer an unnamed-namespace
  helper over a large local lambda, and define that helper close to the place
  where it is used.
- Avoid `auto` unless the type is obvious from the initializer context, such
  as `std::make_unique<...>()`, or the spelled-out type would be excessively
  complex.
- Do not rely on implicit or contextual conversions to `bool`. Compare pointers
  and smart pointers explicitly with `nullptr`, numeric values and counts with
  zero, bitmasks with zero, and enums with named enumerators. This applies to
  conditions, logical operators, and conditional (`?:`) expressions. Boolean
  values and Boolean predicates may be used directly; do not add redundant
  `== true` or `== false` comparisons.
- Be conservative about RAM. Flash is usually cheaper than per-instance state,
  so prefer shared data, existing ownership points, and zero-cost hooks when
  possible.
- Use `///` for Doxygen comments; do not use block-form Doxygen comments.
- All public classes and public methods should have Doxygen comments at the
  declaration site.
- Always leave an empty line between declarations or implementations. The
  only exception is a group of declarations or definitions that each fit
  entirely on a single line, including any comments.
- Always leave one empty separator line between adjacent `struct` or `class`
  declarations.
- Start each Doxygen comment with purpose, then behavior: why the API exists
  and what it does. A single concise summary may cover both. Describe
  implemented behavior; for pure-virtual and otherwise contract-defining
  declarations, describe the behavior implementations must provide. Put
  ownership, lifetime, threading, allocation, and other auxiliary properties
  afterward. Constructor summaries should say what they create and how they
  use the supplied parameters, referencing those parameters with `@p`.
- Make public API comments useful to a human caller: explain meaningful
  parameters, outcomes, asynchronous completion, ownership/lifetime, and
  relevant failure behavior and scheduling context. Do not trade clarity for
  a terse summary; keep detail proportional to the complexity of the contract.
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
- Before handing code over for review or submitting it, run `clang-format` on
  every changed C++ source and header file.
- Before finishing a refactor, compare the old and new public API documentation
  and verify that useful context was not lost.

## Checklist

- Public API declarations have `///` Doxygen comments.
- Namespace-level functions and static methods use `CapitalizedNames()` unless
  their spelling is fixed by the language or framework.
- Declarations and implementations have empty separator lines, except groups
  whose individual declarations or definitions each fit entirely on one line,
  including any comments.
- Adjacent `struct` and `class` declarations have empty separator lines.
- Algorithms stay short through simpler logic, while useful comments,
  whitespace, and documentation preserve readability.
- Split declarations and implementations use matching `.h`/`.cpp` basenames.
- Multiline control-flow constructs use braces.
- Each variable and data member has its own declaration statement.
- Doxygen explains purpose, behavior, and meaningful parameters.
- The code change includes focused unit tests.
- Non-trivial tests have short `Verifies ...` comments immediately before the
  test declaration, covering the whole test case.
- Validation uses the narrowest relevant target first.
- `clang-format` has been run on every changed C++ source and header file
  before review or submission.
- Refactors preserve useful public API documentation and explanatory context.
- Complex implementation comments explain intent, not mechanics.
- Complex algorithms explain their main strategy and important branches.
- Non-trivial helper functions and methods are documented.
- The change does not add avoidable per-instance RAM cost.
- Boolean expressions use explicit comparisons for non-Boolean values.
- If the change implements a design-doc stage, the response includes a
  proposed commit message with a standalone summary sentence followed by a
  descriptive paragraph, references the design doc, and reflects any
  stage-specific commit-message hint.
