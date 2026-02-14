#ifndef BENCH_HASH_H
#define BENCH_HASH_H

#include <stddef.h>

#include "bench_core.h"
#include "ngcc_api.h"

int ngcc_hash_correctness(const ngcc_api_t *api, int digest_len_bits, size_t msg_len);
int ngcc_hash_performance(const ngcc_api_t *api,
                          int digest_len_bits,
                          size_t msg_len,
                          const ngcc_perf_config_t *cfg,
                          ngcc_perf_result_t *out_result);

#endif
