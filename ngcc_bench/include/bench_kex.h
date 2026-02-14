#ifndef BENCH_KEX_H
#define BENCH_KEX_H

#include "bench_core.h"
#include "ngcc_api.h"

int ngcc_kex_correctness(const ngcc_api_t *api);
int ngcc_kex_performance(const ngcc_api_t *api,
                         const ngcc_perf_config_t *cfg,
                         ngcc_perf_result_t *out_result);

#endif
