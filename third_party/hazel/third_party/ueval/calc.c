#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "ueval.h"
#include "uedit.h"

/*
 * Split "name = expr" into trimmed left/right halves in-place.
 * Returns 0 on success, -1 if the format is wrong.
 *
 * Rejects:
 *   - missing '='
 *   - more than one '=' (would confuse == comparisons)
 *   - either side being empty after trimming
 */
static int split_assign(char *str, char **name_out, char **expr_out) {
    if (!str || !name_out || !expr_out) return -1;

    char *eq = strchr(str, '=');
    if (!eq) return -1;
    if (strchr(eq + 1, '=')) return -1; /* reject == */

    *eq = '\0';
    char *name = str;
    char *expr = eq + 1;

    /* trim name (right side only — left side can't have leading spaces here) */
    char *end = name + strlen(name) - 1;
    while (end >= name && isspace((unsigned char)*end)) *end-- = '\0';

    /* trim expr (both sides) */
    while (isspace((unsigned char)*expr)) expr++;
    if (*expr) {
        end = expr + strlen(expr) - 1;
        while (end >= expr && isspace((unsigned char)*end)) *end-- = '\0';
    }

    if (*name == '\0' || *expr == '\0') return -1;

    *name_out = name;
    *expr_out = expr;
    return 0;
}

static void print_err(const char *context, ueval_result res) {
    if (res.error_msg[0]) fprintf(stderr, "# %s: %s\n", context, res.error_msg);
    else fprintf(stderr, "# %s: error %d\n", context, res.status);
}

/*
 * Detect a single, isolated '=' in a line that does NOT start with '\' —
 * i.e. someone almost certainly forgot the '\' prefix for assignment and
 * typed "x = 5" instead of "\x = 5". Returns 1 if such an '=' is found,
 * 0 otherwise.
 *
 * Deliberately conservative: only fires when there is EXACTLY one '=' in
 * the whole line and it is not adjacent to another '=', '!', '<', or '>'
 * (which would make it part of ==, !=, <=, or >=). This avoids false
 * positives on legitimate comparison expressions like "x == 5" or
 * "a <= b" while still catching the most common typo.
 */
static int looks_like_forgotten_backslash(const char *line) {
    const char *eq = strchr(line, '=');
    if (!eq) return 0;
    if (strchr(eq + 1, '=')) return 0;          /* more than one '=' -> not this case */
    if (eq != line && (eq[-1] == '=' || eq[-1] == '!' || eq[-1] == '<' || eq[-1] == '>'))
        return 0;                                /* part of ==, !=, <=, >= */
    return 1;
}

int main(void) {
    ueval_env env;
    ueval_init(&env);

    /* bind common math functions */
    ueval_bind_f1(&env, "sin",  sin);
    ueval_bind_f1(&env, "cos",  cos);
    ueval_bind_f1(&env, "tan",  tan);
    ueval_bind_f1(&env, "sqrt", sqrt);
    ueval_bind_f1(&env, "log",  log);
    ueval_bind_f1(&env, "log2", log2);
    ueval_bind_f1(&env, "abs",  fabs);
    ueval_bind_f1(&env, "exp",  exp);
    ueval_bind_f1(&env, "ceil", ceil);
    ueval_bind_f1(&env, "floor",floor);
    ueval_bind_f2(&env, "pow",  pow);
    ueval_bind_f2(&env, "fmod", fmod);
    ueval_bind(&env, "pi", 3.14159265358979323846);
    ueval_bind(&env, "e",  2.71828182845904523536);

    printf("ueval calc %s -- type an expression, \\name = expr to assign, q to quit\n",
           UEVAL_VERSION);

    char line[1024];

    while (1) {
        int r = uedit("# ", line, sizeof(line));
        if (r < 0) {
            puts("");
            break;
        }

        /* skip blank lines */
        if (line[0] == '\0') continue;

        if (strcmp(line, "exit") == 0 || strcmp(line, "q") == 0) break;

        /*
         * Assignment:  \name = expr
         * Evaluates expr, stores result under name for future expressions.
         */
        if (line[0] == '\\') {
            char *name, *expr;
            if (split_assign(&line[1], &name, &expr) != 0) {
                fprintf(stderr, "# usage: \\name = expression\n");
                continue;
            }
            ueval_result res = ueval_eval(&env, expr);
            if (res.status == UEVAL_OK) {
                if (ueval_bind(&env, name, res.value) != 0)
                    fprintf(stderr, "# variable table full\n");
                else
                    printf("%s = %g\n", name, res.value);
            } else {
                print_err(name, res);
            }
            continue; /* don't fall through to expression evaluator */
        }

        /*
         * Likely typo: "x = 5" without the leading '\' for assignment.
         * ueval's expression grammar has no '=' operator, so this would
         * otherwise silently parse just the "x" part, evaluate to x's
         * current value, and drop " = 5" as unreported trailing garbage
         * (see README "Known Quirks") -- giving no sign anything went
         * wrong. Warn instead of evaluating in that case.
         */
        if (looks_like_forgotten_backslash(line)) {
            fprintf(stderr, "# did you mean: \\%s ?  (assignment needs a leading '\\')\n", line);
            memset(line, 0, sizeof(line));
            continue;
        }

        /*
         * Expression evaluation (only for non-empty input that uedit returned).
         */
        if (r > 0) {
            ueval_result res = ueval_eval(&env, line);
            if (res.status == UEVAL_OK)
                printf("= %g\n", res.value);
            else
                print_err(line, res);
        }

        memset(line, 0, sizeof(line));
    }

    return 0;
}
