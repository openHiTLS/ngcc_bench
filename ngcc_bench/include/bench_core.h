#ifndef BENCH_CORE_H
#define BENCH_CORE_H

#include <stdint.h>
#include <stddef.h>

typedef int (*ngcc_operation_fn)(void *ctx);

typedef struct {
    unsigned long long iterations;
    int cycles_enabled;
} ngcc_perf_config_t;

typedef struct {
    unsigned long long iterations;
    double elapsed_ms;
    double ops_per_sec;
    int cycles_available;
    double cycles_per_op;
} ngcc_perf_result_t;

int ngcc_fill_random(unsigned char *buf, size_t len);
int ngcc_run_performance_op(const ngcc_perf_config_t *cfg,
                            ngcc_operation_fn op,
                            void *op_ctx,
                            ngcc_perf_result_t *out_result);

#endif
