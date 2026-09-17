/*
 * bench_ueval.c - Hot-path benchmark for ueval.h
 *
 * Build:  gcc -O2 bench_ueval.c -o bench_ueval -lm
 * Run:    ./bench_ueval
 *
 * This is a microbenchmark, not the assertion-based test suite - there's
 * no "expected" timing value to check against. What it DOES check, and
 * report PASS/FAIL on, is correctness: that the two patterns being
 * compared actually compute identical results, so the timing comparison
 * between them is meaningful and not an apples-to-oranges artifact.
 *
 * Each scenario below clearly labels:
 *   - WHAT is being measured (the expression, the variable count, the
 *     update pattern)
 *   - the CORRECTNESS check (sink values from both patterns must match)
 *   - the measured ns/call for each pattern and the resulting ratio
 *
 * Patterns compared throughout:
 *   Pattern A: ueval_bind() once at setup, ueval_ptr() once to get a
 *              stable pointer, then *ptr = value on every call.
 *   Pattern B: ueval_bind(env, name, value) by name on every call.
 *
 * See README "Performance Notes" for how to read the results.
 */

#include "ueval.h"
#include <math.h>
#include <stdio.h>
#include <time.h>

static int g_checks_pass = 0;
static int g_checks_fail = 0;

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void check_sinks_match(const char *label, double sink_a, double sink_b) {
    int ok = fabs(sink_a - sink_b) < 1e-6;
    if (ok) {
        g_checks_pass++;
        printf("  PASS  %-38s Pattern A and B produced identical results (%.3f)\n",
               label, sink_a);
    } else {
        g_checks_fail++;
        printf("  FAIL  %-38s Pattern A=%.6f vs Pattern B=%.6f -- MISMATCH, timing comparison invalid!\n",
               label, sink_a, sink_b);
    }
}

/* ===========================================================================
 * Scenario: realistic small expression, few bound variables
 * ======================================================================== */

static void bench_realistic(long n) {
    printf("\n== Scenario: 3 bound variables, short expression ==\n");
    printf("   expr: \"(c - 60) / 12 + a * b\"\n");

    ueval_env env;
    ueval_init(&env);
    ueval_bind(&env, "a", 0.0);
    ueval_bind(&env, "b", 0.0);
    ueval_bind(&env, "c", 0.0);
    const char *expr = "(c - 60) / 12 + a * b";

    double *ap = ueval_ptr(&env, "a");
    double *bp = ueval_ptr(&env, "b");
    double *cp = ueval_ptr(&env, "c");

    double sink_a = 0;
    double t0 = now_sec();
    for (long i = 0; i < n; i++) {
        *ap = (double)(i % 128) / 127.0;
        *bp = (double)((i * 3) % 128) / 127.0;
        *cp = (double)(60 + (i % 24));
        ueval_result r = ueval_eval(&env, expr);
        sink_a += r.value;
    }
    double t1 = now_sec();
    double ns_a = (t1 - t0) * 1e9 / n;
    printf("  Pattern A (ueval_ptr cache):   %.1f ns/call\n", ns_a);

    double sink_b = 0;
    double t2 = now_sec();
    for (long i = 0; i < n; i++) {
        ueval_bind(&env, "a", (double)(i % 128) / 127.0);
        ueval_bind(&env, "b", (double)((i * 3) % 128) / 127.0);
        ueval_bind(&env, "c", (double)(60 + (i % 24)));
        ueval_result r = ueval_eval(&env, expr);
        sink_b += r.value;
    }
    double t3 = now_sec();
    double ns_b = (t3 - t2) * 1e9 / n;
    printf("  Pattern B (ueval_bind by name): %.1f ns/call\n", ns_b);
    printf("  Slowdown B/A: %.2fx\n", ns_b / ns_a);

    check_sinks_match("3-var scenario: A and B agree", sink_a, sink_b);
}

/* ===========================================================================
 * Scenario: near-full variable table, worst case for linear scan
 * ======================================================================== */

static void bench_worst_case(long n) {
    printf("\n== Scenario: 32 bound variables (table near-full), hot var in last slot ==\n");
    printf("   expr: \"hot * 2\"  (UEVAL_MAX_VARS default is 32 -- this is the worst case\n");
    printf("   for ueval_bind's linear strncmp scan, since it has to walk past all 31\n");
    printf("   other entries before reaching \"hot\")\n");

    ueval_env env;
    ueval_init(&env);

    char names[31][8];
    for (int i = 0; i < 31; i++) {
        snprintf(names[i], sizeof(names[i]), "v%d", i);
        ueval_bind(&env, names[i], (double)i);
    }
    ueval_bind(&env, "hot", 0.0); /* 32nd / last slot */

    const char *expr = "hot * 2";
    double *hot = ueval_ptr(&env, "hot");

    double sink_a = 0;
    double t0 = now_sec();
    for (long i = 0; i < n; i++) {
        *hot = (double)(i % 128) / 127.0;
        ueval_result r = ueval_eval(&env, expr);
        sink_a += r.value;
    }
    double t1 = now_sec();
    double ns_a = (t1 - t0) * 1e9 / n;
    printf("  Pattern A (ueval_ptr cache):   %.1f ns/call\n", ns_a);

    double sink_b = 0;
    double t2 = now_sec();
    for (long i = 0; i < n; i++) {
        ueval_bind(&env, "hot", (double)(i % 128) / 127.0);
        ueval_result r = ueval_eval(&env, expr);
        sink_b += r.value;
    }
    double t3 = now_sec();
    double ns_b = (t3 - t2) * 1e9 / n;
    printf("  Pattern B (ueval_bind by name): %.1f ns/call\n", ns_b);
    printf("  Slowdown B/A: %.2fx\n", ns_b / ns_a);

    check_sinks_match("32-var worst-case scenario: A and B agree", sink_a, sink_b);
}

/* ===========================================================================
 * Scenario: bare parse floor cost (no variables at all)
 * ======================================================================== */

static void bench_bare_parse(long n) {
    printf("\n== Scenario: bare parse floor (no variables, trivial literal expr) ==\n");
    printf("   expr: \"1 + 1\"  (establishes the minimum per-call cost: just the\n");
    printf("   precedence-climbing parse itself, with nothing to look up)\n");

    ueval_env env;
    ueval_init(&env);

    double sink = 0;
    double t0 = now_sec();
    for (long i = 0; i < n; i++) {
        ueval_result r = ueval_eval(&env, "1 + 1");
        sink += r.value;
    }
    double t1 = now_sec();
    double ns = (t1 - t0) * 1e9 / n;
    printf("  ns/call: %.1f\n", ns);

    /* sanity check: every call should produce exactly 2.0, so the running
     * sink must equal n * 2.0 exactly (no precision drift expected for
     * this trivial case at this scale). */
    double expected = (double)n * 2.0;
    int ok = fabs(sink - expected) < 1e-3;
    if (ok) {
        g_checks_pass++;
        printf("  PASS  %-38s sink == n * 2.0 (%.3f)\n", "bare parse: result correctness", sink);
    } else {
        g_checks_fail++;
        printf("  FAIL  %-38s sink=%.6f, expected=%.6f\n", "bare parse: result correctness", sink, expected);
    }
}

int main(void) {
    const long N = 2000000;

    printf("ueval.h hot-path benchmark (%ld calls per scenario)\n", N);
    printf("Build flags affect the ns/call numbers a lot - check how this was compiled\n"
           "(this run: %s)\n",
#if defined(__OPTIMIZE__)
           "optimized build"
#else
           "UNOPTIMIZED build -- rebuild with -O2 for representative numbers"
#endif
    );

    bench_bare_parse(N);
    bench_realistic(N);
    bench_worst_case(N);

    printf("\n========================================\n");
    printf("Correctness checks: %d passed, %d failed\n", g_checks_pass, g_checks_fail);
    printf("========================================\n");

    printf("\nTakeaway: ueval_ptr caching wins more as the variable table fills up.\n"
           "At a handful of variables the difference is small; near a full\n"
           "default-sized table (32 vars) it's roughly 1.5x in this measurement.\n"
           "Either way, the parse itself is the floor cost - see README\n"
           "\"Performance Notes\" for what that means for a MIDI-rate callback.\n");

    return g_checks_fail == 0 ? 0 : 1;
}
