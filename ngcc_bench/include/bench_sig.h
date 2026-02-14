#ifndef BENCH_SIG_H
#define BENCH_SIG_H

#include <stddef.h>

#include "bench_core.h"
#include "ngcc_api.h"

int ngcc_sig_correctness(const ngcc_api_t *api, size_t msg_len);
int ngcc_sig_performance(const ngcc_api_t *api,
                         size_t msg_len,
                         const ngcc_perf_config_t *cfg,
                         ngcc_perf_result_t *out_result);

#endif
