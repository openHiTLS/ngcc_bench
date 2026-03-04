#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_parser.h"

#ifndef NGCC_VERSION
#define NGCC_VERSION "unknown"
#endif

/* ── Helpers ───────────────────────────────────────────────────── */

void print_version(void) {
    printf("ngcc_bench %s\n", NGCC_VERSION);
}

void print_usage(const char *prog) {
    printf("ngcc_bench %s\n\n", NGCC_VERSION);
    printf("Usage:\n");
    printf("  %s --lib /path/to/lib.so --test hash|dsa|dsa-keygen|dsa-sig|dsa-verify|kem|kem-keygen|kem-encap|kem-decap|kex|all --mode correctness|performance|memory|stability|all\n", prog);
    printf("     [--duration-hours H] [--stability-max-cases N] [--stability-sample-ms MS]\n");
    printf("     [--json-out PATH] [--kat FILE]\n");
    printf("     [--stable-throughput-cv-percent P] [--stable-cycles-cv-percent P] [--stable-time-cv-percent P]\n");
    printf("     [--stable-memory-growth-percent P] [--stable-error-rate-percent P]\n");
    printf("     [--warning-throughput-cv-percent P] [--warning-cycles-cv-percent P] [--warning-time-cv-percent P]\n");
    printf("     [--warning-memory-growth-percent P] [--warning-error-rate-percent P]\n");
    printf("\n");
    printf("  %s\n", prog);
    printf("     Launch interactive menu mode.\n");
}

void init_default_options(cli_options_t *opts) {
    memset(opts, 0, sizeof(*opts));
    opts->test_mask = TEST_MASK_ALL;
    opts->mode_mask = MODE_MASK_ALL;
    opts->duration_hours = 6.0;
    opts->stability_max_cases = 3000;
    opts->stability_sample_ms = 1.0;
    ngcc_stability_thresholds_set_defaults(&opts->stability_thresholds);
}

int parse_unsigned_ll(const char *s, unsigned long long *out) {
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

int parse_int_value(const char *s, int *out) {
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

int parse_double_value(const char *s, double *out) {
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

static int parse_nonnegative_double_option(const char *opt_name, const char *opt_value, double *out) {
    if (parse_double_value(opt_value, out) != 0 || !isfinite(*out) || *out < 0.0) {
        fprintf(stderr, "invalid --%s value: %s\n", opt_name, opt_value);
        return -1;
    }
    return 0;
}

int parse_test_mask(const char *s, unsigned int *out_mask) {
    if (strcmp(s, "hash") == 0) {
        *out_mask = TEST_MASK_HASH;
        return 0;
    }
    if (strcmp(s, "dsa") == 0) {
        *out_mask = TEST_MASK_DSA;
        return 0;
    }
    if (strcmp(s, "dsa-keygen") == 0) {
        *out_mask = TEST_MASK_DSA_KEYGEN;
        return 0;
    }
    if (strcmp(s, "dsa-sig") == 0) {
        *out_mask = TEST_MASK_DSA_SIG;
        return 0;
    }
    if (strcmp(s, "dsa-verify") == 0) {
        *out_mask = TEST_MASK_DSA_VERIFY;
        return 0;
    }
    if (strcmp(s, "kem") == 0) {
        *out_mask = TEST_MASK_KEM;
        return 0;
    }
    if (strcmp(s, "kem-keygen") == 0) {
        *out_mask = TEST_MASK_KEM_KEYGEN;
        return 0;
    }
    if (strcmp(s, "kem-encap") == 0) {
        *out_mask = TEST_MASK_KEM_ENCAP;
        return 0;
    }
    if (strcmp(s, "kem-decap") == 0) {
        *out_mask = TEST_MASK_KEM_DECAP;
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

int parse_mode_mask(const char *s, unsigned int *out_mask) {
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



/* ── Main option parser ────────────────────────────────────────── */

int parse_cli_options(int argc, char **argv, cli_options_t *opts) {
    int ch;
    int option_index = 0;
    static const struct option long_options[] = {
        {"lib", required_argument, NULL, 'l'},
        {"test", required_argument, NULL, 't'},
        {"mode", required_argument, NULL, 'm'},
        {"duration-hours", required_argument, NULL, 'd'},
        {"stability-max-cases", required_argument, NULL, 's'},
        {"stability-sample-ms", required_argument, NULL, OPT_STABILITY_SAMPLE_MS},
        {"json-out", required_argument, NULL, 'j'},
        {"kat", required_argument, NULL, 'k'},
        {"stable-throughput-cv-percent", required_argument, NULL, OPT_STABLE_THROUGHPUT_CV},
        {"stable-cycles-cv-percent", required_argument, NULL, OPT_STABLE_CYCLES_CV},
        {"stable-time-cv-percent", required_argument, NULL, OPT_STABLE_TIME_CV},
        {"stable-memory-growth-percent", required_argument, NULL, OPT_STABLE_MEMORY_GROWTH},
        {"stable-error-rate-percent", required_argument, NULL, OPT_STABLE_ERROR_RATE},
        {"warning-throughput-cv-percent", required_argument, NULL, OPT_WARNING_THROUGHPUT_CV},
        {"warning-cycles-cv-percent", required_argument, NULL, OPT_WARNING_CYCLES_CV},
        {"warning-time-cv-percent", required_argument, NULL, OPT_WARNING_TIME_CV},
        {"warning-memory-growth-percent", required_argument, NULL, OPT_WARNING_MEMORY_GROWTH},
        {"warning-error-rate-percent", required_argument, NULL, OPT_WARNING_ERROR_RATE},
        {"help", no_argument, NULL, 'h'},
        {"version", no_argument, NULL, 'v'},
        {0, 0, 0, 0}
    };

    while ((ch = getopt_long(argc, argv, "", long_options, &option_index)) != -1) {
        switch (ch) {
            case 'l':
                opts->lib_path = optarg;
                break;
            case 't':
                if (parse_test_mask(optarg, &opts->test_mask) != 0) {
                    fprintf(stderr, "invalid --test value: %s\n", optarg);
                    return -1;
                }
                break;
            case 'm':
                if (parse_mode_mask(optarg, &opts->mode_mask) != 0) {
                    fprintf(stderr, "invalid --mode value: %s\n", optarg);
                    return -1;
                }
                break;
            case 'd':
                if (parse_double_value(optarg, &opts->duration_hours) != 0 || opts->duration_hours <= 0.0) {
                    fprintf(stderr, "invalid --duration-hours value: %s\n", optarg);
                    return -1;
                }
                break;
            case 's':
                if (parse_unsigned_ll(optarg, &opts->stability_max_cases) != 0 || opts->stability_max_cases == 0) {
                    fprintf(stderr, "invalid --stability-max-cases value: %s\n", optarg);
                    return -1;
                }
                break;
            case OPT_STABILITY_SAMPLE_MS:
                if (parse_double_value(optarg, &opts->stability_sample_ms) != 0 ||
                    !isfinite(opts->stability_sample_ms) ||
                    opts->stability_sample_ms <= 0.0) {
                    fprintf(stderr, "invalid --stability-sample-ms value: %s\n", optarg);
                    return -1;
                }
                break;

            case 'j':
                opts->json_out_path = optarg;
                break;
            case 'k':
                opts->kat_path = optarg;
                break;
            case OPT_STABLE_THROUGHPUT_CV:
                if (parse_nonnegative_double_option("stable-throughput-cv-percent", optarg,
                                                    &opts->stability_thresholds.stable_throughput_cv_percent) != 0) {
                    return -1;
                }
                break;
            case OPT_STABLE_CYCLES_CV:
                if (parse_nonnegative_double_option("stable-cycles-cv-percent", optarg,
                                                    &opts->stability_thresholds.stable_cycles_cv_percent) != 0) {
                    return -1;
                }
                break;
            case OPT_STABLE_TIME_CV:
                if (parse_nonnegative_double_option("stable-time-cv-percent", optarg,
                                                    &opts->stability_thresholds.stable_time_cv_percent) != 0) {
                    return -1;
                }
                break;
            case OPT_STABLE_MEMORY_GROWTH:
                if (parse_nonnegative_double_option("stable-memory-growth-percent", optarg,
                                                    &opts->stability_thresholds.stable_memory_growth_percent) != 0) {
                    return -1;
                }
                break;
            case OPT_STABLE_ERROR_RATE:
                if (parse_nonnegative_double_option("stable-error-rate-percent", optarg,
                                                    &opts->stability_thresholds.stable_error_rate_percent) != 0) {
                    return -1;
                }
                break;
            case OPT_WARNING_THROUGHPUT_CV:
                if (parse_nonnegative_double_option("warning-throughput-cv-percent", optarg,
                                                    &opts->stability_thresholds.warning_throughput_cv_percent) != 0) {
                    return -1;
                }
                break;
            case OPT_WARNING_CYCLES_CV:
                if (parse_nonnegative_double_option("warning-cycles-cv-percent", optarg,
                                                    &opts->stability_thresholds.warning_cycles_cv_percent) != 0) {
                    return -1;
                }
                break;
            case OPT_WARNING_TIME_CV:
                if (parse_nonnegative_double_option("warning-time-cv-percent", optarg,
                                                    &opts->stability_thresholds.warning_time_cv_percent) != 0) {
                    return -1;
                }
                break;
            case OPT_WARNING_MEMORY_GROWTH:
                if (parse_nonnegative_double_option("warning-memory-growth-percent", optarg,
                                                    &opts->stability_thresholds.warning_memory_growth_percent) != 0) {
                    return -1;
                }
                break;
            case OPT_WARNING_ERROR_RATE:
                if (parse_nonnegative_double_option("warning-error-rate-percent", optarg,
                                                    &opts->stability_thresholds.warning_error_rate_percent) != 0) {
                    return -1;
                }
                break;
            case 'h':
                print_usage(argv[0]);
                return 1;
            case 'v':
                print_version();
                return 1;
            default:
                print_usage(argv[0]);
                return -1;
        }
    }

    return 0;
}

/* ── Validation ────────────────────────────────────────────────── */

int validate_options(const cli_options_t *opts) {
    int correctness_selected;
    const ngcc_stability_thresholds_t *thr;

    if (opts == NULL) {
        return -1;
    }

    thr = &opts->stability_thresholds;

    if (opts->lib_path == NULL) {
        fprintf(stderr, "error: --lib is required\n");
        return -1;
    }

    correctness_selected = (opts->mode_mask & MODE_MASK_CORRECTNESS) != 0;
    if (opts->kat_path != NULL && !correctness_selected) {
        fprintf(stderr, "error: --kat requires correctness mode\n");
        return -1;
    }

    if (!isfinite(opts->stability_sample_ms) || opts->stability_sample_ms <= 0.0) {
        fprintf(stderr, "error: --stability-sample-ms must be > 0\n");
        return -1;
    }

    if (thr->warning_throughput_cv_percent < thr->stable_throughput_cv_percent ||
        thr->warning_cycles_cv_percent < thr->stable_cycles_cv_percent ||
        thr->warning_time_cv_percent < thr->stable_time_cv_percent ||
        thr->warning_memory_growth_percent < thr->stable_memory_growth_percent ||
        thr->warning_error_rate_percent < thr->stable_error_rate_percent) {
        fprintf(stderr, "error: warning thresholds must be >= stable thresholds\n");
        return -1;
    }

    return 0;
}
