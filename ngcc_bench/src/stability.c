#include "stability.h"

#include <linux/perf_event.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#include "bench_hash.h"
#include "bench_kem.h"
#include "bench_kex.h"
#include "bench_sig.h"
#include "mem_stat.h"

static volatile sig_atomic_t g_stop_requested = 0;

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

static void on_signal(int signo) {
    (void) signo;
    g_stop_requested = 1;
}

static int install_signal_handlers(struct sigaction *old_int, struct sigaction *old_term) {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;

    if (sigaction(SIGINT, &sa, old_int) != 0) {
        return -1;
    }
    if (sigaction(SIGTERM, &sa, old_term) != 0) {
        sigaction(SIGINT, old_int, NULL);
        return -1;
    }

    return 0;
}

static void restore_signal_handlers(const struct sigaction *old_int, const struct sigaction *old_term) {
    sigaction(SIGINT, old_int, NULL);
    sigaction(SIGTERM, old_term, NULL);
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
    return sqrt(stats->m2 / (double) (stats->count - 1U));
}

static int run_correctness_once(const ngcc_api_t *api,
                                ngcc_test_kind_t test_kind,
                                int digest_len_bits,
                                size_t msg_len) {
    switch (test_kind) {
        case NGCC_TEST_HASH:
            return ngcc_hash_correctness(api, digest_len_bits, msg_len);
        case NGCC_TEST_SIG:
            return ngcc_sig_correctness(api, msg_len);
        case NGCC_TEST_KEM:
            return ngcc_kem_correctness(api);
        case NGCC_TEST_KEX:
            return ngcc_kex_correctness(api);
        default:
            return -1;
    }
}

static void append_reason(char *dst, size_t cap, const char *reason) {
    size_t cur = strlen(dst);
    size_t n;
    if (cur >= cap - 1U) {
        return;
    }
    n = strlen(reason);
    if (n > cap - 1U - cur) {
        n = cap - 1U - cur;
    }
    memcpy(dst + cur, reason, n);
    dst[cur + n] = '\0';
}

void ngcc_stability_thresholds_set_defaults(ngcc_stability_thresholds_t *out_thresholds) {
    if (out_thresholds == NULL) {
        return;
    }

    out_thresholds->stable_throughput_cv_percent = 5.0;
    out_thresholds->stable_cycles_cv_percent = 5.0;
    out_thresholds->stable_time_cv_percent = 5.0;
    out_thresholds->stable_memory_growth_percent = 1.0;
    out_thresholds->stable_error_rate_percent = 0.0;

    out_thresholds->warning_throughput_cv_percent = 10.0;
    out_thresholds->warning_cycles_cv_percent = 10.0;
    out_thresholds->warning_time_cv_percent = 10.0;
    out_thresholds->warning_memory_growth_percent = 5.0;
    out_thresholds->warning_error_rate_percent = 1.0;
}

int ngcc_run_stability(const ngcc_api_t *api,
                       ngcc_test_kind_t test_kind,
                       int digest_len_bits,
                       size_t msg_len,
                       int cycles_enabled,
                       double sample_target_ms,
                       double duration_hours,
                       unsigned long long max_cases,
                       const ngcc_stability_thresholds_t *thresholds,
                       ngcc_stability_result_t *out_result) {
    struct sigaction old_int;
    struct sigaction old_term;
    struct timespec ts_start;
    struct timespec ts_now;
    cycle_counter_t counter;
    running_stats_t throughput_stats;
    running_stats_t cycles_stats;
    running_stats_t time_stats;
    double max_seconds;
    unsigned long long cases_run = 0;
    unsigned long long total_executions = 0;
    unsigned long long error_count = 0;
    int loop_failed = 0;
    uint64_t memory_start;
    uint64_t memory_end;
    uint64_t memory_min;
    uint64_t memory_max;
    int cycles_warning_printed = 0;
    ngcc_stability_thresholds_t effective_thresholds;

    if (api == NULL || out_result == NULL || max_cases == 0 ||
        duration_hours <= 0.0 || sample_target_ms <= 0.0 || !isfinite(sample_target_ms)) {
        return -1;
    }

    memset(out_result, 0, sizeof(*out_result));
    g_stop_requested = 0;
    out_result->status[0] = '\0';
    out_result->failure_reasons[0] = '\0';
    ngcc_stability_thresholds_set_defaults(&effective_thresholds);
    if (thresholds != NULL) {
        effective_thresholds = *thresholds;
    }

    if (install_signal_handlers(&old_int, &old_term) != 0) {
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_start) != 0) {
        restore_signal_handlers(&old_int, &old_term);
        return -1;
    }

    if (cycle_counter_open(&counter, cycles_enabled) != 0) {
        counter.source = CYCLE_SOURCE_NONE;
        counter.perf_fd = -1;
        cycles_warning_printed = 1;
    }

    memory_start = ngcc_mem_current_rss_bytes();
    memory_min = memory_start;
    memory_max = memory_start;

    stats_init(&throughput_stats);
    stats_init(&cycles_stats);
    stats_init(&time_stats);
    max_seconds = duration_hours * 3600.0;

    while (1) {
        double elapsed;
        struct timespec ts_batch_start;
        struct timespec ts_batch_end;
        double batch_ms;
        unsigned long long batch_cycle_start;
        unsigned long long batch_cycles;
        unsigned long long batch_ok = 0;
        int batch_failed = 0;
        uint64_t current_mem;

        if (g_stop_requested) {
            out_result->interrupted = 1;
            break;
        }

        if (cases_run >= max_cases) {
            break;
        }

        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_now) != 0) {
            loop_failed = 1;
            break;
        }

        elapsed = (double) (ts_now.tv_sec - ts_start.tv_sec) +
                  ((double) (ts_now.tv_nsec - ts_start.tv_nsec) / 1000000000.0);
        if (elapsed >= max_seconds) {
            break;
        }

        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_batch_start) != 0) {
            loop_failed = 1;
            break;
        }
        batch_cycle_start = cycle_counter_begin(&counter);

        while (1) {
            int case_rc = run_correctness_once(api, test_kind, digest_len_bits, msg_len);

            total_executions++;
            if (case_rc != 0) {
                error_count++;
                loop_failed = 1;
                batch_failed = 1;
                break;
            }
            batch_ok++;
            cases_run++;

            if (g_stop_requested || cases_run >= max_cases) {
                break;
            }

            if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_now) != 0) {
                loop_failed = 1;
                batch_failed = 1;
                break;
            }

            batch_ms = (double) (ts_now.tv_sec - ts_batch_start.tv_sec) * 1000.0 +
                       (double) (ts_now.tv_nsec - ts_batch_start.tv_nsec) / 1000000.0;
            if (batch_ms >= sample_target_ms) {
                break;
            }

            elapsed = (double) (ts_now.tv_sec - ts_start.tv_sec) +
                      ((double) (ts_now.tv_nsec - ts_start.tv_nsec) / 1000000000.0);
            if (elapsed >= max_seconds) {
                break;
            }
        }

        batch_cycles = cycle_counter_end(&counter, batch_cycle_start);
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_batch_end) != 0) {
            loop_failed = 1;
            break;
        }

        batch_ms = (double) (ts_batch_end.tv_sec - ts_batch_start.tv_sec) * 1000.0 +
                   (double) (ts_batch_end.tv_nsec - ts_batch_start.tv_nsec) / 1000000.0;

        if (batch_ok > 0) {
            double avg_case_ms = batch_ms / (double) batch_ok;
            stats_update(&time_stats, avg_case_ms);
            if (batch_ms > 0.0) {
                stats_update(&throughput_stats, ((double) batch_ok * 1000.0) / batch_ms);
            }
            if (counter.source != CYCLE_SOURCE_NONE) {
                stats_update(&cycles_stats, (double) batch_cycles / (double) batch_ok);
            }
        }

        current_mem = ngcc_mem_current_rss_bytes();
        if (current_mem < memory_min) {
            memory_min = current_mem;
        }
        if (current_mem > memory_max) {
            memory_max = current_mem;
        }

        if (batch_failed || loop_failed) {
            break;
        }

        if (g_stop_requested) {
            out_result->interrupted = 1;
            break;
        }
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_now) == 0) {
        out_result->elapsed_seconds = (double) (ts_now.tv_sec - ts_start.tv_sec) +
                                      ((double) (ts_now.tv_nsec - ts_start.tv_nsec) / 1000000000.0);
    }

    memory_end = ngcc_mem_current_rss_bytes();
    if (memory_end < memory_min) {
        memory_min = memory_end;
    }
    if (memory_end > memory_max) {
        memory_max = memory_end;
    }

    out_result->cases_run = cases_run;
    out_result->failed = loop_failed;
    out_result->total_executions = total_executions;
    out_result->error_count = error_count;
    if (total_executions > 0U) {
        out_result->error_rate_percent = (double) error_count * 100.0 / (double) total_executions;
    }

    out_result->time_mean_ms = time_stats.mean;
    out_result->time_stddev_ms = stats_stddev(&time_stats);
    out_result->time_min_ms = time_stats.min;
    out_result->time_max_ms = time_stats.max;
    if (time_stats.mean > 0.0) {
        out_result->time_cv_percent = out_result->time_stddev_ms * 100.0 / time_stats.mean;
    }

    out_result->throughput_mean_ops = throughput_stats.mean;
    out_result->throughput_stddev_ops = stats_stddev(&throughput_stats);
    out_result->throughput_min_ops = throughput_stats.min;
    out_result->throughput_max_ops = throughput_stats.max;
    if (throughput_stats.mean > 0.0) {
        out_result->throughput_cv_percent = out_result->throughput_stddev_ops * 100.0 / throughput_stats.mean;
    }

    out_result->cycles_available = (counter.source != CYCLE_SOURCE_NONE);
    if (out_result->cycles_available) {
        out_result->cycles_mean = cycles_stats.mean;
        out_result->cycles_stddev = stats_stddev(&cycles_stats);
        out_result->cycles_min = cycles_stats.min;
        out_result->cycles_max = cycles_stats.max;
        if (cycles_stats.mean > 0.0) {
            out_result->cycles_cv_percent = out_result->cycles_stddev * 100.0 / cycles_stats.mean;
        }
    }

    out_result->memory_start_bytes = memory_start;
    out_result->memory_end_bytes = memory_end;
    out_result->memory_min_bytes = memory_min;
    out_result->memory_max_bytes = memory_max;
    if (memory_start > 0U) {
        out_result->memory_growth_percent = ((double) ((long long) memory_end - (long long) memory_start) * 100.0) /
                                            (double) memory_start;
    }

    out_result->performance_stable = (out_result->throughput_cv_percent < effective_thresholds.stable_throughput_cv_percent &&
                                      out_result->time_cv_percent < effective_thresholds.stable_time_cv_percent &&
                                      (!out_result->cycles_available ||
                                       out_result->cycles_cv_percent < effective_thresholds.stable_cycles_cv_percent));
    out_result->memory_stable = (fabs(out_result->memory_growth_percent) < effective_thresholds.stable_memory_growth_percent);
    out_result->correctness_stable = (out_result->error_rate_percent <= effective_thresholds.stable_error_rate_percent);
    out_result->is_stable = (out_result->performance_stable &&
                             out_result->memory_stable &&
                             out_result->correctness_stable);

    if (out_result->is_stable) {
        strcpy(out_result->status, "STABLE");
    } else if (out_result->throughput_cv_percent < effective_thresholds.warning_throughput_cv_percent &&
               out_result->time_cv_percent < effective_thresholds.warning_time_cv_percent &&
               (!out_result->cycles_available ||
                out_result->cycles_cv_percent < effective_thresholds.warning_cycles_cv_percent) &&
               fabs(out_result->memory_growth_percent) < effective_thresholds.warning_memory_growth_percent &&
               out_result->error_rate_percent <= effective_thresholds.warning_error_rate_percent) {
        strcpy(out_result->status, "WARNING");
    } else {
        strcpy(out_result->status, "UNSTABLE");
    }

    if (!out_result->performance_stable) {
        append_reason(out_result->failure_reasons, sizeof(out_result->failure_reasons), "performance fluctuation; ");
    }
    if (!out_result->memory_stable) {
        append_reason(out_result->failure_reasons, sizeof(out_result->failure_reasons), "memory growth; ");
    }
    if (!out_result->correctness_stable) {
        append_reason(out_result->failure_reasons, sizeof(out_result->failure_reasons), "runtime errors; ");
    }
    if (cycles_warning_printed) {
        append_reason(out_result->failure_reasons, sizeof(out_result->failure_reasons), "cycle counter unavailable; ");
    }

    cycle_counter_close(&counter);
    restore_signal_handlers(&old_int, &old_term);
    return out_result->failed ? -1 : 0;
}
