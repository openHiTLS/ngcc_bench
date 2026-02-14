#include "stability.h"

#include <signal.h>
#include <string.h>
#include <time.h>

#include "bench_hash.h"
#include "bench_kem.h"
#include "bench_kex.h"
#include "bench_sig.h"

static volatile sig_atomic_t g_stop_requested = 0;

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

int ngcc_run_stability(const ngcc_api_t *api,
                       ngcc_test_kind_t test_kind,
                       int digest_len_bits,
                       size_t msg_len,
                       double duration_hours,
                       unsigned long long max_cases,
                       ngcc_stability_result_t *out_result) {
    struct sigaction old_int;
    struct sigaction old_term;
    struct timespec ts_start;
    struct timespec ts_now;
    double max_seconds;
    unsigned long long cases_run = 0;

    if (api == NULL || out_result == NULL || max_cases == 0 || duration_hours <= 0.0) {
        return -1;
    }

    memset(out_result, 0, sizeof(*out_result));
    g_stop_requested = 0;

    if (install_signal_handlers(&old_int, &old_term) != 0) {
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_start) != 0) {
        restore_signal_handlers(&old_int, &old_term);
        return -1;
    }

    max_seconds = duration_hours * 3600.0;

    while (1) {
        double elapsed;

        if (g_stop_requested) {
            out_result->interrupted = 1;
            break;
        }

        if (cases_run >= max_cases) {
            break;
        }

        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_now) != 0) {
            out_result->failed = 1;
            break;
        }

        elapsed = (double) (ts_now.tv_sec - ts_start.tv_sec) +
                  ((double) (ts_now.tv_nsec - ts_start.tv_nsec) / 1000000000.0);
        if (elapsed >= max_seconds) {
            break;
        }

        if (run_correctness_once(api, test_kind, digest_len_bits, msg_len) != 0) {
            out_result->failed = 1;
            break;
        }

        cases_run++;
    }

    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts_now) == 0) {
        out_result->elapsed_seconds = (double) (ts_now.tv_sec - ts_start.tv_sec) +
                                      ((double) (ts_now.tv_nsec - ts_start.tv_nsec) / 1000000000.0);
    }

    out_result->cases_run = cases_run;

    restore_signal_handlers(&old_int, &old_term);
    return out_result->failed ? -1 : 0;
}
