#ifndef BENCH_SIG_H
#define BENCH_SIG_H

#include <stddef.h>

#include "bench_core.h"
#include "ngcc_api.h"

int ngcc_dsa_correctness(const ngcc_api_t *api, size_t msg_len);
int ngcc_dsa_keygen_correctness(const ngcc_api_t *api);
int ngcc_dsa_sig_correctness(const ngcc_api_t *api, size_t msg_len);
int ngcc_dsa_verify_correctness(const ngcc_api_t *api, size_t msg_len);
int ngcc_dsa_verify_correctness_kat_file(const ngcc_api_t *api,
                                         const char *kat_path,
                                         unsigned long long *out_total,
                                         unsigned long long *out_passed,
                                         unsigned long long *out_failed);
int ngcc_dsa_performance(const ngcc_api_t *api,
                         size_t msg_len,
                         const ngcc_perf_config_t *cfg,
                         ngcc_perf_result_t *out_result);
int ngcc_dsa_keygen_performance(const ngcc_api_t *api,
                                const ngcc_perf_config_t *cfg,
                                ngcc_perf_result_t *out_result);
int ngcc_dsa_sig_performance(const ngcc_api_t *api,
                             size_t msg_len,
                             const ngcc_perf_config_t *cfg,
                             ngcc_perf_result_t *out_result);
int ngcc_dsa_verify_performance(const ngcc_api_t *api,
                                size_t msg_len,
                                const ngcc_perf_config_t *cfg,
                                ngcc_perf_result_t *out_result);

#endif
