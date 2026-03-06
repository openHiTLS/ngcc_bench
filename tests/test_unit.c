/**
 * Unit tests for ngcc_bench core functions.
 *
 * Lightweight assert-based harness — no external dependencies.
 * Exit code 0 = all tests passed, non-zero = failure.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bench_core.h"
#include "cli_parser.h"
#include "cli_types.h"
#include "stability.h"
#include "stats_util.h"

/* ── Minimal test harness ─────────────────────────────────────── */

static int g_tests_run = 0;
static int g_tests_failed = 0;

#define TEST_ASSERT(expr)                                                \
    do {                                                                 \
        if (!(expr)) {                                                   \
            fprintf(stderr, "  FAIL  %s:%d: %s\n", __FILE__, __LINE__,  \
                    #expr);                                              \
            g_tests_failed++;                                            \
            return;                                                      \
        }                                                                \
    } while (0)

#define TEST_ASSERT_INT_EQ(a, b)                                         \
    do {                                                                 \
        int _a = (a), _b = (b);                                          \
        if (_a != _b) {                                                  \
            fprintf(stderr, "  FAIL  %s:%d: %d != %d\n", __FILE__,       \
                    __LINE__, _a, _b);                                    \
            g_tests_failed++;                                            \
            return;                                                      \
        }                                                                \
    } while (0)

#define TEST_ASSERT_DOUBLE_NEAR(a, b, tol)                               \
    do {                                                                 \
        double _a = (a), _b = (b);                                       \
        if (fabs(_a - _b) > (tol)) {                                     \
            fprintf(stderr, "  FAIL  %s:%d: %.10f != %.10f (tol %.10f)\n",\
                    __FILE__, __LINE__, _a, _b, (tol));                  \
            g_tests_failed++;                                            \
            return;                                                      \
        }                                                                \
    } while (0)

#define RUN_TEST(fn)                                                     \
    do {                                                                 \
        int _before = g_tests_failed;                                    \
        g_tests_run++;                                                   \
        fn();                                                            \
        if (g_tests_failed == _before) {                                 \
            printf("  PASS  %s\n", #fn);                                 \
        }                                                                \
    } while (0)

/* ── stats_util tests ─────────────────────────────────────────── */

static void test_stats_single_value(void) {
    running_stats_t s;
    stats_init(&s);
    stats_update(&s, 42.0);

    TEST_ASSERT(s.count == 1);
    TEST_ASSERT_DOUBLE_NEAR(s.mean, 42.0, 1e-9);
    TEST_ASSERT_DOUBLE_NEAR(s.min, 42.0, 1e-9);
    TEST_ASSERT_DOUBLE_NEAR(s.max, 42.0, 1e-9);
    TEST_ASSERT_DOUBLE_NEAR(stats_stddev(&s), 0.0, 1e-9);
}

static void test_stats_known_dataset(void) {
    /* dataset: {2, 4, 4, 4, 5, 5, 7, 9}
     * mean = 5.0, population variance = 4.0, sample variance = 32/7 ≈ 4.571
     * sample stddev = sqrt(32/7) ≈ 2.138 */
    running_stats_t s;
    double data[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    size_t i;

    stats_init(&s);
    for (i = 0; i < sizeof(data) / sizeof(data[0]); ++i) {
        stats_update(&s, data[i]);
    }

    TEST_ASSERT(s.count == 8);
    TEST_ASSERT_DOUBLE_NEAR(s.mean, 5.0, 1e-9);
    TEST_ASSERT_DOUBLE_NEAR(s.min, 2.0, 1e-9);
    TEST_ASSERT_DOUBLE_NEAR(s.max, 9.0, 1e-9);
    TEST_ASSERT_DOUBLE_NEAR(stats_stddev(&s), sqrt(32.0 / 7.0), 1e-6);
}

static void test_stats_stddev_zero_count(void) {
    running_stats_t s;
    stats_init(&s);
    TEST_ASSERT_DOUBLE_NEAR(stats_stddev(&s), 0.0, 1e-9);
}

/* ── timespec_ms_diff tests ───────────────────────────────────── */

static void test_timespec_ms_diff_basic(void) {
    struct timespec start = {1, 0};
    struct timespec end = {2, 500000000L}; /* 1.5 seconds later */
    double diff = timespec_ms_diff(&start, &end);
    TEST_ASSERT_DOUBLE_NEAR(diff, 1500.0, 1e-3);
}

static void test_timespec_ms_diff_zero(void) {
    struct timespec t = {5, 100000000L};
    double diff = timespec_ms_diff(&t, &t);
    TEST_ASSERT_DOUBLE_NEAR(diff, 0.0, 1e-9);
}

static void test_timespec_ms_diff_subsecond(void) {
    struct timespec start = {0, 0};
    struct timespec end = {0, 1000000L}; /* 1 ms */
    double diff = timespec_ms_diff(&start, &end);
    TEST_ASSERT_DOUBLE_NEAR(diff, 1.0, 1e-6);
}

/* ── parse_test_mask tests ────────────────────────────────────── */

static void test_parse_test_mask_valid(void) {
    unsigned int mask = 0;

    TEST_ASSERT_INT_EQ(parse_test_mask("hash", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_HASH);

    TEST_ASSERT_INT_EQ(parse_test_mask("dsa", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_DSA);

    TEST_ASSERT_INT_EQ(parse_test_mask("dsa-keygen", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_DSA_KEYGEN);

    TEST_ASSERT_INT_EQ(parse_test_mask("dsa-sig", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_DSA_SIG);

    TEST_ASSERT_INT_EQ(parse_test_mask("dsa-verify", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_DSA_VERIFY);

    TEST_ASSERT_INT_EQ(parse_test_mask("kem", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_KEM);

    TEST_ASSERT_INT_EQ(parse_test_mask("kem-keygen", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_KEM_KEYGEN);

    TEST_ASSERT_INT_EQ(parse_test_mask("kem-encap", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_KEM_ENCAP);

    TEST_ASSERT_INT_EQ(parse_test_mask("kem-decap", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_KEM_DECAP);

    TEST_ASSERT_INT_EQ(parse_test_mask("kex", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_KEX);

    TEST_ASSERT_INT_EQ(parse_test_mask("all", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_ALL);
}

static void test_parse_test_mask_invalid(void) {
    unsigned int mask = 0;
    TEST_ASSERT_INT_EQ(parse_test_mask("invalid", &mask), -1);
    TEST_ASSERT_INT_EQ(parse_test_mask("", &mask), -1);
    TEST_ASSERT_INT_EQ(parse_test_mask("sig", &mask), -1);
    TEST_ASSERT_INT_EQ(parse_test_mask("HASH", &mask), -1); /* case-sensitive */
}

/* ── parse_mode_mask tests ────────────────────────────────────── */

static void test_parse_mode_mask_valid(void) {
    unsigned int mask = 0;

    TEST_ASSERT_INT_EQ(parse_mode_mask("correctness", &mask), 0);
    TEST_ASSERT(mask == MODE_MASK_CORRECTNESS);

    TEST_ASSERT_INT_EQ(parse_mode_mask("performance", &mask), 0);
    TEST_ASSERT(mask == MODE_MASK_PERFORMANCE);

    TEST_ASSERT_INT_EQ(parse_mode_mask("memory", &mask), 0);
    TEST_ASSERT(mask == MODE_MASK_MEMORY);

    TEST_ASSERT_INT_EQ(parse_mode_mask("stability", &mask), 0);
    TEST_ASSERT(mask == MODE_MASK_STABILITY);

    TEST_ASSERT_INT_EQ(parse_mode_mask("all", &mask), 0);
    TEST_ASSERT(mask == MODE_MASK_ALL);
}

static void test_parse_mode_mask_invalid(void) {
    unsigned int mask = 0;
    TEST_ASSERT_INT_EQ(parse_mode_mask("wrong", &mask), -1);
    TEST_ASSERT_INT_EQ(parse_mode_mask("", &mask), -1);
}

/* ── parse_unsigned_ll tests ──────────────────────────────────── */

static void test_parse_unsigned_ll_valid(void) {
    unsigned long long val = 0;

    TEST_ASSERT_INT_EQ(parse_unsigned_ll("0", &val), 0);
    TEST_ASSERT(val == 0);

    TEST_ASSERT_INT_EQ(parse_unsigned_ll("12345", &val), 0);
    TEST_ASSERT(val == 12345);

    TEST_ASSERT_INT_EQ(parse_unsigned_ll("18446744073709551615", &val), 0);
    TEST_ASSERT(val == 18446744073709551615ULL);
}

static void test_parse_unsigned_ll_invalid(void) {
    unsigned long long val = 0;
    TEST_ASSERT_INT_EQ(parse_unsigned_ll(NULL, &val), -1);
    TEST_ASSERT_INT_EQ(parse_unsigned_ll("", &val), -1);
    TEST_ASSERT_INT_EQ(parse_unsigned_ll("abc", &val), -1);
    TEST_ASSERT_INT_EQ(parse_unsigned_ll("123abc", &val), -1);
}

/* ── parse_int_value tests ────────────────────────────────────── */

static void test_parse_int_value_valid(void) {
    int val = 0;

    TEST_ASSERT_INT_EQ(parse_int_value("0", &val), 0);
    TEST_ASSERT_INT_EQ(val, 0);

    TEST_ASSERT_INT_EQ(parse_int_value("256", &val), 0);
    TEST_ASSERT_INT_EQ(val, 256);

    TEST_ASSERT_INT_EQ(parse_int_value("-10", &val), 0);
    TEST_ASSERT_INT_EQ(val, -10);
}

static void test_parse_int_value_invalid(void) {
    int val = 0;
    TEST_ASSERT_INT_EQ(parse_int_value(NULL, &val), -1);
    TEST_ASSERT_INT_EQ(parse_int_value("", &val), -1);
    TEST_ASSERT_INT_EQ(parse_int_value("xyz", &val), -1);
}

/* ── parse_double_value tests ─────────────────────────────────── */

static void test_parse_double_value_valid(void) {
    double val = 0.0;

    TEST_ASSERT_INT_EQ(parse_double_value("3.14", &val), 0);
    TEST_ASSERT_DOUBLE_NEAR(val, 3.14, 1e-9);

    TEST_ASSERT_INT_EQ(parse_double_value("-1.5", &val), 0);
    TEST_ASSERT_DOUBLE_NEAR(val, -1.5, 1e-9);

    TEST_ASSERT_INT_EQ(parse_double_value("0", &val), 0);
    TEST_ASSERT_DOUBLE_NEAR(val, 0.0, 1e-9);
}

static void test_parse_double_value_invalid(void) {
    double val = 0.0;
    TEST_ASSERT_INT_EQ(parse_double_value(NULL, &val), -1);
    TEST_ASSERT_INT_EQ(parse_double_value("", &val), -1);
    TEST_ASSERT_INT_EQ(parse_double_value("abc", &val), -1);
}

/* ── ngcc_fill_random tests ───────────────────────────────────── */

static void test_fill_random_nonzero(void) {
    unsigned char buf[64];
    int all_zero = 1;
    size_t i;

    memset(buf, 0, sizeof(buf));
    TEST_ASSERT_INT_EQ(ngcc_fill_random(buf, sizeof(buf)), 0);

    for (i = 0; i < sizeof(buf); ++i) {
        if (buf[i] != 0) {
            all_zero = 0;
            break;
        }
    }
    TEST_ASSERT(!all_zero);
}

static void test_fill_random_unique(void) {
    unsigned char buf_a[32];
    unsigned char buf_b[32];

    TEST_ASSERT_INT_EQ(ngcc_fill_random(buf_a, sizeof(buf_a)), 0);
    TEST_ASSERT_INT_EQ(ngcc_fill_random(buf_b, sizeof(buf_b)), 0);
    TEST_ASSERT(memcmp(buf_a, buf_b, sizeof(buf_a)) != 0);
}

static void test_fill_random_null(void) {
    TEST_ASSERT_INT_EQ(ngcc_fill_random(NULL, 16), -1);
}

/* ── ngcc_is_valid_len tests ──────────────────────────────────── */

static void test_is_valid_len(void) {
    TEST_ASSERT(!ngcc_is_valid_len(0));
    TEST_ASSERT(ngcc_is_valid_len(1));
    TEST_ASSERT(ngcc_is_valid_len(NGCC_MAX_BUFFER_LEN));
    TEST_ASSERT(!ngcc_is_valid_len(NGCC_MAX_BUFFER_LEN + 1));
}

/* ── stability thresholds defaults tests ──────────────────────── */

static void test_stability_thresholds_defaults(void) {
    ngcc_stability_thresholds_t t;
    memset(&t, 0, sizeof(t));
    ngcc_stability_thresholds_set_defaults(&t);

    TEST_ASSERT(t.stable_throughput_cv_percent > 0.0);
    TEST_ASSERT(t.stable_cycles_cv_percent > 0.0);
    TEST_ASSERT(t.stable_time_cv_percent > 0.0);
    TEST_ASSERT(t.stable_memory_growth_percent > 0.0);
    TEST_ASSERT(t.stable_error_rate_percent >= 0.0);
    /* warning thresholds >= stable thresholds */
    TEST_ASSERT(t.warning_throughput_cv_percent >= t.stable_throughput_cv_percent);
    TEST_ASSERT(t.warning_cycles_cv_percent >= t.stable_cycles_cv_percent);
    TEST_ASSERT(t.warning_time_cv_percent >= t.stable_time_cv_percent);
    TEST_ASSERT(t.warning_memory_growth_percent >= t.stable_memory_growth_percent);
    TEST_ASSERT(t.warning_error_rate_percent >= t.stable_error_rate_percent);
}

/* ── main ─────────────────────────────────────────────────────── */

int main(void) {
    printf("=== ngcc_bench unit tests ===\n");

    /* stats_util */
    RUN_TEST(test_stats_single_value);
    RUN_TEST(test_stats_known_dataset);
    RUN_TEST(test_stats_stddev_zero_count);

    /* timespec */
    RUN_TEST(test_timespec_ms_diff_basic);
    RUN_TEST(test_timespec_ms_diff_zero);
    RUN_TEST(test_timespec_ms_diff_subsecond);

    /* parse_test_mask */
    RUN_TEST(test_parse_test_mask_valid);
    RUN_TEST(test_parse_test_mask_invalid);

    /* parse_mode_mask */
    RUN_TEST(test_parse_mode_mask_valid);
    RUN_TEST(test_parse_mode_mask_invalid);

    /* parse_unsigned_ll */
    RUN_TEST(test_parse_unsigned_ll_valid);
    RUN_TEST(test_parse_unsigned_ll_invalid);

    /* parse_int_value */
    RUN_TEST(test_parse_int_value_valid);
    RUN_TEST(test_parse_int_value_invalid);

    /* parse_double_value */
    RUN_TEST(test_parse_double_value_valid);
    RUN_TEST(test_parse_double_value_invalid);

    /* ngcc_fill_random */
    RUN_TEST(test_fill_random_nonzero);
    RUN_TEST(test_fill_random_unique);
    RUN_TEST(test_fill_random_null);

    /* ngcc_is_valid_len */
    RUN_TEST(test_is_valid_len);

    /* stability thresholds defaults */
    RUN_TEST(test_stability_thresholds_defaults);

    printf("\n%d/%d tests passed\n", g_tests_run - g_tests_failed, g_tests_run);

    return g_tests_failed > 0 ? 1 : 0;
}
