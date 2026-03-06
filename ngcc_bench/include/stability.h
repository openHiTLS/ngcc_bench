#ifndef STABILITY_H
#define STABILITY_H

#include <stddef.h>
#include <stdint.h>

#include "ngcc_api.h"

#define NGCC_STATUS_LEN  16
#define NGCC_REASON_LEN 256

typedef enum {
    NGCC_TEST_HASH,
    NGCC_TEST_DSA,
    NGCC_TEST_DSA_KEYGEN,
    NGCC_TEST_DSA_SIG,
    NGCC_TEST_DSA_VERIFY,
    NGCC_TEST_KEM,
    NGCC_TEST_KEM_KEYGEN,
    NGCC_TEST_KEM_ENCAP,
    NGCC_TEST_KEM_DECAP,
    NGCC_TEST_KEX
} ngcc_test_kind_t;

typedef struct {
    unsigned long long cases_run;
    double elapsed_seconds;
    int interrupted;
    int failed;

    double throughput_mean_ops;
    double throughput_stddev_ops;
    double throughput_cv_percent;
    double throughput_min_ops;
    double throughput_max_ops;
    double throughput_mean_bytes;
    double throughput_stddev_bytes;
    double throughput_cv_percent_bytes;
    double throughput_min_bytes;
    double throughput_max_bytes;
    double bytes_per_case;

    int cycles_available;
    double cycles_mean;
    double cycles_stddev;
    double cycles_cv_percent;
    double cycles_min;
    double cycles_max;

    double time_mean_ms;
    double time_stddev_ms;
    double time_cv_percent;
    double time_min_ms;
    double time_max_ms;

    uint64_t memory_start_bytes;
    uint64_t memory_end_bytes;
    uint64_t memory_min_bytes;
    uint64_t memory_max_bytes;
    uint64_t memory_peak_rss_bytes;
    double memory_growth_percent;

    unsigned long long total_executions;
    unsigned long long error_count;
    double error_rate_percent;

    int performance_stable;
    int memory_stable;
    int correctness_stable;
    int is_stable;
    char status[NGCC_STATUS_LEN];
    char failure_reasons[NGCC_REASON_LEN];
} ngcc_stability_result_t;

typedef struct {
    double stable_throughput_cv_percent;
    double stable_cycles_cv_percent;
    double stable_time_cv_percent;
    double stable_memory_growth_percent;
    double stable_error_rate_percent;

    double warning_throughput_cv_percent;
    double warning_cycles_cv_percent;
    double warning_time_cv_percent;
    double warning_memory_growth_percent;
    double warning_error_rate_percent;
} ngcc_stability_thresholds_t;

void ngcc_stability_thresholds_set_defaults(ngcc_stability_thresholds_t *out_thresholds);

int ngcc_run_stability(const ngcc_api_t *api,
                       ngcc_test_kind_t test_kind,
                       int digest_len_bits,
                       size_t msg_len,
                       int cycles_enabled,
                       double sample_target_ms,
                       double duration_hours,
                       unsigned long long max_cases,
                       const ngcc_stability_thresholds_t *thresholds,
                       ngcc_stability_result_t *out_result);

#endif
