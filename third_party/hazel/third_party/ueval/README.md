# `ueval`: Micro C-Expression Evaluator

[![CodeFactor](https://www.codefactor.io/repository/github/octetta/ueval/badge)](https://www.codefactor.io/repository/github/octetta/ueval)
[![Version](https://img.shields.io/badge/version-0.1.0-blue.svg)](CHANGELOG.md)

`ueval` is a standalone, thread-safe, header-only C library for evaluating mathematical and logical expressions. It uses a precedence-climbing parser to support hexadecimal literals, bitwise operations, C-style logical operators, short-circuit ternary evaluation, and custom C-function bindings.

A small interactive REPL built on top of it, [`calc`](#calc-interactive-repl), is included for trying things out from the command line.

Current version: **0.1.0** ([changelog](CHANGELOG.md) · [versioning policy](#versioning)) — see `UEVAL_VERSION` in `ueval.h` for the compile-time source of truth.

## Features

* **Header-Only**: Drop `ueval.h` into your project and go.
* **Thread-Safe**: No global state; all context is in a `ueval_env` struct.
* **C-Standard Precedence**: Operators behave exactly like native C.
* **Short-Circuit Ternary**: `cond ? a : b` skips the untaken branch entirely — so `x != 0 ? 1/x : 0` is safe even when `x` is zero.
* **Re-bindable Variables**: Calling `ueval_bind` again with the same name updates the value rather than creating a duplicate.
* **Unary Operators**: `-` (negate), `!` (logical NOT), `~` (bitwise NOT).
* **Power Operator**: `**` (right-associative, backed by `pow()`).
* **Recursion Guard**: Built-in depth counter prevents stack overflow.
* **Dollar-Variable Mode**: Opt-in `$name` syntax for variables — useful when bare identifiers conflict with surrounding systems.

---

## Quick Start

```c
#include "ueval.h"
#include <math.h>
#include <stdio.h>

int main(void) {
    ueval_env env;
    ueval_init(&env);

    ueval_bind(&env, "x", 2.0);
    ueval_bind_f1(&env, "sqrt", sqrt);

    ueval_result r = ueval_eval(&env, "sqrt(x) + x**3");
    if (r.status == UEVAL_OK)
        printf("%g\n", r.value); /* 9.41421 */
    else
        fprintf(stderr, "error: %s\n", r.error_msg);
}
```

---

## How Evaluation Works

`ueval` parses with a single-pass **precedence-climbing (Pratt-style) parser** — there is no separate tokenization pass, no AST, and no dynamic allocation. `ueval_eval` walks the input string once, character by character, and produces a `double` directly.

A few consequences of that design are worth knowing up front:

- **All values are `double`.** There is no integer type. Bitwise and shift operators (`& | ^ ~ << >>`) cast their operands to `long long` for the operation, then cast the result back to `double`. This is exact for the range of integers a `double` can represent exactly (up to 2^53), and matches what you'd expect from C, but it means very large bit patterns can lose precision.
- **Truthiness is C-style.** `0` is false, anything else (including negative numbers) is true. Logical operators (`&& || !`) always *return* `0.0` or `1.0`.
- **The environment is reset on every call.** `ueval_eval` clears the error state and recursion depth at the start of each call, so a single `ueval_env` can be reused indefinitely — including after an error — without needing to call `ueval_init` again.
- **Parsing stops at the first thing it can't consume — it does not require consuming the whole string.** This is the single most important quirk to know about; see [Known Quirks](#known-quirks).
- **Errors are "first wins."** Once `env->err` is set during a parse, no further parsing work overwrites it — the first error encountered is the one reported, even if other problems exist later in the string.

---

## Operator Precedence

Parentheses `()` always have the highest priority. The table below lists binary/unary operators from highest to lowest precedence.

| Prec | Operators           | Description                     | Notes              |
| :--- | :------------------ | :------------------------------ | :----------------- |
| 9    | `**`                | Power                           | Right-associative  |
| 8    | `*`  `/`  `%`       | Multiply, Divide, Modulo        |                    |
| 7    | `+`  `-`            | Add, Subtract                   |                    |
| 6    | `<<`  `>>`          | Bitwise Shift                   |                    |
| 5    | `<`  `>`  `<=`  `>=`| Relational Comparison           |                    |
| 4    | `==`  `!=`          | Equality                        |                    |
| 3    | `&`  `^`  `\|`      | Bitwise AND, XOR, OR            |                    |
| 2    | `&&`                | Logical AND                     |                    |
| 1    | `\|\|`              | Logical OR                      |                    |
| 0    | `? :`               | Ternary Conditional             | Short-circuits     |

Unary operators `-`, `!`, `~` are applied before any binary operator, and bind *tighter* than `**` — see [Known Quirks](#known-quirks) below, `-2**2` is `4`, not `-4`.

---

## Comparison Operators

`ueval` supports the full set of C-style relational and equality operators. All six return `1.0` for true or `0.0` for false — there's no separate boolean type, just `double`.

| Operator | Meaning                  | Precedence | Associativity |
| :------- | :------------------------ | :--------: | :------------ |
| `<`      | Less than                 | 5          | Left           |
| `>`      | Greater than               | 5          | Left           |
| `<=`     | Less than or equal to     | 5          | Left           |
| `>=`     | Greater than or equal to  | 5          | Left           |
| `==`     | Equal to                  | 4          | Left           |
| `!=`     | Not equal to               | 4          | Left           |

```c
ueval_eval(&env, "3 < 5");    /* 1.0 */
ueval_eval(&env, "5 < 3");    /* 0.0 */
ueval_eval(&env, "5 > 3");    /* 1.0 */
ueval_eval(&env, "3 > 5");    /* 0.0 */
ueval_eval(&env, "5 <= 5");   /* 1.0 */
ueval_eval(&env, "4 <= 5");   /* 1.0 */
ueval_eval(&env, "6 <= 5");   /* 0.0 */
ueval_eval(&env, "5 >= 5");   /* 1.0 */
ueval_eval(&env, "5 >= 3");   /* 1.0 */
ueval_eval(&env, "3 >= 5");   /* 0.0 */
ueval_eval(&env, "5 == 5");   /* 1.0 */
ueval_eval(&env, "5 == 4");   /* 0.0 */
ueval_eval(&env, "5 != 4");   /* 1.0 */
ueval_eval(&env, "5 != 5");   /* 0.0 */
```

A few things worth knowing:

- **Comparisons operate on `double`, including equality.** `==` and `!=` do a direct floating-point comparison with no epsilon/tolerance, so the result of any computation feeding into one should be an exact value (e.g. integers, or values built only from `+`/`-`/`*` on exact inputs) if you want reliable equality checks. `0.1 + 0.2 == 0.3` is `0.0`, just as in native C, because of standard floating-point representation error.
- **`<` / `>` / `<=` / `>=` sit one precedence level below shifts and one above equality.** `1 << 2 < 8` parses as `(1 << 2) < 8`, and `a < b == c < d` parses as `(a < b) == (c < d)`.
- **Comparisons chain left-to-right like any other same-precedence binary operator, not like Python.** `1 < 2 < 3` does **not** mean `(1 < 2) && (2 < 3)`. It parses as `(1 < 2) < 3`, i.e. `1.0 < 3`, which is `1.0`. Use `(1 < 2) && (2 < 3)` if you want the chained-comparison meaning.
- **Combine with `&&` / `||` for compound conditions**, since `&&`/`||` sit at lower precedence and don't need extra parens around simple comparisons: `x > 0 && x < 10` works as expected.
- **Comparisons compose naturally with the ternary operator**: `x >= 18 ? 1 : 0` is the idiomatic way to turn a comparison into an explicit value, though the comparison's own `1.0`/`0.0` result usually makes the ternary redundant.

```c
/* compound condition, no extra parens needed */
ueval_bind(&env, "x", 5.0);
ueval_eval(&env, "x > 0 && x < 10");   /* 1.0 */

/* chained comparison pitfall */
ueval_eval(&env, "1 < 2 < 3");          /* 1.0 — parses as (1 < 2) < 3, not chained */
ueval_eval(&env, "(1 < 2) && (2 < 3)"); /* 1.0 — the idiomatic chained-comparison form */
```

---

## API Reference

### Lifecycle

```c
void ueval_init(ueval_env *env);
```
Initialises the environment. Call once before anything else. To reset and reuse, call again.

```c
ueval_result ueval_eval(ueval_env *env, const char *expr);
```
Parses and evaluates `expr`. Returns a `ueval_result` with:
- `.value`     — the computed `double` (0 on error)
- `.status`    — `UEVAL_OK` or an error code
- `.error_msg` — human-readable description (empty string on success)

### Binding Variables

```c
int ueval_bind(ueval_env *env, const char *name, double value);
```
Binds `name` to `value`. If `name` already exists, its value is updated. Returns `0` on success, `-1` if the variable table is full (`UEVAL_MAX_VARS`, default 32).

```c
double *ueval_ptr(ueval_env *env, const char *name);
```
Returns a pointer to the stored value of a bound variable, or `NULL` if not found. The pointer is stable for the lifetime of the env. Use it to read or update the variable directly on the hot path — no function call needed after the initial lookup:

```c
ueval_bind(&env, "vel", 0.0);
double *vel = ueval_ptr(&env, "vel");
// later, in a MIDI callback or tight loop:
*vel = incoming_value;
ueval_eval(&env, expr);
```

### Binding Functions

```c
int ueval_bind_f1(ueval_env *env, const char *name, ueval_func1 f);
int ueval_bind_f2(ueval_env *env, const char *name, ueval_func2 f);
```
Binds a 1- or 2-argument C function. Replaces an existing binding with the same name. Returns `0` on success, `-1` if the function table is full (`UEVAL_MAX_FUNCS`, default 32).

### Error Codes

| Code | Meaning |
| :--- | :------ |
| `UEVAL_OK` | Success |
| `UEVAL_ERR_SYMBOL_NOT_FOUND` | Unbound variable or function name |
| `UEVAL_ERR_EXPECTED_CLOSE_PAREN` | Missing `)` |
| `UEVAL_ERR_EXPECTED_COLON` | Missing `:` in ternary |
| `UEVAL_ERR_INVALID_ARGS` | Wrong number of arguments to a function |
| `UEVAL_ERR_DIVISION_BY_ZERO` | Division or modulo by zero |
| `UEVAL_ERR_STACK_OVERFLOW` | Expression exceeds `UEVAL_MAX_DEPTH` (default 64) |
| `UEVAL_ERR_UNEXPECTED_CHAR` | Unrecognised character in input |

---

## Configuration

Define these before including `ueval.h` to override the defaults:

```c
#define UEVAL_MAX_VARS  64   /* default: 32 */
#define UEVAL_MAX_FUNCS 16   /* default: 32 */
#define UEVAL_MAX_DEPTH 32   /* default: 64 */
#include "ueval.h"
```

---

## Examples

### Ternary with Short-Circuit (Safe Division)

The false branch is never evaluated when the condition is true, and vice versa.

```c
ueval_bind(&env, "x", 0.0);
ueval_result r = ueval_eval(&env, "x != 0 ? 1/x : -1");
/* r.value = -1.0 — no division-by-zero error */
```

### Direct Variable Access (Hot Path)

Bind once during setup, then get a stable pointer with `ueval_ptr` and update it directly — no function call overhead on the hot path.

```c
ueval_bind(&env, "vel", 0.0);
ueval_bind(&env, "cc7", 0.0);
double *vel = ueval_ptr(&env, "vel");
double *cc7 = ueval_ptr(&env, "cc7");

// In a real-time MIDI callback:
*vel = msg.velocity / 127.0;
*cc7 = msg.cc_value / 127.0;
ueval_result r = ueval_eval(&env, expr);
```

### Re-binding a Variable

```c
ueval_bind(&env, "t", 0.0);
for (int i = 0; i < 10; i++) {
    ueval_bind(&env, "t", (double)i);   /* updates in place */
    ueval_result r = ueval_eval(&env, "t * t + 2*t + 1");
    printf("%g\n", r.value);
}
```

### Bitwise / Hex Literals

```c
/* Check if bit 0 is set, return 0xAA or 0xBB accordingly */
ueval_result r = ueval_eval(&env, "(0xFF & 0x01) ? 0xAA : 0xBB");
/* r.value = 170.0 (0xAA) */
```

### Unary Operators

```c
ueval_bind(&env, "flag", 0.0);
ueval_eval(&env, "!flag");   /* 1.0 — logical NOT  */
ueval_eval(&env, "~0xF0");   /* bitwise NOT        */
ueval_eval(&env, "-(2+3)");  /* -5.0               */
```

Consecutive `!` characters are collapsed into one toggle (XOR'd) before being applied — they are **not** applied value-by-value like idiomatic C. An even count cancels out completely:

```c
ueval_eval(&env, "!!5");     /* 5.0  — NOT 1.0; two '!' cancel out      */
ueval_eval(&env, "!!!5");    /* 0.0  — odd count behaves like one '!'   */
```

Unary minus also binds *tighter* than `**`, so it applies to the base before the power is taken:

```c
ueval_eval(&env, "-2**2");    /* 4.0  — parses as (-2)**2, not -(2**2) */
ueval_eval(&env, "-(2**2)");  /* -4.0 — use explicit parens for this   */
```

### Power Operator

```c
ueval_eval(&env, "2**10");       /* 1024.0 */
ueval_eval(&env, "2**2**3");     /* 256.0  — right-associative: 2**(2**3) */
```

### Handling Errors

```c
ueval_result r = ueval_eval(&env, "1 / 0");
if (r.status != UEVAL_OK)
    fprintf(stderr, "error: %s\n", r.error_msg);
/* error: division by zero: '/' */
```

### Dollar-Variable Mode

Set `env.dollar_vars = 1` to require variables to be written as `$name`. Functions are unaffected and are still called without a prefix.

```c
ueval_env env;
ueval_init(&env);
env.dollar_vars = 1;

ueval_bind(&env, "x", 10.0);
ueval_bind(&env, "y", 3.0);
ueval_bind_f1(&env, "sqrt", sqrt);

ueval_result r = ueval_eval(&env, "$x * $y + sqrt($x)");
/* r.value = 33.162 */
```

In this mode, a bare identifier that isn't a function call is an error:

```c
ueval_result r = ueval_eval(&env, "x * 2"); /* forgot the $ */
/* r.status = UEVAL_ERR_SYMBOL_NOT_FOUND, r.error_msg = "symbol not found: '$x'" */
```

The error message includes the `$` hint so the cause is immediately obvious.

---

### Custom Two-Argument Function

```c
double clamp(double v, double hi) { return v > hi ? hi : v; }

ueval_bind_f2(&env, "clamp", clamp);
ueval_bind(&env, "VOL", 1.2);
ueval_result r = ueval_eval(&env, "clamp(VOL, 1.0)");
/* r.value = 1.0 */
```

---

## `calc`: Interactive REPL

`calc.c` is a small companion REPL built on top of `ueval.h` and `uedit.h` (a terminal line editor included in this repo). It's both a usable interactive calculator and a worked example of wiring `ueval` into an application.

```sh
make calc
./calc
```

On startup it prints a one-line reminder of how to use it, including the live library version:

```
ueval calc 0.1.0 -- type an expression, \name = expr to assign, q to quit
```

### What it can do

- **Evaluate any expression `ueval` supports** — see [Operator Precedence](#operator-precedence) and [Comparison Operators](#comparison-operators) for the full grammar:

  ```
  # 2 + 3 * 4
  = 14
  # sin(pi / 2)
  = 1
  # 5 <= 5
  = 1
  ```

- **Built-in math functions and constants are pre-bound**, so they're available immediately without any setup: `sin`, `cos`, `tan`, `sqrt`, `log`, `log2`, `abs`, `exp`, `ceil`, `floor` (all 1-argument), `pow`, `fmod` (both 2-argument), plus the constants `pi` and `e`.

- **Assign a variable with `\name = expr`** — evaluates the right-hand side and binds the result under `name` for later expressions. A successful assignment echoes back what was bound:

  ```
  # \x = 3.14
  x = 3.14
  # x * 2
  = 6.28
  # \y = x + 1
  y = 4.14
  ```

  Re-running an assignment for the same name updates it in place (`ueval_bind`'s normal behavior — see [API Reference](#api-reference)), and the right-hand side can reference the variable's own current value:

  ```
  # \x = x + 10
  x = 13.14
  ```

- **The `\` prefix is required for assignment, and a forgotten `\` is caught and reported.** `ueval`'s expression grammar has no `=` operator — only `==`, `!=`, `<=`, `>=` — so typing `x = 5` instead of `\x = 5` doesn't mean what it looks like it means: by itself, `ueval_eval` would parse just the `x` part, return its current value, and silently drop ` = 5` as unreported trailing garbage (see [Known Quirks](#known-quirks)). `calc` checks for this specific shape of typo — a single, isolated `=` on a line that doesn't start with `\` — and reports it instead of evaluating it as if nothing were wrong:

  ```
  # x = 5
  # did you mean: \x = 5 ?  (assignment needs a leading '\')
  ```

  This check is conservative: it only fires on a lone `=`, so legitimate comparisons like `x == 5`, `x != 5`, `x <= 5`, and `x >= 5` are left alone and evaluated normally.

- **`exit` or `q` quits.**

### Variables and functions are separate namespaces

`ueval` keeps bound variables and bound functions in two separate tables internally (see [API Reference](#api-reference)), and `calc` inherits that directly: you can bind a variable with the same name as one of the pre-bound math functions without breaking the function, because `name` (bare) and `name(...)` (call syntax) look up different tables.

```
# \sqrt = 9
sqrt = 9
# sqrt(16)
= 4
# sqrt
= 9
```

`sqrt(16)` still calls the real `sqrt` function; bare `sqrt` now reads the variable. This is occasionally useful (e.g. naming a control-rate value the same as a related function) but is also a sharp edge worth knowing about before you hit it by accident.

### What it deliberately doesn't do

- **No way to unbind a variable or list what's currently bound.** Everything lives for the life of the process; restart `calc` to clear state.
- **No persistence between runs.** Bindings aren't saved to a file and reloaded.
- **No multi-line input or scripting.** Each line is one expression or one assignment.

None of this is a limitation of `ueval` itself — `calc` is intentionally a minimal example, not a full-featured calculator application. If you need any of the above, they're straightforward to add on top of the existing `ueval_bind`/`ueval_ptr`/`ueval_eval` API.

---

## Known Quirks

These behaviors were verified directly against the current `ueval.h` source while building out the test suite. None of them crash or corrupt state — they're silent semantic surprises, which is exactly why they're worth documenting and pinning down with tests.

> **Note:** an earlier version of this README documented a bug where `<=` and `>=` silently evaluated as `<<` and `>>` (the dispatch switch used token length alone to mean "this is a shift," and `<=`/`>=` are also 2 characters). This has been **fixed** — the operator's second character is now captured before the recursive right-hand-side parse and used to correctly disambiguate `<<` from `<=`, and `>>` from `>=`. `5 <= 5` now correctly evaluates to `1`. The test suite asserts the corrected behavior.

### Extra function arguments are silently accepted

`UEVAL_ERR_INVALID_ARGS` is correctly raised when a 2-argument function is called with *too few* arguments. It is **not** raised when either a 1- or 2-argument function is called with *too many*:

```c
ueval_eval(&env, "sqrt(16, 99)");   /* 4.0 — extra arg ", 99" silently ignored   */
ueval_eval(&env, "clamp(5, 3, 9)"); /* 3.0 — extra arg ", 9" silently ignored    */
ueval_eval(&env, "clamp(5)");       /* error: UEVAL_ERR_INVALID_ARGS (too few)  */
```

### Trailing garbage after a valid expression is never reported

`ueval_eval` does not verify that parsing consumed the whole input string. As soon as the precedence-climbing loop hits something it can't interpret as an operator, it simply returns whatever value it has computed so far:

```c
ueval_eval(&env, "1 + 1 @");  /* 2.0 — the '@' is silently dropped, no error */
ueval_eval(&env, "5 5");      /* 5.0 — second literal is never reached       */
ueval_eval(&env, "0xZZ");     /* 0.0 — "0x" parses as 0, "ZZ" is dropped     */
```

If you're evaluating live user-typed input (e.g. a REPL or text field), don't rely on `UEVAL_OK` to mean "the whole string was meaningful" — it only means "the prefix we managed to parse didn't hit an error."

### Empty and whitespace-only input is `0.0`, not an error

```c
ueval_eval(&env, "");    /* 0.0, status = UEVAL_OK */
ueval_eval(&env, "   "); /* 0.0, status = UEVAL_OK */
```

There's no dedicated "empty expression" error code — an empty string is treated the same as a literal `0`.

---

## Testing

A standalone test suite (`test_ueval.c`) exercises literals, every operator and its precedence, both ternary branches' short-circuit behavior, variable rebinding, `ueval_ptr`, function binding/arity, dollar-variable mode, every error code, the recursion guard, and all of the quirks above — each quirk is asserted against its *actual* current behavior so a regression (or an upstream fix that changes behavior) shows up immediately as a failing test rather than silently.

Output is grouped into named sections, and every individual assertion prints a labeled `PASS`/`FAIL` line describing exactly what it checked and what it got — nothing passes silently. Each section prints its own subtotal, and a grand total closes the run:

```
== Logical and comparison operators ==
  PASS  <= equal case              "5 <= 5" -> 1
  PASS  <= false case              "5 <= 4" -> 0
  ...
   (28 passed, 0 failed)
```

```sh
make test
```

or directly:

```sh
gcc -Wall -Wextra test_ueval.c -o test_ueval -lm
./test_ueval
```

Pass `-q` (or `--quiet`) to suppress individual `PASS` lines and show only section headers, subtotals, and any failures — useful for CI logs where you mainly want to know if something broke:

```sh
./test_ueval -q
```

It has no dependencies beyond the standard library and `ueval.h` itself, exits `0` on success / `1` on any failure, and is suitable as a CI gate.

A separate benchmark (`bench_ueval.c`) measures hot-path call cost directly, and also reports labeled `PASS`/`FAIL` correctness checks confirming the patterns being timed actually produce identical results (so the timing comparison itself is meaningful) — see [Performance Notes](#performance-notes) for how to run it and how to read the results.

---

## Performance Notes

`ueval` was designed for exactly this shape of workload: a short expression, re-evaluated very frequently, against a handful of inputs that change on every call (a MIDI note/pitchbend/CC handler, a per-sample or per-block DSP control-rate update, a live-coding REPL, etc.). The numbers below are from `bench_ueval.c` (included alongside the test suite — see [Testing](#testing)), built with `gcc -O2` on the machine these docs were written on. Build flags, CPU, and compiler all shift the absolute numbers; what's stable across runs is the *shape* of the result.

### Don't `ueval_bind` on the hot path — cache a pointer instead

`ueval_bind` does a linear `strncmp` scan over every currently-bound variable before it finds (or creates) the slot to write to. That's the right cost to pay once at setup time, but it's wasted work if you pay it again on every call just to push in a new value.

`ueval_ptr` exists for exactly this: look up the variable's storage location once, then read or write through that pointer directly — no scan, no string comparison, on the call itself.

```c
/* Setup, once: */
ueval_bind(&env, "vel", 0.0);
ueval_bind(&env, "cc7", 0.0);
double *vel = ueval_ptr(&env, "vel");
double *cc7 = ueval_ptr(&env, "cc7");

/* Hot path, every call: */
*vel = incoming_velocity;
*cc7 = incoming_cc_value;
ueval_result r = ueval_eval(&env, expr);
```

Measured difference, 2,000,000 calls per scenario, evaluating a short expression against the changing input(s):

| Scenario                                   | `ueval_ptr` cache | `ueval_bind` by name | Slowdown |
| :------------------------------------------ | -----------------: | ---------------------: | -------: |
| 3 bound variables                          | ~258 ns/call      | ~265 ns/call          | ~1.02×   |
| 32 bound variables, hot var in last slot   | ~163 ns/call      | ~247 ns/call          | ~1.52×   |

With only a few variables bound (the common case for a single control-rate expression), the difference is small — the linear scan over 3 entries is nearly free. It grows as the variable table fills up, since `ueval_bind`'s scan cost is `O(var_count)` per call while `ueval_ptr`'s write is `O(1)`. **Cache the pointer regardless** — it costs nothing extra to set up and removes the scan entirely, so there's no real reason to take the `ueval_bind` cost on a repeated call even when it's currently small.

### The parse itself is the real floor cost

Even with pointer caching, most of the per-call time is the precedence-climbing parse walking the expression string from scratch — `ueval` has no AST or bytecode to cache, by design (header-only, no dynamic allocation). In the same benchmark, parsing a trivial literal expression (`"1 + 1"`, no variables at all) costs roughly **85 ns/call**, which is most of the ~163 ns/call floor seen above for a short expression with one variable reference.

Practical implications:

- **Keep hot-path expressions short.** A few terms and one or two function calls costs barely more than a literal; a long expression with many operators and variable references costs proportionally more, since the whole string is re-walked every call.
- **There's no per-call allocation to worry about.** No `malloc`/`free` happens anywhere in `ueval_eval`, so there's nothing to free or leak, and behavior is consistent call to call — useful for a callback you don't want introducing GC-style pauses or fragmentation over a long-running session.
- **At MIDI rates, none of this is likely to matter.** Even the busiest realistic MIDI control stream (dense pitchbend/CC at full resolution) is several orders of magnitude slower than a few hundred nanoseconds per message. The advice above is about not leaving easy headroom on the table, not about hitting a wall — if you're calling `ueval_eval` once per audio sample instead of once per control-rate update, the calculus changes and you'd want to measure for your specific expression and call rate.

### One `ueval_env` per independently-updated signal

Since `ueval_env` holds no global state and the library does no internal locking (see [Memory & Thread Safety](#memory--thread-safety)), the cheapest way to handle multiple independent inputs — separate MIDI channels, separate synth voices, note vs. pitchbend vs. CC — is usually a separate `ueval_env` (and its own cached `ueval_ptr` handles) per signal, rather than sharing one env across threads with external locking. Set each one up once, then in steady state only ever touch it through cached pointers and `ueval_eval`.

### Verifying on your own hardware

```sh
make bench
```

or directly:

```sh
gcc -O2 bench_ueval.c -o bench_ueval -lm
./bench_ueval
```

This prints the same three scenarios above (bare-parse floor, a realistic small-variable-count case, and a worst-case near-full variable table), each clearly labeled with the expression and variable count being measured, so you can get real numbers for your actual compiler, flags, and target hardware rather than relying on the figures here. Alongside the timings, it also prints labeled `PASS`/`FAIL` checks confirming Pattern A and Pattern B computed identical results in each scenario — if those ever show `FAIL`, the timing comparison for that run isn't trustworthy and should be investigated before trusting the ns/call numbers.

---

## Memory & Thread Safety

`ueval` uses no dynamic allocation. Every `ueval_env` is fully self-contained, so multiple environments can exist simultaneously in different threads without any locking. Do not share a single `ueval_env` between threads without external synchronisation.

---

## Versioning

`ueval` follows [Semantic Versioning](https://semver.org) (`MAJOR.MINOR.PATCH`), with the current version available at compile time via macros defined at the top of `ueval.h`:

```c
#define UEVAL_VERSION_MAJOR 0
#define UEVAL_VERSION_MINOR 1
#define UEVAL_VERSION_PATCH 0
#define UEVAL_VERSION "0.1.0"
```

`UEVAL_VERSION` is a string for logging/display; the three numeric macros are meant for preprocessor checks, e.g. `#if UEVAL_VERSION_MAJOR >= 1`.

Since `ueval` is a single header consumed directly via `#include`, not a linked library, there's no separate runtime version query — the macros above are the only source of truth, and they're checked for self-consistency in the test suite ([Testing](#testing)).

### What counts as MAJOR / MINOR / PATCH here

For a parser/expression-evaluator library, "the public API" means more than function signatures — it also means *what a given expression evaluates to*. A change that makes `"5 <= 5"` return something different than it used to is just as breaking for a downstream user as renaming `ueval_eval`. With that in mind:

- **MAJOR** — a previously-successful expression now evaluates to a different result, or now fails where it used to succeed (or vice versa), or an existing function's signature changes. Several items in [Known Quirks](#known-quirks) are candidates for a future MAJOR bump if they're ever tightened — e.g. making extra function arguments an error, or making trailing garbage after a valid expression an error. Either of those would correctly reject input that currently succeeds silently, which is a breaking change for anyone with that pattern baked into a saved expression/preset.
- **MINOR** — new, additive capability with no change to existing behavior: new bound-function helpers, new error codes, new configuration macros, and so on. Old code keeps compiling and keeps behaving identically.
- **PATCH** — bug fixes that make behavior match documented or clearly-intended semantics, with no signature changes. The `<=`/`>=` shift-collision fix (see [CHANGELOG.md](CHANGELOG.md)) is the model case: `<=` was always documented and intended to mean "less than or equal," so making it actually do that is a patch, not a behavior change anyone could legitimately be relying on.

### Why this project is starting at 0.1.0, not 1.0.0

Per semver, `0.x` versions explicitly mean "anything may still change." That's an accurate description of where `ueval` is right now — the [Known Quirks](#known-quirks) section documents several open behavioral questions (should extra function arguments be rejected? should trailing garbage be reported?) that may reasonably be resolved in ways that change behavior for existing expressions. Moving to `1.0.0` is meant to be a deliberate signal that the expression language's semantics are considered locked in, not a default version to start from.

### Changelog

See [CHANGELOG.md](CHANGELOG.md) for the full history of changes, organized by version, in [Keep a Changelog](https://keepachangelog.com) format.
