#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bench_hash.h"
#include "bench_kem.h"
#include "bench_kex.h"
#include "bench_sig.h"
#include "loader.h"
#include "mem_stat.h"
#include "stability.h"

#define TEST_MASK_HASH (1U << 0)
#define TEST_MASK_SIG  (1U << 1)
#define TEST_MASK_KEM  (1U << 2)
#define TEST_MASK_KEX  (1U << 3)
#define TEST_MASK_ALL  (TEST_MASK_HASH | TEST_MASK_SIG | TEST_MASK_KEM | TEST_MASK_KEX)

#define MODE_MASK_CORRECTNESS (1U << 0)
#define MODE_MASK_PERFORMANCE (1U << 1)
#define MODE_MASK_MEMORY      (1U << 2)
#define MODE_MASK_STABILITY   (1U << 3)
#define MODE_MASK_ALL         (MODE_MASK_CORRECTNESS | MODE_MASK_PERFORMANCE | MODE_MASK_MEMORY | MODE_MASK_STABILITY)

typedef struct {
    const char *lib_path;
    unsigned int test_mask;
    unsigned int mode_mask;
    unsigned long long iterations;
    double duration_hours;
    size_t msg_len;
    int digest_len_bits;
    int cycles_enabled;
    unsigned long long stability_max_cases;
} cli_options_t;

typedef struct {
    unsigned int mask;
    ngcc_test_kind_t kind;
    const char *name;
} test_entry_t;

static const test_entry_t k_tests[] = {
    {TEST_MASK_HASH, NGCC_TEST_HASH, "hash"},
    {TEST_MASK_SIG, NGCC_TEST_SIG, "sig"},
    {TEST_MASK_KEM, NGCC_TEST_KEM, "kem"},
    {TEST_MASK_KEX, NGCC_TEST_KEX, "kex"}
};

static void print_usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s --lib /path/to/lib.so --test hash|sig|kem|kex|all --mode correctness|performance|memory|stability|all\n", prog);
    printf("     [--iterations N] [--duration-hours H] [--msg-len BYTES] [--digest-len-bits BITS] [--cycles on|off]\n");
}

static int parse_unsigned_ll(const char *s, unsigned long long *out) {
    char *end = NULL;
    unsigned long long v;

    if (s == NULL || out == NULL || *s == '\0') {
        return -1;
    }

    v = strtoull(s, &end, 10);
    if (end == NULL || *end != '\0') {
        return -1;
    }

    *out = v;
    return 0;
}

static int parse_int_value(const char *s, int *out) {
    char *end = NULL;
    long v;

    if (s == NULL || out == NULL || *s == '\0') {
        return -1;
    }

    v = strtol(s, &end, 10);
    if (end == NULL || *end != '\0') {
        return -1;
    }

    *out = (int) v;
    return 0;
}

static int parse_double_value(const char *s, double *out) {
    char *end = NULL;
    double v;

    if (s == NULL || out == NULL || *s == '\0') {
        return -1;
    }

    v = strtod(s, &end);
    if (end == NULL || *end != '\0') {
        return -1;
    }

    *out = v;
    return 0;
}

static int parse_test_mask(const char *s, unsigned int *out_mask) {
    if (strcmp(s, "hash") == 0) {
        *out_mask = TEST_MASK_HASH;
        return 0;
    }
    if (strcmp(s, "sig") == 0) {
        *out_mask = TEST_MASK_SIG;
        return 0;
    }
    if (strcmp(s, "kem") == 0) {
        *out_mask = TEST_MASK_KEM;
        return 0;
    }
    if (strcmp(s, "kex") == 0) {
        *out_mask = TEST_MASK_KEX;
        return 0;
    }
    if (strcmp(s, "all") == 0) {
        *out_mask = TEST_MASK_ALL;
        return 0;
    }
    return -1;
}

static int parse_mode_mask(const char *s, unsigned int *out_mask) {
    if (strcmp(s, "correctness") == 0) {
        *out_mask = MODE_MASK_CORRECTNESS;
        return 0;
    }
    if (strcmp(s, "performance") == 0) {
        *out_mask = MODE_MASK_PERFORMANCE;
        return 0;
    }
    if (strcmp(s, "memory") == 0) {
        *out_mask = MODE_MASK_MEMORY;
        return 0;
    }
    if (strcmp(s, "stability") == 0) {
        *out_mask = MODE_MASK_STABILITY;
        return 0;
    }
    if (strcmp(s, "all") == 0) {
        *out_mask = MODE_MASK_ALL;
        return 0;
    }
    return -1;
}

static int parse_cycles(const char *s, int *enabled) {
    if (strcmp(s, "on") == 0) {
        *enabled = 1;
        return 0;
    }
    if (strcmp(s, "off") == 0) {
        *enabled = 0;
        return 0;
    }
    return -1;
}

static int run_correctness_for_test(const ngcc_api_t *api,
                                    ngcc_test_kind_t kind,
                                    const char *name,
                                    const cli_options_t *opts) {
    int rc;

    switch (kind) {
        case NGCC_TEST_HASH:
            rc = ngcc_hash_correctness(api, opts->digest_len_bits, opts->msg_len);
            break;
        case NGCC_TEST_SIG:
            rc = ngcc_sig_correctness(api, opts->msg_len);
            break;
        case NGCC_TEST_KEM:
            rc = ngcc_kem_correctness(api);
            break;
        case NGCC_TEST_KEX:
            rc = ngcc_kex_correctness(api);
            break;
        default:
            rc = -1;
            break;
    }

    printf("[%s][correctness] %s\n", name, rc == 0 ? "PASS" : "FAIL");
    return rc;
}

static int run_performance_for_test(const ngcc_api_t *api,
                                    ngcc_test_kind_t kind,
                                    const char *name,
                                    const cli_options_t *opts) {
    ngcc_perf_config_t cfg;
    ngcc_perf_result_t result;
    int rc;

    cfg.iterations = opts->iterations;
    cfg.cycles_enabled = opts->cycles_enabled;

    switch (kind) {
        case NGCC_TEST_HASH:
            rc = ngcc_hash_performance(api, opts->digest_len_bits, opts->msg_len, &cfg, &result);
            break;
        case NGCC_TEST_SIG:
            rc = ngcc_sig_performance(api, opts->msg_len, &cfg, &result);
            break;
        case NGCC_TEST_KEM:
            rc = ngcc_kem_performance(api, &cfg, &result);
            break;
        case NGCC_TEST_KEX:
            rc = ngcc_kex_performance(api, &cfg, &result);
            break;
        default:
            rc = -1;
            break;
    }

    if (rc != 0) {
        printf("[%s][performance] FAIL\n", name);
        return rc;
    }

    if (result.cycles_available) {
        printf("[%s][performance] ops=%llu time_ms=%.3f ops/s=%.3f cycles/op=%.3f\n",
               name,
               result.iterations,
               result.elapsed_ms,
               result.ops_per_sec,
               result.cycles_per_op);
    } else {
        printf("[%s][performance] ops=%llu time_ms=%.3f ops/s=%.3f\n",
               name,
               result.iterations,
               result.elapsed_ms,
               result.ops_per_sec);
    }

    return 0;
}

static int run_stability_for_test(const ngcc_api_t *api,
                                  ngcc_test_kind_t kind,
                                  const char *name,
                                  const cli_options_t *opts) {
    ngcc_stability_result_t result;
    int rc;

    rc = ngcc_run_stability(api,
                            kind,
                            opts->digest_len_bits,
                            opts->msg_len,
                            opts->duration_hours,
                            opts->stability_max_cases,
                            &result);

    if (rc == 0) {
        const char *status = result.interrupted ? "STOPPED" : "PASS";
        printf("[%s][stability] %s cases=%llu elapsed_s=%.3f\n",
               name,
               status,
               result.cases_run,
               result.elapsed_seconds);
    } else {
        printf("[%s][stability] FAIL cases=%llu elapsed_s=%.3f\n",
               name,
               result.cases_run,
               result.elapsed_seconds);
    }

    return rc;
}

static int run_memory_mode(const ngcc_api_t *api, const cli_options_t *opts) {
    uint64_t baseline_bytes;
    uint64_t peak_bytes;
    size_t i;
    int failed = 0;

    baseline_bytes = ngcc_mem_current_rss_bytes();

    for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
        if ((opts->test_mask & k_tests[i].mask) == 0) {
            continue;
        }
        if (run_correctness_for_test(api, k_tests[i].kind, k_tests[i].name, opts) != 0) {
            failed = 1;
        }
    }

    peak_bytes = ngcc_mem_peak_rss_bytes();
    printf("[memory] baseline_bytes=%llu peak_bytes=%llu\n",
           (unsigned long long) baseline_bytes,
           (unsigned long long) peak_bytes);

    return failed ? -1 : 0;
}

int main(int argc, char **argv) {
    cli_options_t opts;
    ngcc_library_t lib;
    int ch;
    int option_index = 0;
    size_t i;
    int failed = 0;
    int hash_selected;

    static const struct option long_options[] = {
        {"lib", required_argument, NULL, 'l'},
        {"test", required_argument, NULL, 't'},
        {"mode", required_argument, NULL, 'm'},
        {"iterations", required_argument, NULL, 'i'},
        {"duration-hours", required_argument, NULL, 'd'},
        {"msg-len", required_argument, NULL, 'g'},
        {"digest-len-bits", required_argument, NULL, 'b'},
        {"cycles", required_argument, NULL, 'c'},
        {"help", no_argument, NULL, 'h'},
        {0, 0, 0, 0}
    };

    memset(&opts, 0, sizeof(opts));
    opts.test_mask = TEST_MASK_ALL;
    opts.mode_mask = MODE_MASK_ALL;
    opts.iterations = 1000;
    opts.duration_hours = 6.0;
    opts.msg_len = 1024;
    opts.digest_len_bits = 0;
    opts.cycles_enabled = 1;
    opts.stability_max_cases = 3000;

    while ((ch = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
        switch (ch) {
            case 'l':
                opts.lib_path = optarg;
                break;
            case 't':
                if (parse_test_mask(optarg, &opts.test_mask) != 0) {
                    fprintf(stderr, "invalid --test value: %s\n", optarg);
                    return 1;
                }
                break;
            case 'm':
                if (parse_mode_mask(optarg, &opts.mode_mask) != 0) {
                    fprintf(stderr, "invalid --mode value: %s\n", optarg);
                    return 1;
                }
                break;
            case 'i':
                if (parse_unsigned_ll(optarg, &opts.iterations) != 0 || opts.iterations == 0) {
                    fprintf(stderr, "invalid --iterations value: %s\n", optarg);
                    return 1;
                }
                break;
            case 'd':
                if (parse_double_value(optarg, &opts.duration_hours) != 0 || opts.duration_hours <= 0.0) {
                    fprintf(stderr, "invalid --duration-hours value: %s\n", optarg);
                    return 1;
                }
                break;
            case 'g': {
                unsigned long long msg_len_val;
                if (parse_unsigned_ll(optarg, &msg_len_val) != 0 || msg_len_val == 0) {
                    fprintf(stderr, "invalid --msg-len value: %s\n", optarg);
                    return 1;
                }
                opts.msg_len = (size_t) msg_len_val;
                break;
            }
            case 'b':
                if (parse_int_value(optarg, &opts.digest_len_bits) != 0 || opts.digest_len_bits <= 0) {
                    fprintf(stderr, "invalid --digest-len-bits value: %s\n", optarg);
                    return 1;
                }
                break;
            case 'c':
                if (parse_cycles(optarg, &opts.cycles_enabled) != 0) {
                    fprintf(stderr, "invalid --cycles value: %s\n", optarg);
                    return 1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (opts.lib_path == NULL) {
        fprintf(stderr, "error: --lib is required\n");
        print_usage(argv[0]);
        return 1;
    }

    hash_selected = (opts.test_mask & TEST_MASK_HASH) != 0;
    if (hash_selected && opts.digest_len_bits <= 0) {
        fprintf(stderr, "error: --digest-len-bits is required when hash test is selected\n");
        return 1;
    }

    if (ngcc_load_library(opts.lib_path, &lib) != 0) {
        fprintf(stderr, "error: failed to load library: %s\n", opts.lib_path);
        return 1;
    }

    if (opts.mode_mask & MODE_MASK_CORRECTNESS) {
        for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
            if ((opts.test_mask & k_tests[i].mask) == 0) {
                continue;
            }
            if (run_correctness_for_test(&lib.api, k_tests[i].kind, k_tests[i].name, &opts) != 0) {
                failed = 1;
            }
        }
    }

    if (opts.mode_mask & MODE_MASK_PERFORMANCE) {
        for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
            if ((opts.test_mask & k_tests[i].mask) == 0) {
                continue;
            }
            if (run_performance_for_test(&lib.api, k_tests[i].kind, k_tests[i].name, &opts) != 0) {
                failed = 1;
            }
        }
    }

    if (opts.mode_mask & MODE_MASK_MEMORY) {
        if (run_memory_mode(&lib.api, &opts) != 0) {
            failed = 1;
        }
    }

    if (opts.mode_mask & MODE_MASK_STABILITY) {
        for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
            if ((opts.test_mask & k_tests[i].mask) == 0) {
                continue;
            }
            if (run_stability_for_test(&lib.api, k_tests[i].kind, k_tests[i].name, &opts) != 0) {
                failed = 1;
            }
        }
    }

    ngcc_unload_library(&lib);
    return failed ? 1 : 0;
}
