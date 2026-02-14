#ifndef BENCH_KEM_H
#define BENCH_KEM_H

#include "bench_core.h"
#include "ngcc_api.h"

int ngcc_kem_correctness(const ngcc_api_t *api);
int ngcc_kem_performance(const ngcc_api_t *api,
                         const ngcc_perf_config_t *cfg,
                         ngcc_perf_result_t *out_result);

#endif
