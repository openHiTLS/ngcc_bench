#ifndef STABILITY_H
#define STABILITY_H

#include <stddef.h>

#include "ngcc_api.h"

typedef enum {
    NGCC_TEST_HASH,
    NGCC_TEST_SIG,
    NGCC_TEST_KEM,
    NGCC_TEST_KEX
} ngcc_test_kind_t;

typedef struct {
    unsigned long long cases_run;
    double elapsed_seconds;
    int interrupted;
    int failed;
} ngcc_stability_result_t;

int ngcc_run_stability(const ngcc_api_t *api,
                       ngcc_test_kind_t test_kind,
                       int digest_len_bits,
                       size_t msg_len,
                       double duration_hours,
                       unsigned long long max_cases,
                       ngcc_stability_result_t *out_result);

#endif
