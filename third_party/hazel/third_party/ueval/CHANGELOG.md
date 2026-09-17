# Changelog

All notable changes to `ueval` are documented in this file.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
this project uses [Semantic Versioning](https://semver.org) — see the
"Versioning" section in `README.md` for what MAJOR/MINOR/PATCH specifically
mean for this library (a single-header C expression evaluator, where "API"
covers both function signatures *and* expression-language behavior).

## [Unreleased]

### Added (calc.c)

- `calc` now prints a one-line startup banner reminding the user how to use it (expression evaluation, `\name = expr` assignment, `q` to quit), including the live `UEVAL_VERSION`.
- A successful `\name = expr` assignment now echoes `name = value` for confirmation, rather than completing silently.
- `calc` now detects the common typo of forgetting the `\` prefix for assignment (e.g. typing `x = 5` instead of `\x = 5`) and reports `did you mean: \x = 5 ?` instead of silently doing nothing. Without this, the bare `=` would be silently swallowed by `ueval`'s "trailing garbage is dropped" behavior (see Known Quirks) and give no indication that the assignment never happened. The check is conservative and does not fire on legitimate `==`, `!=`, `<=`, `>=` comparisons.
- `README.md`: new "`calc`: Interactive REPL" section documenting what the tool supports (expression evaluation, pre-bound math functions/constants, variable assignment, the forgotten-backslash typo check), the variable/function separate-namespace behavior (you can bind a variable named e.g. `sqrt` without breaking the `sqrt(...)` function, since they live in different tables), and what the tool deliberately does not do (no unbind/list, no persistence, no scripting).

### Fixed (calc.c / Makefile)

- `Makefile`'s `calc` target was missing `uedit.h` as a build dependency even though `calc.c` includes it, so `make calc` would not rebuild after an `uedit.h`-only change. Added.

## [0.1.0] - 2026-06-21

First versioned release. This snapshot reflects a documentation, testing,
and bug-fixing pass over the existing `ueval.h`, plus the introduction of
version macros and this changelog. No prior tagged version existed.

### Fixed

- **`<=` and `>=` previously evaluated as `<<` and `>>`.** The binary-operator
  dispatch in `ueval__expr` used token length alone (`len == 2`) to decide a
  `<`/`>` token was the shift form, so the two-character comparison operators
  fell into the same branch as the shift operators and were silently
  evaluated as shifts — `5 <= 5` returned `160` (i.e. `5 << 5`) instead of
  `1`. Fixed by capturing the operator's second character before the
  recursive right-hand-side parse (which would otherwise advance `env->src`
  and make a later check stale) and using it to correctly distinguish `<<`
  from `<=`, and `>>` from `>=`. **This is the one behavior change in this
  release that could affect existing expressions** — any expression using
  `<=` or `>=` that was unknowingly relying on the old shift behavior will
  now evaluate differently (correctly, as a comparison). Expressions using
  `<`, `>`, `<<`, `>>`, `==`, `!=` are unaffected.

### Added

- `UEVAL_VERSION`, `UEVAL_VERSION_MAJOR`, `UEVAL_VERSION_MINOR`,
  `UEVAL_VERSION_PATCH` macros at the top of `ueval.h`.
- `CHANGELOG.md` (this file).
- `test_ueval.c`: a standalone, dependency-free test suite (128 assertions)
  covering literals, every operator and its precedence, both ternary branches'
  short-circuit behavior (including a side-effect-counter check confirming
  untaken branches' function calls are never invoked), variable rebinding,
  `ueval_ptr`, function binding and arity (including the cases where extra
  arguments are silently accepted, on purpose, see "Known Quirks" below),
  dollar-variable mode, every documented error code, the recursion guard,
  multi-env independence, variable-table capacity, and the version macros
  themselves. Output is grouped into labeled sections with per-assertion
  `PASS`/`FAIL` lines describing exactly what was checked; supports a `-q`
  flag for quieter CI-style output. Wired into `Makefile` as `make test`.
- `bench_ueval.c`: a standalone microbenchmark measuring hot-path call cost
  for two input-update patterns (`ueval_ptr` pointer caching vs. per-call
  `ueval_bind` by name) across a bare-parse floor case, a realistic
  few-variable case, and a worst-case near-full variable-table case. Reports
  `PASS`/`FAIL` correctness checks confirming both patterns produce identical
  results, so the timing comparison itself is verified rather than assumed.
  Wired into `Makefile` as `make bench`.
- `README.md` expanded significantly:
  - "How Evaluation Works" section describing the single-pass,
    allocation-free precedence-climbing parser and its consequences
    (all-`double` values, C-style truthiness, env reuse, first-error-wins).
  - "Comparison Operators" section: full reference table, worked examples,
    and explicit documentation of non-chaining (`1 < 2 < 3` does **not**
    mean `(1<2) && (2<3)`) and float-equality-has-no-epsilon behavior.
  - "Known Quirks" section documenting, with verified before/after values:
    consecutive unary `!` XOR-toggling instead of idiomatic boolify; unary
    minus binding tighter than `**` (unlike Python); extra function
    arguments being silently accepted rather than rejected; trailing
    garbage after a valid expression never being reported; empty/whitespace
    input evaluating to `0` rather than erroring.
  - "Performance Notes" section with measured (not estimated) hot-path
    timings comparing `ueval_ptr` caching against per-call `ueval_bind`,
    aimed at high-frequency callback use (e.g. a MIDI note/pitchbend/CC
    handler), plus guidance on env-per-signal usage given the library's
    lack of internal locking.
  - "Testing" section documenting how to run the test suite and benchmark
    and how to read their output.
  - "Versioning" section (see below).

### Notes

- No function signatures changed in this release.
- The `<=`/`>=` fix is the only behavior change; everything else in this
  release is additive (tests, benchmarks, docs, version macros).
