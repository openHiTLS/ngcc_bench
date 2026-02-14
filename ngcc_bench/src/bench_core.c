#include "bench_core.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/random.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#if defined(__x86_64__)
static unsigned long long read_tsc(void) {
    unsigned int lo;
    unsigned int hi;
    __asm__ volatile ("lfence\n\t"
                      "rdtsc"
                      : "=a"(lo), "=d"(hi)
                      :
                      : "memory");
    return ((unsigned long long) hi << 32) | lo;
}
#endif

typedef enum {
    CYCLE_SOURCE_NONE = 0,
    CYCLE_SOURCE_PERF,
    CYCLE_SOURCE_TSC
} cycle_source_t;

typedef struct {
    cycle_source_t source;
    int perf_fd;
} cycle_counter_t;

typedef struct {
    unsigned long long count;
    double mean;
    double m2;
    double min;
    double max;
} running_stats_t;

static int g_cycles_warning_printed = 0;

static double timespec_ms_diff(const struct timespec *start, const struct timespec *end) {
    long sec = end->tv_sec - start->tv_sec;
    long nsec = end->tv_nsec - start->tv_nsec;
    return (double) sec * 1000.0 + (double) nsec / 1000000.0;
}

static void stats_init(running_stats_t *stats) {
    memset(stats, 0, sizeof(*stats));
}

static void stats_update(running_stats_t *stats, double value) {
    if (stats->count == 0) {
        stats->count = 1;
        stats->mean = value;
        stats->m2 = 0.0;
        stats->min = value;
        stats->max = value;
        return;
    }

    stats->count++;
    {
        double delta = value - stats->mean;
        stats->mean += delta / (double) stats->count;
        {
            double delta2 = value - stats->mean;
            stats->m2 += delta * delta2;
        }
    }

    if (value < stats->min) {
        stats->min = value;
    }
    if (value > stats->max) {
        stats->max = value;
    }
}

static double stats_stddev(const running_stats_t *stats) {
    if (stats->count < 2) {
        return 0.0;
    }
    return sqrt(stats->m2 / (double) (stats->count - 1));
}

static int compare_double(const void *a, const void *b) {
    double da = *(const double *) a;
    double db = *(const double *) b;
    if (da < db) {
        return -1;
    }
    if (da > db) {
        return 1;
    }
    return 0;
}

static double compute_median(double *values, size_t count) {
    if (values == NULL || count == 0) {
        return 0.0;
    }

    qsort(values, count, sizeof(values[0]), compare_double);
    if ((count % 2U) != 0U) {
        return values[count / 2U];
    }
    return (values[(count / 2U) - 1U] + values[count / 2U]) * 0.5;
}

static void cycle_counter_close(cycle_counter_t *counter) {
    if (counter->perf_fd >= 0) {
        close(counter->perf_fd);
    }
    counter->perf_fd = -1;
    counter->source = CYCLE_SOURCE_NONE;
}

static int cycle_counter_open(cycle_counter_t *counter, int cycles_enabled) {
    struct perf_event_attr pe;

    counter->source = CYCLE_SOURCE_NONE;
    counter->perf_fd = -1;

    if (!cycles_enabled) {
        return 0;
    }

    memset(&pe, 0, sizeof(pe));
    pe.type = PERF_TYPE_HARDWARE;
    pe.size = sizeof(pe);
    pe.config = PERF_COUNT_HW_CPU_CYCLES;
    pe.disabled = 1;
    pe.exclude_kernel = 0;
    pe.exclude_hv = 0;

    counter->perf_fd = (int) syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    if (counter->perf_fd >= 0) {
        counter->source = CYCLE_SOURCE_PERF;
        return 0;
    }

#if defined(__x86_64__)
    counter->source = CYCLE_SOURCE_TSC;
    return 0;
#else
    return -1;
#endif
}

static unsigned long long cycle_counter_begin(cycle_counter_t *counter) {
    if (counter->source == CYCLE_SOURCE_PERF) {
        ioctl(counter->perf_fd, PERF_EVENT_IOC_RESET, 0);
        ioctl(counter->perf_fd, PERF_EVENT_IOC_ENABLE, 0);
        return 0;
    }

#if defined(__x86_64__)
    if (counter->source == CYCLE_SOURCE_TSC) {
        return read_tsc();
    }
#endif

    return 0;
}

static unsigned long long cycle_counter_end(cycle_counter_t *counter, unsigned long long start_cycles) {
    unsigned long long cycles = 0;

    if (counter->source == CYCLE_SOURCE_PERF) {
        ioctl(counter->perf_fd, PERF_EVENT_IOC_DISABLE, 0);
        if (read(counter->perf_fd, &cycles, sizeof(cycles)) != (ssize_t) sizeof(cycles)) {
            return 0;
        }
        return cycles;
    }

#if defined(__x86_64__)
    if (counter->source == CYCLE_SOURCE_TSC) {
        unsigned long long end_cycles = read_tsc();
        return end_cycles - start_cycles;
    }
#endif

    return 0;
}

int ngcc_fill_random(unsigned char *buf, size_t len) {
    size_t offset = 0;

    if (buf == NULL) {
        return -1;
    }

    while (offset < len) {
        ssize_t got = getrandom(buf + offset, len - offset, 0);
        if (got > 0) {
            offset += (size_t) got;
            continue;
        }
        if (got < 0 && errno == EINTR) {
            continue;
        }
        break;
    }

    if (offset == len) {
        return 0;
    }

    {
        int fd = open("/dev/urandom", O_RDONLY);
        if (fd < 0) {
            return -1;
        }

        while (offset < len) {
            ssize_t got = read(fd, buf + offset, len - offset);
            if (got > 0) {
                offset += (size_t) got;
                continue;
            }
            if (got < 0 && errno == EINTR) {
                continue;
            }
            close(fd);
            return -1;
        }

        close(fd);
    }

    return 0;
}

int ngcc_run_performance_op(const ngcc_perf_config_t *cfg,
                            ngcc_operation_fn op,
                            void *op_ctx,
                            ngcc_perf_result_t *out_result) {
    unsigned long long i;
    unsigned long long warmup;
    struct timespec ts_total_start;
    struct timespec ts_total_end;
    struct timespec ts_iter_start;
    struct timespec ts_iter_end;
    cycle_counter_t counter;
    running_stats_t time_stats;
    running_stats_t cycle_stats;
    double *time_samples = NULL;
    double *cycle_samples = NULL;
    size_t sample_capacity = 0;
    int keep_time_samples = 0;
    int keep_cycle_samples = 0;

    if (cfg == NULL || op == NULL || out_result == NULL || cfg->iterations == 0) {
        return -1;
    }

    warmup = cfg->iterations / 100;
    if (warmup < 10) {
        warmup = 10;
    }

    for (i = 0; i < warmup; ++i) {
        if (op(op_ctx) != 0) {
            return -1;
        }
    }

    memset(out_result, 0, sizeof(*out_result));
    out_result->iterations = cfg->iterations;
    out_result->warmup_iterations = warmup;

    if (cycle_counter_open(&counter, cfg->cycles_enabled) != 0) {
        if (!g_cycles_warning_printed) {
            fprintf(stderr, "warning: cycle counter unavailable, falling back to time-only metrics\n");
            g_cycles_warning_printed = 1;
        }
        counter.source = CYCLE_SOURCE_NONE;
        counter.perf_fd = -1;
    }

    stats_init(&time_stats);
    stats_init(&cycle_stats);

    if (cfg->iterations <= (unsigned long long) SIZE_MAX) {
        sample_capacity = (size_t) cfg->iterations;
    }

    if (sample_capacity > 0) {
        time_samples = (double *) malloc(sample_capacity * sizeof(time_samples[0]));
        if (time_samples != NULL) {
            keep_time_samples = 1;
        }
        if (counter.source != CYCLE_SOURCE_NONE) {
            cycle_samples = (double *) malloc(sample_capacity * sizeof(cycle_samples[0]));
            if (cycle_samples != NULL) {
                keep_cycle_samples = 1;
            }
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_total_start) != 0) {
        free(time_samples);
        free(cycle_samples);
        cycle_counter_close(&counter);
        return -1;
    }

    for (i = 0; i < cfg->iterations; ++i) {
        unsigned long long iter_cycle_start;
        unsigned long long iter_cycles;
        double iter_time_ms;

        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_iter_start) != 0) {
            free(time_samples);
            free(cycle_samples);
            cycle_counter_close(&counter);
            return -1;
        }

        iter_cycle_start = cycle_counter_begin(&counter);
        if (op(op_ctx) != 0) {
            free(time_samples);
            free(cycle_samples);
            cycle_counter_close(&counter);
            return -1;
        }
        iter_cycles = cycle_counter_end(&counter, iter_cycle_start);

        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_iter_end) != 0) {
            free(time_samples);
            free(cycle_samples);
            cycle_counter_close(&counter);
            return -1;
        }

        iter_time_ms = timespec_ms_diff(&ts_iter_start, &ts_iter_end);
        stats_update(&time_stats, iter_time_ms);
        if (keep_time_samples) {
            time_samples[(size_t) i] = iter_time_ms;
        }

        if (counter.source != CYCLE_SOURCE_NONE) {
            double iter_cycle_value = (double) iter_cycles;
            stats_update(&cycle_stats, iter_cycle_value);
            if (keep_cycle_samples) {
                cycle_samples[(size_t) i] = iter_cycle_value;
            }
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_total_end) != 0) {
        free(time_samples);
        free(cycle_samples);
        cycle_counter_close(&counter);
        return -1;
    }

    out_result->elapsed_ms = timespec_ms_diff(&ts_total_start, &ts_total_end);
    if (out_result->elapsed_ms > 0.0) {
        out_result->ops_per_sec = ((double) cfg->iterations * 1000.0) / out_result->elapsed_ms;
    }

    out_result->cycles_available = (counter.source != CYCLE_SOURCE_NONE);
    out_result->time_ms_min = time_stats.min;
    out_result->time_ms_mean = time_stats.mean;
    out_result->time_ms_max = time_stats.max;
    out_result->time_ms_stddev = stats_stddev(&time_stats);
    if (time_stats.mean > 0.0) {
        out_result->time_ms_cv_percent = (out_result->time_ms_stddev / time_stats.mean) * 100.0;
    }
    out_result->time_ms_median = keep_time_samples ? compute_median(time_samples, sample_capacity) : time_stats.mean;

    if (out_result->cycles_available && cycle_stats.count > 0) {
        out_result->cycles_per_op = cycle_stats.mean;
        out_result->cycles_min = cycle_stats.min;
        out_result->cycles_max = cycle_stats.max;
        out_result->cycles_stddev = stats_stddev(&cycle_stats);
        if (cycle_stats.mean > 0.0) {
            out_result->cycles_cv_percent = (out_result->cycles_stddev / cycle_stats.mean) * 100.0;
        }
        out_result->cycles_median = keep_cycle_samples ? compute_median(cycle_samples, sample_capacity) : cycle_stats.mean;
    } else {
        out_result->cycles_per_op = 0.0;
        out_result->cycles_min = 0.0;
        out_result->cycles_max = 0.0;
        out_result->cycles_stddev = 0.0;
        out_result->cycles_cv_percent = 0.0;
        out_result->cycles_median = 0.0;
    }

    free(time_samples);
    free(cycle_samples);
    cycle_counter_close(&counter);
    return 0;
}
