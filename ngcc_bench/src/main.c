#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bench_hash.h"
#include "bench_kem.h"
#include "bench_kex.h"
#include "bench_sig.h"
#include "cli_parser.h"
#include "cli_types.h"
#include "interactive.h"
#include "json_report.h"
#include "loader.h"
#include "mem_stat.h"
#include "stability.h"

/* ── Shared test table ─────────────────────────────────────────── */

typedef int (*ngcc_kat_dispatch_fn)(const ngcc_api_t *api,
                                    int digest_len_bits,
                                    const char *kat_path,
                                    unsigned long long *out_total,
                                    unsigned long long *out_passed,
                                    unsigned long long *out_failed);

typedef int (*ngcc_performance_dispatch_fn)(const ngcc_api_t *api,
                                            int digest_len_bits,
                                            size_t msg_len,
                                            const ngcc_perf_config_t *cfg,
                                            ngcc_perf_result_t *out_result);

typedef struct {
    ngcc_correctness_dispatch_fn correctness_fn;
    ngcc_kat_dispatch_fn kat_fn;
    ngcc_performance_dispatch_fn performance_fn;
    ngcc_bytes_per_case_fn bytes_per_case_fn;
} test_dispatch_entry_t;

static int hash_correctness_dispatch(const ngcc_api_t *api, int digest_len_bits, size_t msg_len) {
    return ngcc_hash_correctness(api, digest_len_bits, msg_len);
}

static int sig_correctness_dispatch(const ngcc_api_t *api, int digest_len_bits, size_t msg_len) {
    (void) digest_len_bits;
    return ngcc_sig_correctness(api, msg_len);
}

static int kem_correctness_dispatch(const ngcc_api_t *api, int digest_len_bits, size_t msg_len) {
    (void) digest_len_bits;
    (void) msg_len;
    return ngcc_kem_correctness(api);
}

static int kex_correctness_dispatch(const ngcc_api_t *api, int digest_len_bits, size_t msg_len) {
    (void) digest_len_bits;
    (void) msg_len;
    return ngcc_kex_correctness(api);
}

static int hash_kat_dispatch(const ngcc_api_t *api,
                             int digest_len_bits,
                             const char *kat_path,
                             unsigned long long *out_total,
                             unsigned long long *out_passed,
                             unsigned long long *out_failed) {
    return ngcc_hash_correctness_kat_file(api, digest_len_bits, kat_path, out_total, out_passed, out_failed);
}

static int sig_verify_kat_dispatch(const ngcc_api_t *api,
                                   int digest_len_bits,
                                   const char *kat_path,
                                   unsigned long long *out_total,
                                   unsigned long long *out_passed,
                                   unsigned long long *out_failed) {
    (void) digest_len_bits;
    return ngcc_sig_verify_correctness_kat_file(api, kat_path, out_total, out_passed, out_failed);
}

static int kem_kat_dispatch(const ngcc_api_t *api,
                            int digest_len_bits,
                            const char *kat_path,
                            unsigned long long *out_total,
                            unsigned long long *out_passed,
                            unsigned long long *out_failed) {
    (void) digest_len_bits;
    return ngcc_kem_correctness_kat_file(api, kat_path, out_total, out_passed, out_failed);
}

static int kex_kat_dispatch(const ngcc_api_t *api,
                            int digest_len_bits,
                            const char *kat_path,
                            unsigned long long *out_total,
                            unsigned long long *out_passed,
                            unsigned long long *out_failed) {
    (void) digest_len_bits;
    return ngcc_kex_correctness_kat_file(api, kat_path, out_total, out_passed, out_failed);
}

static int hash_performance_dispatch(const ngcc_api_t *api,
                                     int digest_len_bits,
                                     size_t msg_len,
                                     const ngcc_perf_config_t *cfg,
                                     ngcc_perf_result_t *out_result) {
    return ngcc_hash_performance(api, digest_len_bits, msg_len, cfg, out_result);
}



static unsigned long long msg_len_bytes_per_case(const ngcc_api_t *api, size_t msg_len) {
    (void) api;
    return (unsigned long long) msg_len;
}

static unsigned long long kem_bytes_per_case(const ngcc_api_t *api, size_t msg_len) {
    (void) msg_len;
    if (api == NULL) {
        return 0;
    }
    return api->kem_get_ct_len_bytes();
}

static unsigned long long kex_bytes_per_case(const ngcc_api_t *api, size_t msg_len) {
    (void) msg_len;
    if (api == NULL) {
        return 0;
    }
    return api->kex_get_total_msg_len_bytes();
}

#define NGCC_TEST_DISPATCH_TABLE(X) \
    X(TEST_MASK_HASH, NGCC_TEST_HASH, "hash", hash_correctness_dispatch, hash_kat_dispatch, hash_performance_dispatch, msg_len_bytes_per_case) \
    X(TEST_MASK_SIG, NGCC_TEST_SIG, "sig", sig_correctness_dispatch, sig_verify_kat_dispatch, NULL, msg_len_bytes_per_case) \
    X(TEST_MASK_KEM, NGCC_TEST_KEM, "kem", kem_correctness_dispatch, kem_kat_dispatch, NULL, kem_bytes_per_case) \
    X(TEST_MASK_KEX, NGCC_TEST_KEX, "kex", kex_correctness_dispatch, kex_kat_dispatch, NULL, kex_bytes_per_case)

#define NGCC_TEST_ENTRY_ROW(mask, kind, name, correctness_fn, kat_fn, performance_fn, bytes_per_case_fn) \
    {mask, kind, name},
const test_entry_t k_tests[] = {
    NGCC_TEST_DISPATCH_TABLE(NGCC_TEST_ENTRY_ROW)
};
#undef NGCC_TEST_ENTRY_ROW

#define NGCC_TEST_DISPATCH_ROW(mask, kind, name, correctness_fn, kat_fn, performance_fn, bytes_per_case_fn) \
    {correctness_fn, kat_fn, performance_fn, bytes_per_case_fn},
static const test_dispatch_entry_t k_test_dispatch[] = {
    NGCC_TEST_DISPATCH_TABLE(NGCC_TEST_DISPATCH_ROW)
};
#undef NGCC_TEST_DISPATCH_ROW
#undef NGCC_TEST_DISPATCH_TABLE

_Static_assert((sizeof(k_tests) / sizeof(k_tests[0])) == NGCC_NUM_TESTS, "k_tests size mismatch");
_Static_assert((sizeof(k_test_dispatch) / sizeof(k_test_dispatch[0])) == NGCC_NUM_TESTS, "k_test_dispatch size mismatch");

/* ── Test dispatch functions ───────────────────────────────────── */

static int run_correctness_for_test(const ngcc_api_t *api,
                                    size_t test_index,
                                    const cli_options_t *opts,
                                    test_report_t *report) {
    const test_entry_t *test = &k_tests[test_index];
    const test_dispatch_entry_t *dispatch = &k_test_dispatch[test_index];
    int rc;

    if (opts->kat_path != NULL) {
        unsigned long long total = 0;
        unsigned long long passed = 0;
        unsigned long long failed = 0;
        int kat_rc = dispatch->kat_fn(api,
                                      opts->digest_len_bits,
                                      opts->kat_path,
                                      &total,
                                      &passed,
                                      &failed);

        if (kat_rc == 0 || kat_rc < 0) {
            printf("[%s][correctness] %s total=%llu passed=%llu failed=%llu source=kat\n",
                   test->name,
                   kat_rc == 0 ? "PASS" : "FAIL",
                   total,
                   passed,
                   failed);
            if (report != NULL) {
                report->correctness_status = (kat_rc == 0) ? STATUS_PASS : STATUS_FAIL;
                report->kat_used = 1;
                report->kat_total = total;
                report->kat_passed = passed;
                report->kat_failed = failed;
            }
            return kat_rc;
        }

        printf("[%s][correctness] KAT_NO_VECTOR fallback=random\n", test->name);
    }

    rc = dispatch->correctness_fn(api, opts->digest_len_bits, k_msg_lens[0]);
    printf("[%s][correctness] %s\n", test->name, rc == 0 ? "PASS" : "FAIL");
    if (report != NULL) {
        report->correctness_status = (rc == 0) ? STATUS_PASS : STATUS_FAIL;
    }
    return rc;
}

static void print_perf_result(const char *prefix, size_t msg_len,
                              const ngcc_perf_result_t *result, int is_hash) {
    if (is_hash) {
        printf("[%s][performance][%zuB] ops=%llu warmup=%llu elapsed_ms=%.3f bytes/op=%.0f\n",
               prefix, msg_len,
               result->iterations,
               result->warmup_iterations,
               result->elapsed_ms,
               result->bytes_per_op);
    } else {
        printf("[%s][performance] ops=%llu warmup=%llu elapsed_ms=%.3f\n",
               prefix,
               result->iterations,
               result->warmup_iterations,
               result->elapsed_ms);
    }
    if (result->cycles_available) {
        if (is_hash) {
            /* Hash: report cycles per byte */
            printf("[%s][performance][%zuB][cycles] min=%.3f mean=%.3f median=%.3f max=%.3f stddev=%.3f cv=%.3f%% per_byte=%.3f\n",
                   prefix, msg_len,
                   result->cycles_min,
                   result->cycles_per_op,
                   result->cycles_median,
                   result->cycles_max,
                   result->cycles_stddev,
                   result->cycles_cv_percent,
                   result->cycles_per_byte);
        } else {
            /* Pubkey: report cycles per op */
            printf("[%s][performance][cycles] min=%.3f mean=%.3f median=%.3f max=%.3f stddev=%.3f cv=%.3f%%\n",
                   prefix,
                   result->cycles_min,
                   result->cycles_per_op,
                   result->cycles_median,
                   result->cycles_max,
                   result->cycles_stddev,
                   result->cycles_cv_percent);
        }
    } else {
        if (is_hash) {
            printf("[%s][performance][%zuB][cycles] unavailable\n", prefix, msg_len);
        } else {
            printf("[%s][performance][cycles] unavailable\n", prefix);
        }
    }
    if (is_hash) {
        /* Hash: byte throughput (bytes/s) */
        printf("[%s][performance][%zuB][throughput] bytes/s=%.3f\n",
               prefix, msg_len,
               result->bytes_per_sec);
        printf("[%s][performance][%zuB][time] mean_ms=%.6f median_ms=%.6f stddev_ms=%.6f cv=%.3f%%\n",
               prefix, msg_len,
               result->time_ms_mean,
               result->time_ms_median,
               result->time_ms_stddev,
               result->time_ms_cv_percent);
    } else {
        /* Pubkey: operation throughput (ops/s) */
        printf("[%s][performance][throughput] ops/s=%.3f\n",
               prefix,
               result->ops_per_sec);
        printf("[%s][performance][time] mean_ms=%.6f median_ms=%.6f stddev_ms=%.6f cv=%.3f%%\n",
               prefix,
               result->time_ms_mean,
               result->time_ms_median,
               result->time_ms_stddev,
               result->time_ms_cv_percent);
    }
}

static int run_sig_sub_performance(const ngcc_api_t *api,
                                   const ngcc_perf_config_t *cfg,
                                   test_report_t *report) {
    ngcc_perf_result_t result;
    int any_fail = 0;
    int rc;
    int idx = 0;
    static const size_t sig_msg_len = 64;

    /* keygen */
    rc = ngcc_sig_keygen_performance(api, cfg, &result);
    if (rc != 0) {
        printf("[sig][keygen][performance] FAIL\n");
        any_fail = 1;
    } else {
        print_perf_result("sig][keygen", 0, &result, 0);
        if (report != NULL) {
            report->performance[idx] = result;
            report->performance_labels[idx] = "\u5bc6\u94a5\u751f\u6210";
            report->performance_labels_en[idx] = "keygen";
            idx++;
        }
    }

    /* sign */
    rc = ngcc_sig_sign_performance(api, sig_msg_len, cfg, &result);
    if (rc != 0) {
        printf("[sig][sign][performance][%zuB] FAIL\n", sig_msg_len);
        any_fail = 1;
    } else {
        print_perf_result("sig][sign", sig_msg_len, &result, 0);
        if (report != NULL) {
            report->performance[idx] = result;
            report->performance_labels[idx] = "\u7b7e\u540d";
            report->performance_labels_en[idx] = "sign";
            idx++;
        }
    }

    /* verify */
    rc = ngcc_sig_verify_performance(api, sig_msg_len, cfg, &result);
    if (rc != 0) {
        printf("[sig][verify][performance][%zuB] FAIL\n", sig_msg_len);
        any_fail = 1;
    } else {
        print_perf_result("sig][verify", sig_msg_len, &result, 0);
        if (report != NULL) {
            report->performance[idx] = result;
            report->performance_labels[idx] = "\u9a8c\u7b7e";
            report->performance_labels_en[idx] = "verify";
            idx++;
        }
    }

    if (report != NULL) {
        report->performance_count = idx;
    }
    return any_fail ? -1 : 0;
}

static int run_kem_sub_performance(const ngcc_api_t *api,
                                   const ngcc_perf_config_t *cfg,
                                   test_report_t *report) {
    static const char *const sub_names[] = {"kem][keygen", "kem][encap", "kem][decap"};
    static const char *const labels[] = {"\u5bc6\u94a5\u751f\u6210", "\u5c01\u88c5", "\u89e3\u5c01\u88c5"};
    static const char *const labels_en[] = {"keygen", "encap", "decap"};
    ngcc_perf_result_t result;
    int any_fail = 0;
    int idx = 0;
    size_t si;

    typedef int (*kem_sub_perf_fn)(const ngcc_api_t *, const ngcc_perf_config_t *, ngcc_perf_result_t *);
    kem_sub_perf_fn sub_fns[] = {
        ngcc_kem_keygen_performance,
        ngcc_kem_encap_performance,
        ngcc_kem_decap_performance
    };

    for (si = 0; si < 3; ++si) {
        int rc = sub_fns[si](api, cfg, &result);
        if (rc != 0) {
            printf("[%s][performance] FAIL\n", sub_names[si]);
            any_fail = 1;
            continue;
        }
        print_perf_result(sub_names[si], 0, &result, 0);
        if (report != NULL) {
            report->performance[idx] = result;
            report->performance_labels[idx] = labels[si];
            report->performance_labels_en[idx] = labels_en[si];
            idx++;
        }
    }
    if (report != NULL) {
        report->performance_count = idx;
    }
    return any_fail ? -1 : 0;
}

static int run_kex_sub_performance(const ngcc_api_t *api,
                                   const ngcc_perf_config_t *cfg,
                                   test_report_t *report) {
    ngcc_perf_result_t result_a, result_b;
    int rc = ngcc_kex_derive_ss_performance(api, cfg, &result_a, &result_b);
    if (rc != 0) {
        printf("[kex][performance] FAIL\n");
        return -1;
    }
    print_perf_result("kex][derive_ss_a", 0, &result_a, 0);
    print_perf_result("kex][derive_ss_b", 0, &result_b, 0);
    if (report != NULL) {
        report->performance[0] = result_a;
        report->performance_labels[0] = "\u5bc6\u94a5\u534f\u5546A";
        report->performance_labels_en[0] = "kex_derive_a";
        report->performance[1] = result_b;
        report->performance_labels[1] = "\u5bc6\u94a5\u534f\u5546B";
        report->performance_labels_en[1] = "kex_derive_b";
        report->performance_count = 2;
    }
    return 0;
}

static int run_performance_for_test(const ngcc_api_t *api,
                                    size_t test_index,
                                    const cli_options_t *opts,
                                    test_report_t *report) {
    const test_entry_t *test = &k_tests[test_index];
    const test_dispatch_entry_t *dispatch = &k_test_dispatch[test_index];
    ngcc_perf_config_t cfg;
    ngcc_perf_result_t result;
    int rc;
    size_t mi;
    int any_fail = 0;
    int digest_bits = 0;


    if (test->kind == NGCC_TEST_HASH) {
        digest_bits = opts->digest_len_bits;
    }

    cfg.iterations = NGCC_PERF_ITERATIONS;
    cfg.bytes_per_op = 0;

    /* For SIG, KEM, KEX: run sub-operation performance (no aggregate) */
    if (test->kind == NGCC_TEST_SIG) {
        rc = run_sig_sub_performance(api, &cfg, report);
        if (report != NULL) {
            report->performance_status = (rc != 0) ? STATUS_FAIL : STATUS_PASS;
        }
        return rc;
    }
    if (test->kind == NGCC_TEST_KEM) {
        rc = run_kem_sub_performance(api, &cfg, report);
        if (report != NULL) {
            report->performance_status = (rc != 0) ? STATUS_FAIL : STATUS_PASS;
        }
        return rc;
    }
    if (test->kind == NGCC_TEST_KEX) {
        rc = run_kex_sub_performance(api, &cfg, report);
        if (report != NULL) {
            report->performance_status = (rc != 0) ? STATUS_FAIL : STATUS_PASS;
        }
        return rc;
    }

    /* HASH only: run the aggregate performance function */
    for (mi = 0; mi < NGCC_NUM_MSG_LENS; ++mi) {
        size_t msg_len = k_msg_lens[mi];

        rc = dispatch->performance_fn(api, digest_bits, msg_len, &cfg, &result);

        if (rc != 0) {
            printf("[%s][performance][%zuB] FAIL\n", test->name, msg_len);
            any_fail = 1;
            continue;
        }

        print_perf_result(test->name, msg_len, &result, 1);

        if (report != NULL) {
            char *label = (char *) malloc(16);
            if (label != NULL) {
                snprintf(label, 16, "%zuB", msg_len);
            }
            report->performance[mi] = result;
            report->performance_labels[mi] = label;
            report->performance_labels_en[mi] = label;
            report->performance_count = (int) mi + 1;
            report->is_hash = 1;
        }
    }

    if (report != NULL) {
        report->performance_status = any_fail ? STATUS_FAIL : STATUS_PASS;
    }
    return any_fail ? -1 : 0;
}

static int run_stability_for_test(const ngcc_api_t *api,
                                  size_t test_index,
                                  const cli_options_t *opts,
                                  test_report_t *report) {
    const test_entry_t *test = &k_tests[test_index];
    const test_dispatch_entry_t *dispatch = &k_test_dispatch[test_index];
    ngcc_stability_result_t result;
    int rc;
    int unstable_fail = 0;

    memset(&result, 0, sizeof(result));
    rc = ngcc_run_stability(api,
                            dispatch->correctness_fn,
                            (test->kind == NGCC_TEST_HASH) ? dispatch->bytes_per_case_fn : NULL,
                            (test->kind == NGCC_TEST_HASH) ? opts->digest_len_bits : 0,
                            NGCC_STABILITY_MSG_LEN,
                            1,
                            opts->stability_sample_ms,
                            opts->duration_hours,
                            opts->stability_max_cases,
                            &opts->stability_thresholds,
                            &result);

    if (rc == 0) {
        const char *status = result.interrupted ? "STOPPED" : result.status;
        if (!result.interrupted && strcmp(result.status, "UNSTABLE") == 0) {
            unstable_fail = 1;
        }
        printf("[%s][stability] %s cases=%llu samples=%llu elapsed_s=%.3f\n",
               test->name,
               status,
               result.cases_run,
               result.sample_count,
               result.elapsed_seconds);
        printf("[%s][stability][throughput] mean=%.3f stddev=%.3f cv=%.3f%% min=%.3f max=%.3f\n",
               test->name,
               result.throughput_mean_ops,
               result.throughput_stddev_ops,
               result.throughput_cv_percent,
               result.throughput_min_ops,
               result.throughput_max_ops);
        if (result.bytes_per_case > 0.0) {
            printf("[%s][stability][throughput_bytes] mean=%.3f stddev=%.3f cv=%.3f%% min=%.3f max=%.3f bytes/case=%.3f\n",
                   test->name,
                   result.throughput_mean_bytes,
                   result.throughput_stddev_bytes,
                   result.throughput_cv_percent_bytes,
                   result.throughput_min_bytes,
                   result.throughput_max_bytes,
                   result.bytes_per_case);
        }
        if (result.cycles_available) {
            printf("[%s][stability][cycles] mean=%.3f stddev=%.3f cv=%.3f%% min=%.3f max=%.3f\n",
                   test->name,
                   result.cycles_mean,
                   result.cycles_stddev,
                   result.cycles_cv_percent,
                   result.cycles_min,
                   result.cycles_max);
        } else {
            printf("[%s][stability][cycles] unavailable\n", test->name);
        }
        printf("[%s][stability][time] mean_ms=%.6f stddev_ms=%.6f cv=%.3f%% min_ms=%.6f max_ms=%.6f\n",
               test->name,
               result.time_mean_ms,
               result.time_stddev_ms,
               result.time_cv_percent,
               result.time_min_ms,
               result.time_max_ms);
        printf("[%s][stability][memory] start=%llu end=%llu min=%llu max=%llu peak_rss=%llu growth=%.3f%%\n",
               test->name,
               (unsigned long long) result.memory_start_bytes,
               (unsigned long long) result.memory_end_bytes,
               (unsigned long long) result.memory_min_bytes,
               (unsigned long long) result.memory_max_bytes,
               (unsigned long long) result.memory_peak_rss_bytes,
               result.memory_growth_percent);
        printf("[%s][stability][errors] total=%llu failed=%llu rate=%.6f%% status=%s\n",
               test->name,
               result.total_executions,
               result.error_count,
               result.error_rate_percent,
               result.status);
        if (result.failure_reasons[0] != '\0') {
            printf("[%s][stability][reason] %s\n", test->name, result.failure_reasons);
        }
        if (report != NULL) {
            report->stability = result;
            if (result.interrupted) {
                report->stability_status = STATUS_STOPPED;
            } else if (unstable_fail) {
                report->stability_status = STATUS_FAIL;
            } else {
                report->stability_status = STATUS_PASS;
            }
        }
    } else {
        printf("[%s][stability] FAIL cases=%llu samples=%llu elapsed_s=%.3f\n",
               test->name,
               result.cases_run,
               result.sample_count,
               result.elapsed_seconds);
        if (report != NULL) {
            report->stability = result;
            report->stability_status = STATUS_FAIL;
        }
    }

    if (rc == 0 && unstable_fail) {
        return -1;
    }
    return rc;
}

static int run_memory_mode(const ngcc_api_t *api,
                           const cli_options_t *opts,
                           run_report_t *report) {
    ngcc_static_mem_t static_mem;
    size_t i;
    int failed = 0;

    /* static memory: ELF segment sizes of the algorithm library (shared) */
    if (ngcc_mem_analyze_static(opts->lib_path, &static_mem) == 0) {
        printf("[memory][static] text=%llu data=%llu bss=%llu rodata=%llu total=%llu\n",
               (unsigned long long) static_mem.text_size,
               (unsigned long long) static_mem.data_size,
               (unsigned long long) static_mem.bss_size,
               (unsigned long long) static_mem.rodata_size,
               (unsigned long long) static_mem.total);
    } else {
        printf("[memory][static] unavailable\n");
        memset(&static_mem, 0, sizeof(static_mem));
    }
    report->static_mem = static_mem;

    /* dynamic memory: measure heap per-algorithm */
    for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
        int digest_bits = 0;
        uint64_t heap_baseline;
        uint64_t heap_end;
        int rc;

        if ((opts->test_mask & k_tests[i].mask) == 0) {
            continue;
        }
        if (k_tests[i].kind == NGCC_TEST_HASH) {
            digest_bits = opts->digest_len_bits;
        }

        heap_baseline = ngcc_mem_heap_bytes();
        rc = k_test_dispatch[i].correctness_fn(api, digest_bits, k_msg_lens[0]);
        heap_end = ngcc_mem_heap_bytes();

        printf("[%s][memory][dynamic] heap_baseline=%llu heap_after=%llu heap_delta=%lld\n",
               k_tests[i].name,
               (unsigned long long) heap_baseline,
               (unsigned long long) heap_end,
               (long long) heap_end - (long long) heap_baseline);

        report->tests[i].heap_baseline_bytes = heap_baseline;
        report->tests[i].heap_peak_bytes = heap_end;
        report->tests[i].memory_status = (rc == 0) ? STATUS_PASS : STATUS_FAIL;

        if (rc != 0) {
            failed = 1;
        }
    }

    return failed ? -1 : 0;
}

/* ── Entry point ───────────────────────────────────────────────── */

int main(int argc, char **argv) {
    cli_options_t opts;
    ngcc_library_t lib;
    run_report_t report;
    size_t i;
    int failed = 0;
    char interactive_lib[NGCC_PATH_BUF_SIZE];
    char interactive_kat[NGCC_PATH_BUF_SIZE];
    char interactive_json[NGCC_PATH_BUF_SIZE];

    init_default_options(&opts);
    memset(&report, 0, sizeof(report));
    memset(interactive_lib, 0, sizeof(interactive_lib));
    memset(interactive_kat, 0, sizeof(interactive_kat));
    memset(interactive_json, 0, sizeof(interactive_json));

    for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
        report.tests[i].name = k_tests[i].name;
        report.tests[i].correctness_status = STATUS_SKIPPED;
        report.tests[i].performance_status = STATUS_SKIPPED;
        report.tests[i].stability_status = STATUS_SKIPPED;
        report.tests[i].memory_status = STATUS_SKIPPED;
    }

    if (argc == 1) {
        if (run_interactive_setup(&opts,
                                  interactive_lib,
                                  sizeof(interactive_lib),
                                  interactive_kat,
                                  sizeof(interactive_kat),
                                  interactive_json,
                                  sizeof(interactive_json)) != 0) {
            return 1;
        }
    } else {
        int parse_rc = parse_cli_options(argc, argv, &opts);
        if (parse_rc > 0) {
            return 0;
        }
        if (parse_rc < 0) {
            return 1;
        }
    }

    if (validate_options(&opts) != 0) {
        if (argc != 1) {
            print_usage(argv[0]);
        }
        return 1;
    }

    if (ngcc_load_library(opts.lib_path, opts.test_mask, &lib) != 0) {
        fprintf(stderr, "[ERROR][main] failed to load library: %s\n", opts.lib_path);
        return 1;
    }

    for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
        report.tests[i].selected = ((opts.test_mask & k_tests[i].mask) != 0);
    }

    if (opts.mode_mask & MODE_MASK_CORRECTNESS) {
        for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
            if ((opts.test_mask & k_tests[i].mask) == 0) {
                continue;
            }
            if (run_correctness_for_test(&lib.api, i, &opts, &report.tests[i]) != 0) {
                failed = 1;
            }
        }
    }

    if (opts.mode_mask & MODE_MASK_PERFORMANCE) {
        for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
            if ((opts.test_mask & k_tests[i].mask) == 0) {
                continue;
            }
            if (run_performance_for_test(&lib.api, i, &opts, &report.tests[i]) != 0) {
                failed = 1;
            }
        }
    }

    if (opts.mode_mask & MODE_MASK_MEMORY) {
        if (run_memory_mode(&lib.api, &opts, &report) != 0) {
            failed = 1;
        }
    }

    if (opts.mode_mask & MODE_MASK_STABILITY) {
        for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
            if ((opts.test_mask & k_tests[i].mask) == 0) {
                continue;
            }
            if (run_stability_for_test(&lib.api, i, &opts, &report.tests[i]) != 0) {
                failed = 1;
            }
        }
    }

    if (opts.json_out_path != NULL) {
        if (write_json_reports(&opts, &report, failed) != 0) {
            failed = 1;
        }
    }

    ngcc_unload_library(&lib);
    return failed ? 1 : 0;
}
