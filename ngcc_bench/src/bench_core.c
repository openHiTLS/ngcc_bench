#include "bench_core.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <stdio.h>
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
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
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

static int g_cycles_warning_printed = 0;

static double timespec_ms_diff(const struct timespec *start, const struct timespec *end) {
    long sec = end->tv_sec - start->tv_sec;
    long nsec = end->tv_nsec - start->tv_nsec;
    return (double) sec * 1000.0 + (double) nsec / 1000000.0;
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
    struct timespec ts_start;
    struct timespec ts_end;
    cycle_counter_t counter;
    unsigned long long cycle_begin = 0;
    unsigned long long cycle_total = 0;
    double elapsed_ms;

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

    if (cycle_counter_open(&counter, cfg->cycles_enabled) != 0) {
        if (!g_cycles_warning_printed) {
            fprintf(stderr, "warning: cycle counter unavailable, falling back to time-only metrics\n");
            g_cycles_warning_printed = 1;
        }
        counter.source = CYCLE_SOURCE_NONE;
        counter.perf_fd = -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_start) != 0) {
        cycle_counter_close(&counter);
        return -1;
    }

    cycle_begin = cycle_counter_begin(&counter);

    for (i = 0; i < cfg->iterations; ++i) {
        if (op(op_ctx) != 0) {
            cycle_counter_close(&counter);
            return -1;
        }
    }

    cycle_total = cycle_counter_end(&counter, cycle_begin);

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_end) != 0) {
        cycle_counter_close(&counter);
        return -1;
    }

    elapsed_ms = timespec_ms_diff(&ts_start, &ts_end);

    out_result->iterations = cfg->iterations;
    out_result->elapsed_ms = elapsed_ms;
    if (elapsed_ms > 0.0) {
        out_result->ops_per_sec = ((double) cfg->iterations * 1000.0) / elapsed_ms;
    }
    out_result->cycles_available = (counter.source != CYCLE_SOURCE_NONE);
    if (out_result->cycles_available) {
        out_result->cycles_per_op = (double) cycle_total / (double) cfg->iterations;
    }

    cycle_counter_close(&counter);
    return 0;
}
