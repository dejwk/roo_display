---
name: roo-display-code-authoring
description: 'roo_display-specific addendum for code authoring and validation. Use with the local embedded-cpp-code-authoring instruction when editing public APIs, adding tests, documenting behavior, running Bazel tests, or checking compile coverage.'
argument-hint: 'Describe the code change you are making and what you need to validate.'
user-invocable: true
---

# roo_display Code Authoring

Use this skill for `roo_display`-specific guidance on top of the local
[embedded-cpp-code-authoring instruction](../../instructions/embedded-cpp-code-authoring.instructions.md).

## roo_display-Specific Guidance

- When extending an existing implementation in a file, prefer the prevailing
  local style for branch ordering, early returns, helper factoring, naming,
  and comment density unless there is a clear reason to refactor the broader
  surrounding code.
- When a change affects user-visible behavior or recommended usage, update
  `doc/programming_guide.md` or other relevant user-facing docs.
- Use the repository validation commands below instead of generic build or test
  guesses.

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

- Validation uses the narrowest relevant Bazel target first.
- Compile-coverage checks use `products_compile_test` when relevant.
- User-facing docs are updated when behavior or recommended usage changes.
- Extensions stay consistent with the established local style in the same
  file unless the change intentionally refactors that surrounding code.