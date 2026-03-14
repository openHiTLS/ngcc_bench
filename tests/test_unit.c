/**
 * Unit tests for ngcc_bench core functions.
 *
 * Lightweight assert-based harness — no external dependencies.
 * Exit code 0 = all tests passed, non-zero = failure.
 */
#include <math.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bench_core.h"
#include "cli_parser.h"
#include "cli_types.h"
#include "json_report.h"
#include "loader.h"
#include "stability.h"
#include "stats_util.h"

#ifndef NGCC_UNIT_MOCK_NGCC
#define NGCC_UNIT_MOCK_NGCC ""
#endif

#ifndef NGCC_UNIT_MOCK_HASH_ONLY
#define NGCC_UNIT_MOCK_HASH_ONLY ""
#endif

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

static int read_text_file(const char *path, char **out_data) {
    FILE *fp;
    long size;
    char *data;

    if (path == NULL || out_data == NULL) {
        return -1;
    }

    fp = fopen(path, "rb");
    if (fp == NULL) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    data = (char *) malloc((size_t) size + 1U);
    if (data == NULL) {
        fclose(fp);
        return -1;
    }
    if (size > 0 && fread(data, 1, (size_t) size, fp) != (size_t) size) {
        fclose(fp);
        free(data);
        return -1;
    }
    fclose(fp);
    data[size] = '\0';
    *out_data = data;
    return 0;
}

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

static void test_monotonic_clock_gettime_valid(void) {
    struct timespec start;
    struct timespec end;

    TEST_ASSERT_INT_EQ(ngcc_monotonic_clock_gettime(&start), 0);
    TEST_ASSERT_INT_EQ(ngcc_monotonic_clock_gettime(&end), 0);
    TEST_ASSERT(timespec_ms_diff(&start, &end) >= 0.0);
}

static void test_monotonic_clock_gettime_null(void) {
    TEST_ASSERT_INT_EQ(ngcc_monotonic_clock_gettime(NULL), -1);
}

/* ── loader tests ─────────────────────────────────────────────── */

static void test_loader_hash_only_selected_symbols(void) {
    ngcc_library_t lib;

    TEST_ASSERT(NGCC_UNIT_MOCK_HASH_ONLY[0] != '\0');
    TEST_ASSERT_INT_EQ(ngcc_load_library(NGCC_UNIT_MOCK_HASH_ONLY, TEST_MASK_HASH, &lib), 0);
    TEST_ASSERT(lib.handle != NULL);
    TEST_ASSERT(lib.api.CryptHash != NULL);
    TEST_ASSERT(lib.api.sig_get_pk_len_bytes == NULL);
    TEST_ASSERT(lib.api.kem_get_pk_len_bytes == NULL);
    TEST_ASSERT(lib.api.kex_get_passes_num == NULL);
    ngcc_unload_library(&lib);
}

static void test_loader_hash_only_missing_required_symbols(void) {
    ngcc_library_t lib;

    TEST_ASSERT(NGCC_UNIT_MOCK_HASH_ONLY[0] != '\0');
    TEST_ASSERT_INT_EQ(ngcc_load_library(NGCC_UNIT_MOCK_HASH_ONLY, TEST_MASK_SIG, &lib), -1);
    TEST_ASSERT(lib.handle == NULL);
    TEST_ASSERT(lib.api.CryptHash == NULL);
}

static void test_loader_selected_groups_only_loaded(void) {
    ngcc_library_t lib;

    TEST_ASSERT(NGCC_UNIT_MOCK_NGCC[0] != '\0');
    TEST_ASSERT_INT_EQ(ngcc_load_library(NGCC_UNIT_MOCK_NGCC, TEST_MASK_HASH, &lib), 0);
    TEST_ASSERT(lib.handle != NULL);
    TEST_ASSERT(lib.api.CryptHash != NULL);
    TEST_ASSERT(lib.api.sig_keygen == NULL);
    TEST_ASSERT(lib.api.kem_keygen == NULL);
    TEST_ASSERT(lib.api.kex_init_a == NULL);
    ngcc_unload_library(&lib);
}

/* ── parse_test_mask tests ────────────────────────────────────── */

static void test_parse_test_mask_valid(void) {
    unsigned int mask = 0;

    TEST_ASSERT_INT_EQ(parse_test_mask("hash", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_HASH);

    TEST_ASSERT_INT_EQ(parse_test_mask("sig", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_SIG);

    TEST_ASSERT_INT_EQ(parse_test_mask("kem", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_KEM);

    TEST_ASSERT_INT_EQ(parse_test_mask("kex", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_KEX);

    TEST_ASSERT_INT_EQ(parse_test_mask("all", &mask), 0);
    TEST_ASSERT(mask == TEST_MASK_ALL);
}

static void test_parse_test_mask_invalid(void) {
    unsigned int mask = 0;
    TEST_ASSERT_INT_EQ(parse_test_mask("invalid", &mask), -1);
    TEST_ASSERT_INT_EQ(parse_test_mask("", &mask), -1);
    TEST_ASSERT_INT_EQ(parse_test_mask("dsa", &mask), -1);
    TEST_ASSERT_INT_EQ(parse_test_mask("sig-keygen", &mask), -1);
    TEST_ASSERT_INT_EQ(parse_test_mask("sig-sign", &mask), -1);
    TEST_ASSERT_INT_EQ(parse_test_mask("sig-verify", &mask), -1);
    TEST_ASSERT_INT_EQ(parse_test_mask("kem-keygen", &mask), -1);
    TEST_ASSERT_INT_EQ(parse_test_mask("kem-encap", &mask), -1);
    TEST_ASSERT_INT_EQ(parse_test_mask("kem-decap", &mask), -1);
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

/* ── json_report tests ─────────────────────────────────────────── */

static void test_write_json_report_basic(void) {
    static const char *const k_test_names[NGCC_NUM_TESTS] = {
        "hash", "sig", "kem", "kex"
    };
    char tmp_dir[] = "/tmp/ngcc_unit_json.XXXXXX";
    char json_path[PATH_MAX];
    cli_options_t opts;
    run_report_t report;
    char *json_data = NULL;
    size_t i;

    TEST_ASSERT(mkdtemp(tmp_dir) != NULL);
    TEST_ASSERT(snprintf(json_path, sizeof(json_path), "%s/report.json", tmp_dir) < (int) sizeof(json_path));

    init_default_options(&opts);
    memset(&report, 0, sizeof(report));
    opts.lib_path = "/tmp/mock_lib.so";
    opts.json_out_path = json_path;
    opts.kat_path = "/tmp/vectors.kat";
    opts.cycles_enabled = 0;

    for (i = 0; i < NGCC_NUM_TESTS; ++i) {
        report.tests[i].name = k_test_names[i];
        report.tests[i].correctness_status = STATUS_SKIPPED;
        report.tests[i].performance_status = STATUS_SKIPPED;
        report.tests[i].stability_status = STATUS_SKIPPED;
    }

    report.tests[0].selected = 1;
    report.tests[0].correctness_status = STATUS_PASS;
    report.tests[0].performance_status = STATUS_PASS;
    report.tests[0].stability_status = STATUS_PASS;
    report.tests[0].kat_used = 1;
    report.tests[0].kat_total = 3;
    report.tests[0].kat_passed = 3;
    report.tests[0].kat_failed = 0;
    report.tests[0].performance.iterations = 8;
    report.tests[0].performance.warmup_iterations = 10;
    report.tests[0].performance.elapsed_ms = 1.5;
    report.tests[0].performance.bytes_per_op = 64.0;
    report.tests[0].performance.ops_per_sec = 1000.0;
    report.tests[0].performance.bytes_per_sec = 64000.0;
    report.tests[0].performance.time_ms_mean = 0.2;
    report.tests[0].performance.time_ms_median = 0.2;
    report.tests[0].performance.time_ms_stddev = 0.01;
    report.tests[0].performance.time_ms_cv_percent = 5.0;
    report.tests[0].stability.status[0] = 'W';
    strcpy(report.tests[0].stability.status, "WARNING");

    report.memory_status = STATUS_PASS;
    report.static_mem.text_size = 10;
    report.static_mem.data_size = 20;
    report.static_mem.bss_size = 30;
    report.static_mem.rodata_size = 40;
    report.static_mem.total = 100;
    report.memory_heap_baseline_bytes = 111;
    report.memory_heap_peak_bytes = 222;

    TEST_ASSERT_INT_EQ(write_json_report(&opts, &report, 1), 0);
    TEST_ASSERT_INT_EQ(read_text_file(json_path, &json_data), 0);
    TEST_ASSERT(strstr(json_data, "\"schema_version\": 4") != NULL);
    TEST_ASSERT(strstr(json_data, "\"library\": \"/tmp/mock_lib.so\"") != NULL);
    TEST_ASSERT(strstr(json_data, "\"report_metadata\"") != NULL);
    TEST_ASSERT(strstr(json_data, "\"generator\": \"ngcc_bench\"") != NULL);
    TEST_ASSERT(strstr(json_data, "\"json_out_path\": \"") != NULL);
    TEST_ASSERT(strstr(json_data, "\"environment\"") != NULL);
    TEST_ASSERT(strstr(json_data, "\"hostname\":") != NULL);
    TEST_ASSERT(strstr(json_data, "\"cwd\":") != NULL);
    TEST_ASSERT(strstr(json_data, "\"cycles\": \"off\"") != NULL);
    TEST_ASSERT(strstr(json_data, "\"stability\": \"WARNING\"") != NULL);
    TEST_ASSERT(strstr(json_data, "\"total\": 3") != NULL);
    TEST_ASSERT(strstr(json_data, "\"overall\": {\n    \"status\": \"FAIL\"") != NULL);

    free(json_data);
    TEST_ASSERT(unlink(json_path) == 0);
    TEST_ASSERT(rmdir(tmp_dir) == 0);
}

/* ── stability runner tests ───────────────────────────────────── */

static int stability_stub_success(const ngcc_api_t *api, int digest_len_bits, size_t msg_len) {
    (void) api;
    (void) digest_len_bits;
    (void) msg_len;
    return 0;
}

static int stability_stub_fail(const ngcc_api_t *api, int digest_len_bits, size_t msg_len) {
    (void) api;
    (void) digest_len_bits;
    (void) msg_len;
    return -1;
}

static unsigned long long stability_stub_bytes(const ngcc_api_t *api, size_t msg_len) {
    (void) api;
    return (unsigned long long) msg_len;
}

static void test_run_stability_single_success(void) {
    ngcc_api_t api;
    ngcc_stability_result_t result;
    ngcc_stability_thresholds_t thresholds;

    memset(&api, 0, sizeof(api));
    memset(&result, 0, sizeof(result));
    ngcc_stability_thresholds_set_defaults(&thresholds);
    thresholds.stable_throughput_cv_percent = 100.0;
    thresholds.stable_time_cv_percent = 100.0;
    thresholds.stable_memory_growth_percent = 100.0;
    thresholds.warning_throughput_cv_percent = 200.0;
    thresholds.warning_time_cv_percent = 200.0;
    thresholds.warning_memory_growth_percent = 200.0;
    TEST_ASSERT_INT_EQ(ngcc_run_stability(&api,
                                          stability_stub_success,
                                          stability_stub_bytes,
                                          0,
                                          64,
                                          0,
                                          1.0,
                                          0.0001,
                                          1,
                                          &thresholds,
                                          &result),
                       0);
    TEST_ASSERT(result.cases_run == 1);
    TEST_ASSERT(result.total_executions == 1);
    TEST_ASSERT(result.error_count == 0);
    TEST_ASSERT_DOUBLE_NEAR(result.bytes_per_case, 64.0, 1e-9);
    TEST_ASSERT(strcmp(result.status, "STABLE") == 0);
}

static void test_run_stability_single_failure(void) {
    ngcc_api_t api;
    ngcc_stability_result_t result;

    memset(&api, 0, sizeof(api));
    memset(&result, 0, sizeof(result));
    TEST_ASSERT_INT_EQ(ngcc_run_stability(&api,
                                          stability_stub_fail,
                                          stability_stub_bytes,
                                          0,
                                          64,
                                          0,
                                          1.0,
                                          0.0001,
                                          1,
                                          NULL,
                                          &result),
                       -1);
    TEST_ASSERT(result.cases_run == 0);
    TEST_ASSERT(result.total_executions == 1);
    TEST_ASSERT(result.error_count == 1);
    TEST_ASSERT(result.failed);
    TEST_ASSERT(strstr(result.failure_reasons, "runtime errors;") != NULL);
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
    RUN_TEST(test_monotonic_clock_gettime_valid);
    RUN_TEST(test_monotonic_clock_gettime_null);

    /* loader */
    RUN_TEST(test_loader_hash_only_selected_symbols);
    RUN_TEST(test_loader_hash_only_missing_required_symbols);
    RUN_TEST(test_loader_selected_groups_only_loaded);

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

    /* json_report */
    RUN_TEST(test_write_json_report_basic);

    /* stability runner */
    RUN_TEST(test_run_stability_single_success);
    RUN_TEST(test_run_stability_single_failure);

    printf("\n%d/%d tests passed\n", g_tests_run - g_tests_failed, g_tests_run);

    return g_tests_failed > 0 ? 1 : 0;
}
