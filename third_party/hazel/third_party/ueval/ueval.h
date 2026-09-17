/*
 * ueval.h - Single-header arithmetic expression evaluator
 *
 * QUICK START:
 *   ueval_env env;
 *   ueval_init(&env);
 *   ueval_bind(&env, "x", 3.14);
 *   ueval_bind_f1(&env, "sin", sin);
 *   ueval_result r = ueval_eval(&env, "sin(x) * 2");
 *   if (r.status == UEVAL_OK) printf("%f\n", r.value);
 *
 * DOLLAR-VARIABLE MODE (optional):
 *   env.dollar_vars = 1;
 *   ueval_result r = ueval_eval(&env, "sin($x) * 2");
 *
 * SUPPORTED SYNTAX:
 *   Literals:      3.14, 0xFF, 1e10
 *   Arithmetic:    + - * / %
 *   Bitwise:       & | ^ ~ << >>
 *   Logical:       && || !
 *   Comparison:    == != < > <= >=
 *   Ternary:       cond ? a : b   (short-circuit: only one branch evaluated)
 *   Grouping:      (expr)
 *   Functions:     f(x) or f(x, y)
 *   Variables:     any bound name, or $name in dollar-variable mode
 *
 * DOLLAR-VARIABLE MODE:
 *   env.dollar_vars = 1;   // enable: variables must be written as $name
 *   env.dollar_vars = 0;   // disable (default): bare names as usual
 *   In this mode bare identifiers are rejected; only $name is recognised.
 *   Functions are still called without a $ prefix (sin(x) not $sin(x)).
 */

#ifndef UEVAL_H
#define UEVAL_H

/* ---------------------------------------------------------------------------
 * Version
 *
 * Semantic versioning (https://semver.org), adapted for a single-header C
 * expression-evaluator library where "API" means both function signatures
 * AND expression-language behavior:
 *
 *   MAJOR - a previously-successful expression now evaluates to a different
 *           result or now errors, OR an existing function's signature
 *           changes. (Example: if the "extra function arguments are
 *           silently accepted" quirk documented in the README is ever
 *           tightened into an error, that's a MAJOR bump even though no
 *           C signature changes - a previously-accepted expression now
 *           fails, which can break saved presets/patches.)
 *   MINOR - new, additive capability with no change to existing behavior:
 *           new bound-function helpers, new error codes, new config
 *           macros, etc. Old code keeps compiling and behaving the same.
 *   PATCH - bug fixes that make behavior match documented/intended
 *           semantics, with no signature changes. (Example: the <=/>=
 *           shift-collision fix - "<=" was always documented/intended to
 *           mean "less than or equal", so making it actually do that is
 *           a patch, not a behavior change someone could be relying on.)
 *
 * 0.x means "anything may still change" per semver - this library is still
 * in that phase. See README "Versioning" and CHANGELOG.md for details and
 * history.
 * ------------------------------------------------------------------------ */
#define UEVAL_VERSION_MAJOR 0
#define UEVAL_VERSION_MINOR 1
#define UEVAL_VERSION_PATCH 0
#define UEVAL_VERSION "0.1.0"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ---------- tunables ---------------------------------------------------- */

#ifndef UEVAL_MAX_VARS
#  define UEVAL_MAX_VARS 32
#endif
#ifndef UEVAL_MAX_FUNCS
#  define UEVAL_MAX_FUNCS 32
#endif
#ifndef UEVAL_MAX_DEPTH
#  define UEVAL_MAX_DEPTH 64
#endif

/* ---------- public types ------------------------------------------------- */

typedef enum {
    UEVAL_OK = 0,
    UEVAL_ERR_SYMBOL_NOT_FOUND,
    UEVAL_ERR_EXPECTED_CLOSE_PAREN,
    UEVAL_ERR_EXPECTED_COLON,
    UEVAL_ERR_INVALID_ARGS,
    UEVAL_ERR_DIVISION_BY_ZERO,
    UEVAL_ERR_STACK_OVERFLOW,
    UEVAL_ERR_UNEXPECTED_CHAR
} ueval_status;

typedef struct {
    double      value;
    ueval_status status;
    char        error_msg[80]; /* human-readable description */
} ueval_result;

typedef double (*ueval_func1)(double);
typedef double (*ueval_func2)(double, double);

typedef struct {
    char   name[32];
    double value;
} ueval__var;

typedef struct {
    char         name[32];
    ueval_func1  f1;
    ueval_func2  f2;
    int          args; /* 1 or 2 */
} ueval__func;

typedef struct {
    const char   *src;         /* current parse position */
    ueval__var    vars[UEVAL_MAX_VARS];
    ueval__func   funcs[UEVAL_MAX_FUNCS];
    int           var_count;
    int           func_count;
    int           depth;
    ueval_status  err;
    char          err_detail[48];
    int           dollar_vars; /* if non-zero, variables must be prefixed with $ */
} ueval_env;

/* ---------- forward declarations (internal) ------------------------------ */

static double ueval__expr(ueval_env *env, int min_prec);

/* ---------- error helpers ------------------------------------------------ */

static void ueval__err(ueval_env *env, ueval_status s, const char *detail) {
    if (env->err == UEVAL_OK) {
        env->err = s;
        strncpy(env->err_detail, detail ? detail : "", sizeof(env->err_detail) - 1);
        env->err_detail[sizeof(env->err_detail) - 1] = '\0';
    }
}

static const char *ueval__status_str(ueval_status s) {
    switch (s) {
        case UEVAL_OK:                    return "OK";
        case UEVAL_ERR_SYMBOL_NOT_FOUND:  return "symbol not found";
        case UEVAL_ERR_EXPECTED_CLOSE_PAREN: return "expected ')'";
        case UEVAL_ERR_EXPECTED_COLON:    return "expected ':' in ternary";
        case UEVAL_ERR_INVALID_ARGS:      return "wrong number of arguments";
        case UEVAL_ERR_DIVISION_BY_ZERO:  return "division by zero";
        case UEVAL_ERR_STACK_OVERFLOW:    return "expression too deeply nested";
        case UEVAL_ERR_UNEXPECTED_CHAR:   return "unexpected character";
        default:                          return "unknown error";
    }
}

/* ---------- skip whitespace --------------------------------------------- */

static void ueval__skip(ueval_env *env) {
    while (isspace((unsigned char)*env->src)) env->src++;
}

/* ---------- primary / unary --------------------------------------------- */

static double ueval__primary(ueval_env *env) {
    ueval__skip(env);

    if (env->err) return 0;
    if (env->depth >= UEVAL_MAX_DEPTH) {
        ueval__err(env, UEVAL_ERR_STACK_OVERFLOW, "");
        return 0;
    }

    /* unary minus/plus: loop, not recurse, to avoid stack abuse */
    int negate = 0;
    while (*env->src == '-' || *env->src == '+') {
        if (*env->src == '-') negate ^= 1;
        env->src++;
        ueval__skip(env);
    }

    /* unary logical NOT */
    int lnot = 0;
    while (*env->src == '!') {
        /* peek: must not be != operator */
        if (env->src[1] == '=') break;
        lnot ^= 1;
        env->src++;
        ueval__skip(env);
    }

    /* unary bitwise NOT */
    int bnot = 0;
    if (*env->src == '~') {
        bnot = 1;
        env->src++;
        ueval__skip(env);
    }

    double val = 0;

    /* parenthesised sub-expression */
    if (*env->src == '(') {
        env->src++;
        env->depth++;
        val = ueval__expr(env, 0);
        env->depth--;
        ueval__skip(env);
        if (*env->src == ')') {
            env->src++;
        } else {
            ueval__err(env, UEVAL_ERR_EXPECTED_CLOSE_PAREN, "");
        }

    /* dollar-variable: $name — only a variable, never a function call */
    } else if (*env->src == '$') {
        env->src++; /* consume $ */
        if (!isalpha((unsigned char)*env->src) && *env->src != '_') {
            ueval__err(env, UEVAL_ERR_UNEXPECTED_CHAR, "$");
        } else {
            const char *start = env->src;
            while (isalnum((unsigned char)*env->src) || *env->src == '_') env->src++;
            int len = (int)(env->src - start);
            char name[32];
            strncpy(name, start, len < 31 ? len : 31);
            name[len < 31 ? len : 31] = '\0';

            int found = 0;
            for (int i = 0; i < env->var_count; i++) {
                if (strcmp(name, env->vars[i].name) == 0) {
                    val = env->vars[i].value;
                    found = 1;
                    break;
                }
            }
            if (!found) ueval__err(env, UEVAL_ERR_SYMBOL_NOT_FOUND, name);
        }

    /* identifier: variable or function call.
     * In dollar_vars mode, bare identifiers are only accepted as function names. */
    } else if (isalpha((unsigned char)*env->src) || *env->src == '_') {
        const char *start = env->src;
        while (isalnum((unsigned char)*env->src) || *env->src == '_') env->src++;
        int len = (int)(env->src - start);
        char name[32];
        strncpy(name, start, len < 31 ? len : 31);
        name[len < 31 ? len : 31] = '\0';

        ueval__skip(env);

        if (*env->src == '(') {
            /* function call — always allowed regardless of dollar_vars */
            env->src++;
            env->depth++;

            /* find function */
            int fi = -1;
            for (int i = 0; i < env->func_count; i++) {
                if (strcmp(name, env->funcs[i].name) == 0) { fi = i; break; }
            }
            if (fi < 0) {
                ueval__err(env, UEVAL_ERR_SYMBOL_NOT_FOUND, name);
                env->depth--;
                return 0;
            }

            double a1 = ueval__expr(env, 0);
            ueval__skip(env);

            if (env->funcs[fi].args == 1) {
                if (*env->src == ')') env->src++;
                env->depth--;
                val = env->funcs[fi].f1(a1);
            } else {
                /* two-arg function */
                if (*env->src == ',') env->src++;
                else ueval__err(env, UEVAL_ERR_INVALID_ARGS, name);
                double a2 = ueval__expr(env, 0);
                ueval__skip(env);
                if (*env->src == ')') env->src++;
                env->depth--;
                val = env->funcs[fi].f2(a1, a2);
            }
        } else if (env->dollar_vars) {
            /* bare name where a variable was expected — tell the user */
            char hint[36];
            snprintf(hint, sizeof(hint), "$%s", name);
            ueval__err(env, UEVAL_ERR_SYMBOL_NOT_FOUND, hint);
        } else {
            /* normal variable lookup */
            int found = 0;
            for (int i = 0; i < env->var_count; i++) {
                if (strcmp(name, env->vars[i].name) == 0) {
                    val = env->vars[i].value;
                    found = 1;
                    break;
                }
            }
            if (!found) ueval__err(env, UEVAL_ERR_SYMBOL_NOT_FOUND, name);
        }

    /* numeric literal */
    } else if (isdigit((unsigned char)*env->src) || *env->src == '.') {
        char *end;
        if (env->src[0] == '0' && (env->src[1] == 'x' || env->src[1] == 'X'))
            val = (double)strtoll(env->src, &end, 16);
        else
            val = strtod(env->src, &end);
        if (end == env->src) {
            ueval__err(env, UEVAL_ERR_UNEXPECTED_CHAR, "");
        }
        env->src = end;

    } else if (*env->src != '\0') {
        char bad[4] = {*env->src, '\0'};
        ueval__err(env, UEVAL_ERR_UNEXPECTED_CHAR, bad);
    }

    if (negate) val = -val;
    if (lnot)   val = (val == 0) ? 1.0 : 0.0;
    if (bnot)   val = (double)(~(long long)val);

    return val;
}

/* ---------- operator precedence table ------------------------------------ */

typedef struct { int prec; int len; char op; char op2; } ueval__op;

static int ueval__op_info(const char *p, int *out_prec, int *out_len) {
    /* returns 0 if not an infix operator at this position */
    struct { int prec; int len; const char *tok; } ops[] = {
        {9, 2, "**"},   /* power (right-assoc handled separately) */
        {8, 1, "*"},
        {8, 1, "/"},
        {8, 1, "%"},
        {7, 1, "+"},
        {7, 1, "-"},
        {6, 2, "<<"},
        {6, 2, ">>"},
        {5, 2, "<="},
        {5, 2, ">="},
        {5, 1, "<"},
        {5, 1, ">"},
        {4, 2, "=="},
        {4, 2, "!="},
        {3, 1, "&"},   /* bitwise AND (before &&) */
        {3, 1, "^"},
        {3, 1, "|"},   /* bitwise OR  (before ||) */
        {2, 2, "&&"},
        {1, 2, "||"},
        {0, 1, "?"},
    };
    for (int i = 0; i < (int)(sizeof(ops)/sizeof(ops[0])); i++) {
        int l = ops[i].len;
        if (strncmp(p, ops[i].tok, l) == 0) {
            /* Don't confuse & with && or | with || */
            if (l == 1 && ops[i].tok[0] == '&' && p[1] == '&') continue;
            if (l == 1 && ops[i].tok[0] == '|' && p[1] == '|') continue;
            *out_prec = ops[i].prec;
            *out_len  = l;
            return 1;
        }
    }
    return 0;
}

/* ---------- expression parser (Pratt / precedence climbing) -------------- */

static double ueval__expr(ueval_env *env, int min_prec) {
    double left = ueval__primary(env);

    while (!env->err) {
        ueval__skip(env);
        char c = *env->src;

        /* terminators */
        if (!c || c == ')' || c == ',' || c == ':' || c == ';') break;

        int prec, len;
        if (!ueval__op_info(env->src, &prec, &len)) break;
        if (prec < min_prec) break;

        /* ternary: short-circuit evaluation */
        if (c == '?') {
            env->src += len;
            int cond = (left != 0);
            /* parse true branch — skip if cond is false */
            if (!cond) {
                /* fast-forward by counting depth */
                int d = 0;
                while (*env->src && !(d == 0 && *env->src == ':')) {
                    if (*env->src == '(') d++;
                    else if (*env->src == ')') d--;
                    env->src++;
                }
                ueval__skip(env);
                if (*env->src == ':') { env->src++; left = ueval__expr(env, 0); }
                else ueval__err(env, UEVAL_ERR_EXPECTED_COLON, "");
            } else {
                double true_val = ueval__expr(env, 0);
                ueval__skip(env);
                if (*env->src == ':') {
                    env->src++;
                    /* skip false branch */
                    int d = 0;
                    while (*env->src && !(!d && (*env->src == ')' || *env->src == ',' || *env->src == ';'))) {
                        if (*env->src == '(') d++;
                        else if (*env->src == ')') { if (d == 0) break; d--; }
                        env->src++;
                    }
                    left = true_val;
                } else {
                    ueval__err(env, UEVAL_ERR_EXPECTED_COLON, "");
                }
            }
            continue;
        }

        /* right-associative power */
        int next_min = (c == '*' && env->src[1] == '*') ? prec : prec + 1;

        /* capture the operator's second character (if any) now, before the
         * recursive call below advances env->src past the right operand */
        char c2 = (len == 2) ? env->src[1] : '\0';

        env->src += len;
        double right = ueval__expr(env, next_min);

        if (env->err) break;

        /* apply operator */
        switch (c) {
            case '+': left += right; break;
            case '-': left -= right; break;
            case '*':
                if (len == 2) { left = pow(left, right); } /* ** */
                else          { left *= right; }
                break;
            case '/':
                if (right == 0.0) { ueval__err(env, UEVAL_ERR_DIVISION_BY_ZERO, "/"); }
                else              { left /= right; }
                break;
            case '%':
                if ((long long)right == 0) { ueval__err(env, UEVAL_ERR_DIVISION_BY_ZERO, "%"); }
                else { left = (double)((long long)left % (long long)right); }
                break;
            case '<':
                if (len == 2 && c2 == '<') left = (double)((long long)left << (int)right); /* << */
                else if (len == 2 && c2 == '=') left = (left <= right);                     /* <= */
                else left = (left < right);
                break;
            case '>':
                if (len == 2 && c2 == '>') left = (double)((long long)left >> (int)right); /* >> */
                else if (len == 2 && c2 == '=') left = (left >= right);                     /* >= */
                else left = (left > right);
                break;
            case '=': left = (left == right); break;
            case '!': left = (left != right); break;
            case '&':
                if (len == 2) left = (left != 0 && right != 0) ? 1.0 : 0.0;
                else          left = (double)((long long)left & (long long)right);
                break;
            case '|':
                if (len == 2) left = (left != 0 || right != 0) ? 1.0 : 0.0;
                else          left = (double)((long long)left | (long long)right);
                break;
            case '^': left = (double)((long long)left ^ (long long)right); break;
        }
    }
    return left;
}

/* ---------- public API --------------------------------------------------- */

/*
 * Initialise an env. Call this once before anything else.
 * All fields are zeroed, including dollar_vars (disabled by default).
 * Set env.dollar_vars = 1 after init to require $name syntax for variables.
 */
static inline void ueval_init(ueval_env *env) {
    memset(env, 0, sizeof(*env));
}

/*
 * Bind a variable name to a value.
 * Calling again with the same name updates the existing binding.
 * Returns 0 on success, -1 if the table is full.
 */
static inline int ueval_bind(ueval_env *env, const char *name, double value) {
    for (int i = 0; i < env->var_count; i++) {
        if (strncmp(env->vars[i].name, name, 31) == 0) {
            env->vars[i].value = value;
            return 0;
        }
    }
    if (env->var_count >= UEVAL_MAX_VARS) return -1;
    strncpy(env->vars[env->var_count].name, name, 31);
    env->vars[env->var_count].name[31] = '\0';
    env->vars[env->var_count].value = value;
    env->var_count++;
    return 0;
}

/*
 * Look up a bound variable by name and return a pointer to its stored value.
 *
 * The pointer is stable for the lifetime of the env. Use it to read or
 * update the variable directly without going through ueval_bind again:
 *
 *   ueval_bind(&env, "vel", 0.0);
 *   double *vel = ueval_ptr(&env, "vel");
 *   // later, in a MIDI callback or tight loop:
 *   *vel = incoming_value;
 *
 * Returns NULL if name is not found.
 */
static inline double *ueval_ptr(ueval_env *env, const char *name) {
    for (int i = 0; i < env->var_count; i++) {
        if (strncmp(env->vars[i].name, name, 31) == 0)
            return &env->vars[i].value;
    }
    return NULL;
}

/*
 * Bind a single-argument C function.
 * Calling again with the same name replaces the binding.
 * Returns 0 on success, -1 if the table is full.
 */
static inline int ueval_bind_f1(ueval_env *env, const char *name, ueval_func1 f) {
    for (int i = 0; i < env->func_count; i++) {
        if (strncmp(env->funcs[i].name, name, 31) == 0) {
            env->funcs[i].f1 = f; env->funcs[i].args = 1; return 0;
        }
    }
    if (env->func_count >= UEVAL_MAX_FUNCS) return -1;
    strncpy(env->funcs[env->func_count].name, name, 31);
    env->funcs[env->func_count].name[31] = '\0';
    env->funcs[env->func_count].f1   = f;
    env->funcs[env->func_count].args = 1;
    env->func_count++;
    return 0;
}

/*
 * Bind a two-argument C function.
 * Calling again with the same name replaces the binding.
 * Returns 0 on success, -1 if the table is full.
 */
static inline int ueval_bind_f2(ueval_env *env, const char *name, ueval_func2 f) {
    for (int i = 0; i < env->func_count; i++) {
        if (strncmp(env->funcs[i].name, name, 31) == 0) {
            env->funcs[i].f2 = f; env->funcs[i].args = 2; return 0;
        }
    }
    if (env->func_count >= UEVAL_MAX_FUNCS) return -1;
    strncpy(env->funcs[env->func_count].name, name, 31);
    env->funcs[env->func_count].name[31] = '\0';
    env->funcs[env->func_count].f2   = f;
    env->funcs[env->func_count].args = 2;
    env->func_count++;
    return 0;
}

/*
 * Evaluate an expression string.
 * The returned ueval_result contains .value and .status.
 * On error, .error_msg holds a human-readable description.
 * The env state is reset on every call; it is safe to reuse.
 */
static inline ueval_result ueval_eval(ueval_env *env, const char *expr) {
    env->src   = expr;
    env->err   = UEVAL_OK;
    env->depth = 0;
    memset(env->err_detail, 0, sizeof(env->err_detail));

    ueval_result res;
    res.value  = ueval__expr(env, 0);
    res.status = env->err;

    if (env->err == UEVAL_OK) {
        res.error_msg[0] = '\0';
    } else {
        if (env->err_detail[0]) {
            snprintf(res.error_msg, sizeof(res.error_msg),
                     "%s: '%s'", ueval__status_str(env->err), env->err_detail);
        } else {
            snprintf(res.error_msg, sizeof(res.error_msg),
                     "%s", ueval__status_str(env->err));
        }
    }
    return res;
}

#endif /* UEVAL_H */
