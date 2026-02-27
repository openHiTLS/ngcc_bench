#include <stdio.h>
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

const test_entry_t k_tests[] = {
    {TEST_MASK_HASH, NGCC_TEST_HASH, "hash"},
    {TEST_MASK_SIG, NGCC_TEST_SIG, "sig"},
    {TEST_MASK_KEM, NGCC_TEST_KEM, "kem"},
    {TEST_MASK_KEX, NGCC_TEST_KEX, "kex"}
};

/* ── Test dispatch functions ───────────────────────────────────── */

static int run_correctness_for_test(const ngcc_api_t *api,
                                    ngcc_test_kind_t kind,
                                    const char *name,
                                    const cli_options_t *opts,
                                    test_report_t *report) {
    int rc;

    if (opts->kat_path != NULL) {
        unsigned long long total = 0;
        unsigned long long passed = 0;
        unsigned long long failed = 0;
        int kat_rc = 1;

        switch (kind) {
            case NGCC_TEST_HASH:
                kat_rc = ngcc_hash_correctness_kat_file(api,
                                                        opts->digest_len_bits,
                                                        opts->kat_path,
                                                        &total,
                                                        &passed,
                                                        &failed);
                break;
            case NGCC_TEST_SIG:
                kat_rc = ngcc_sig_correctness_kat_file(api,
                                                       opts->kat_path,
                                                       &total,
                                                       &passed,
                                                       &failed);
                break;
            case NGCC_TEST_KEM:
                kat_rc = ngcc_kem_correctness_kat_file(api,
                                                       opts->kat_path,
                                                       &total,
                                                       &passed,
                                                       &failed);
                break;
            case NGCC_TEST_KEX:
                kat_rc = ngcc_kex_correctness_kat_file(api,
                                                       opts->kat_path,
                                                       &total,
                                                       &passed,
                                                       &failed);
                break;
            default:
                kat_rc = -1;
                break;
        }

        if (kat_rc == 0 || kat_rc < 0) {
            printf("[%s][correctness] %s total=%llu passed=%llu failed=%llu source=kat\n",
                   name,
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

        printf("[%s][correctness] KAT_NO_VECTOR fallback=random\n", name);
    }

    switch (kind) {
        case NGCC_TEST_HASH:
            rc = ngcc_hash_correctness(api, opts->digest_len_bits, opts->msg_len);
            break;
        case NGCC_TEST_SIG:
            rc = ngcc_sig_correctness(api, opts->msg_len);
            break;
        case NGCC_TEST_KEM:
            rc = ngcc_kem_correctness(api);
            break;
        case NGCC_TEST_KEX:
            rc = ngcc_kex_correctness(api);
            break;
        default:
            rc = -1;
            break;
    }

    printf("[%s][correctness] %s\n", name, rc == 0 ? "PASS" : "FAIL");
    if (report != NULL) {
        report->correctness_status = (rc == 0) ? STATUS_PASS : STATUS_FAIL;
    }
    return rc;
}

static int run_performance_for_test(const ngcc_api_t *api,
                                    ngcc_test_kind_t kind,
                                    const char *name,
                                    const cli_options_t *opts,
                                    test_report_t *report) {
    ngcc_perf_config_t cfg;
    ngcc_perf_result_t result;
    int rc;

    cfg.iterations = opts->iterations;
    cfg.cycles_enabled = opts->cycles_enabled;
    cfg.bytes_per_op = 0;

    switch (kind) {
        case NGCC_TEST_HASH:
            rc = ngcc_hash_performance(api, opts->digest_len_bits, opts->msg_len, &cfg, &result);
            break;
        case NGCC_TEST_SIG:
            rc = ngcc_sig_performance(api, opts->msg_len, &cfg, &result);
            break;
        case NGCC_TEST_KEM:
            rc = ngcc_kem_performance(api, &cfg, &result);
            break;
        case NGCC_TEST_KEX:
            rc = ngcc_kex_performance(api, &cfg, &result);
            break;
        default:
            rc = -1;
            break;
    }

    if (rc != 0) {
        printf("[%s][performance] FAIL\n", name);
        if (report != NULL) {
            report->performance_status = STATUS_FAIL;
        }
        return rc;
    }

    /* basic config */
    printf("[%s][performance] ops=%llu warmup=%llu elapsed_ms=%.3f bytes/op=%.3f\n",
           name,
           result.iterations,
           result.warmup_iterations,
           result.elapsed_ms,
           result.bytes_per_op);
    /* cpu cycles */
    if (result.cycles_available) {
        printf("[%s][performance][cycles] min=%.3f mean=%.3f median=%.3f max=%.3f stddev=%.3f cv=%.3f%% per_byte=%.3f\n",
               name,
               result.cycles_min,
               result.cycles_per_op,
               result.cycles_median,
               result.cycles_max,
               result.cycles_stddev,
               result.cycles_cv_percent,
               result.cycles_per_byte);
    } else {
        printf("[%s][performance][cycles] unavailable\n", name);
    }
    /* throughput */
    printf("[%s][performance][throughput] ops/s=%.3f bytes/s=%.3f\n",
           name,
           result.ops_per_sec,
           result.bytes_per_sec);
    /* time */
    printf("[%s][performance][time] mean_ms=%.6f median_ms=%.6f stddev_ms=%.6f cv=%.3f%%\n",
           name,
           result.time_ms_mean,
           result.time_ms_median,
           result.time_ms_stddev,
           result.time_ms_cv_percent);

    if (report != NULL) {
        report->performance = result;
        report->performance_status = STATUS_PASS;
    }
    return 0;
}

static int run_stability_for_test(const ngcc_api_t *api,
                                  ngcc_test_kind_t kind,
                                  const char *name,
                                  const cli_options_t *opts,
                                  test_report_t *report) {
    ngcc_stability_result_t result;
    int rc;
    int unstable_fail = 0;

    rc = ngcc_run_stability(api,
                            kind,
                            opts->digest_len_bits,
                            opts->msg_len,
                            opts->cycles_enabled,
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
        printf("[%s][stability] %s cases=%llu elapsed_s=%.3f\n",
               name,
               status,
               result.cases_run,
               result.elapsed_seconds);
        printf("[%s][stability][throughput] mean=%.3f stddev=%.3f cv=%.3f%% min=%.3f max=%.3f\n",
               name,
               result.throughput_mean_ops,
               result.throughput_stddev_ops,
               result.throughput_cv_percent,
               result.throughput_min_ops,
               result.throughput_max_ops);
        printf("[%s][stability][throughput_bytes] mean=%.3f stddev=%.3f cv=%.3f%% min=%.3f max=%.3f bytes/case=%.3f\n",
               name,
               result.throughput_mean_bytes,
               result.throughput_stddev_bytes,
               result.throughput_cv_percent_bytes,
               result.throughput_min_bytes,
               result.throughput_max_bytes,
               result.bytes_per_case);
        if (result.cycles_available) {
            printf("[%s][stability][cycles] mean=%.3f stddev=%.3f cv=%.3f%% min=%.3f max=%.3f\n",
                   name,
                   result.cycles_mean,
                   result.cycles_stddev,
                   result.cycles_cv_percent,
                   result.cycles_min,
                   result.cycles_max);
        } else {
            printf("[%s][stability][cycles] unavailable\n", name);
        }
        printf("[%s][stability][time] mean_ms=%.6f stddev_ms=%.6f cv=%.3f%% min_ms=%.6f max_ms=%.6f\n",
               name,
               result.time_mean_ms,
               result.time_stddev_ms,
               result.time_cv_percent,
               result.time_min_ms,
               result.time_max_ms);
        printf("[%s][stability][memory] start=%llu end=%llu min=%llu max=%llu peak_rss=%llu growth=%.3f%%\n",
               name,
               (unsigned long long) result.memory_start_bytes,
               (unsigned long long) result.memory_end_bytes,
               (unsigned long long) result.memory_min_bytes,
               (unsigned long long) result.memory_max_bytes,
               (unsigned long long) result.memory_peak_rss_bytes,
               result.memory_growth_percent);
        printf("[%s][stability][errors] total=%llu failed=%llu rate=%.6f%% status=%s\n",
               name,
               result.total_executions,
               result.error_count,
               result.error_rate_percent,
               result.status);
        if (result.failure_reasons[0] != '\0') {
            printf("[%s][stability][reason] %s\n", name, result.failure_reasons);
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
        printf("[%s][stability] FAIL cases=%llu elapsed_s=%.3f\n",
               name,
               result.cases_run,
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
    uint64_t baseline_bytes;
    uint64_t peak_bytes;
    uint64_t heap_baseline;
    uint64_t heap_end;
    size_t i;
    int failed = 0;

    baseline_bytes = ngcc_mem_current_rss_bytes();
    heap_baseline = ngcc_mem_heap_bytes();

    for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
        if ((opts->test_mask & k_tests[i].mask) == 0) {
            continue;
        }
        if (run_correctness_for_test(api, k_tests[i].kind, k_tests[i].name, opts, NULL) != 0) {
            failed = 1;
        }
    }

    peak_bytes = ngcc_mem_peak_rss_bytes();
    heap_end = ngcc_mem_heap_bytes();
    printf("[memory] baseline_bytes=%llu peak_bytes=%llu heap_baseline=%llu heap_end=%llu heap_delta=%lld\n",
           (unsigned long long) baseline_bytes,
           (unsigned long long) peak_bytes,
           (unsigned long long) heap_baseline,
           (unsigned long long) heap_end,
           (long long) heap_end - (long long) heap_baseline);

    report->memory_baseline_bytes = baseline_bytes;
    report->memory_peak_bytes = peak_bytes;
    report->memory_heap_baseline_bytes = heap_baseline;
    report->memory_heap_peak_bytes = heap_end;
    report->memory_status = failed ? STATUS_FAIL : STATUS_PASS;
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
    }
    report.memory_status = STATUS_SKIPPED;

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
            if (run_correctness_for_test(&lib.api, k_tests[i].kind, k_tests[i].name, &opts, &report.tests[i]) != 0) {
                failed = 1;
            }
        }
    }

    if (opts.mode_mask & MODE_MASK_PERFORMANCE) {
        for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
            if ((opts.test_mask & k_tests[i].mask) == 0) {
                continue;
            }
            if (run_performance_for_test(&lib.api, k_tests[i].kind, k_tests[i].name, &opts, &report.tests[i]) != 0) {
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
            if (run_stability_for_test(&lib.api, k_tests[i].kind, k_tests[i].name, &opts, &report.tests[i]) != 0) {
                failed = 1;
            }
        }
    }

    if (opts.json_out_path != NULL) {
        if (write_json_report(&opts, &report, failed) != 0) {
            failed = 1;
        }
    }

    ngcc_unload_library(&lib);
    return failed ? 1 : 0;
}
