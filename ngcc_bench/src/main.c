#include <getopt.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

enum {
    OPT_STABLE_THROUGHPUT_CV = 1000,
    OPT_STABLE_CYCLES_CV,
    OPT_STABLE_TIME_CV,
    OPT_STABLE_MEMORY_GROWTH,
    OPT_STABLE_ERROR_RATE,
    OPT_WARNING_THROUGHPUT_CV,
    OPT_WARNING_CYCLES_CV,
    OPT_WARNING_TIME_CV,
    OPT_WARNING_MEMORY_GROWTH,
    OPT_WARNING_ERROR_RATE,
    OPT_STABILITY_SAMPLE_MS
};

typedef struct {
    const char *lib_path;
    const char *json_out_path;
    const char *kat_path;
    unsigned int test_mask;
    unsigned int mode_mask;
    unsigned long long iterations;
    double duration_hours;
    size_t msg_len;
    int digest_len_bits;
    int cycles_enabled;
    unsigned long long stability_max_cases;
    double stability_sample_ms;
    ngcc_stability_thresholds_t stability_thresholds;
} cli_options_t;

typedef struct {
    unsigned int mask;
    ngcc_test_kind_t kind;
    const char *name;
} test_entry_t;

typedef enum {
    STATUS_SKIPPED = 0,
    STATUS_PASS,
    STATUS_FAIL,
    STATUS_STOPPED
} run_status_t;

typedef struct {
    const char *name;
    int selected;
    run_status_t correctness_status;
    run_status_t performance_status;
    run_status_t stability_status;
    ngcc_perf_result_t performance;
    ngcc_stability_result_t stability;
    int kat_used;
    unsigned long long kat_total;
    unsigned long long kat_passed;
    unsigned long long kat_failed;
} test_report_t;

typedef struct {
    test_report_t tests[4];
    run_status_t memory_status;
    uint64_t memory_baseline_bytes;
    uint64_t memory_peak_bytes;
} run_report_t;

static const test_entry_t k_tests[] = {
    {TEST_MASK_HASH, NGCC_TEST_HASH, "hash"},
    {TEST_MASK_SIG, NGCC_TEST_SIG, "sig"},
    {TEST_MASK_KEM, NGCC_TEST_KEM, "kem"},
    {TEST_MASK_KEX, NGCC_TEST_KEX, "kex"}
};

static void print_usage(const char *prog) {
    printf("Usage:\n");
    printf("  %s --lib /path/to/lib.so --test hash|sig|kem|kex|all --mode correctness|performance|memory|stability|all\n", prog);
    printf("     [--iterations N] [--duration-hours H] [--stability-max-cases N] [--stability-sample-ms MS] [--msg-len BYTES]\n");
    printf("     [--digest-len-bits BITS] [--cycles on|off] [--json-out PATH] [--kat FILE]\n");
    printf("     [--stable-throughput-cv-percent P] [--stable-cycles-cv-percent P] [--stable-time-cv-percent P]\n");
    printf("     [--stable-memory-growth-percent P] [--stable-error-rate-percent P]\n");
    printf("     [--warning-throughput-cv-percent P] [--warning-cycles-cv-percent P] [--warning-time-cv-percent P]\n");
    printf("     [--warning-memory-growth-percent P] [--warning-error-rate-percent P]\n");
    printf("\n");
    printf("  %s\n", prog);
    printf("     Launch interactive menu mode.\n");
}

static void init_default_options(cli_options_t *opts) {
    memset(opts, 0, sizeof(*opts));
    opts->test_mask = TEST_MASK_ALL;
    opts->mode_mask = MODE_MASK_ALL;
    opts->iterations = 1000;
    opts->duration_hours = 6.0;
    opts->msg_len = 1024;
    opts->digest_len_bits = 0;
    opts->cycles_enabled = 1;
    opts->stability_max_cases = 3000;
    opts->stability_sample_ms = 1.0;
    ngcc_stability_thresholds_set_defaults(&opts->stability_thresholds);
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

static int parse_nonnegative_double_option(const char *opt_name, const char *opt_value, double *out) {
    if (parse_double_value(opt_value, out) != 0 || !isfinite(*out) || *out < 0.0) {
        fprintf(stderr, "invalid --%s value: %s\n", opt_name, opt_value);
        return -1;
    }
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

static const char *status_to_text(run_status_t status) {
    switch (status) {
        case STATUS_PASS:
            return "PASS";
        case STATUS_FAIL:
            return "FAIL";
        case STATUS_STOPPED:
            return "STOPPED";
        case STATUS_SKIPPED:
        default:
            return "SKIPPED";
    }
}

static void json_write_escaped(FILE *fp, const char *s) {
    const unsigned char *p = (const unsigned char *) s;

    fputc('"', fp);
    if (p != NULL) {
        while (*p != '\0') {
            switch (*p) {
                case '\\':
                    fputs("\\\\", fp);
                    break;
                case '"':
                    fputs("\\\"", fp);
                    break;
                case '\n':
                    fputs("\\n", fp);
                    break;
                case '\r':
                    fputs("\\r", fp);
                    break;
                case '\t':
                    fputs("\\t", fp);
                    break;
                default:
                    if (*p < 0x20U) {
                        fprintf(fp, "\\u%04x", (unsigned int) *p);
                    } else {
                        fputc((int) *p, fp);
                    }
                    break;
            }
            ++p;
        }
    }
    fputc('"', fp);
}

static int prompt_read_line(const char *prompt, char *buf, size_t buf_len) {
    size_t n;

    if (prompt != NULL) {
        printf("%s", prompt);
        fflush(stdout);
    }

    if (fgets(buf, (int) buf_len, stdin) == NULL) {
        return -1;
    }

    n = strlen(buf);
    if (n > 0 && buf[n - 1] == '\n') {
        buf[n - 1] = '\0';
    }
    return 0;
}

static int prompt_menu_choice(const char *title, const char *const *items, size_t item_count, int *out_choice) {
    char line[64];
    unsigned long long choice;
    size_t i;

    printf("\n%s\n", title);
    for (i = 0; i < item_count; ++i) {
        printf("  %zu) %s\n", i + 1U, items[i]);
    }

    while (1) {
        if (prompt_read_line("Select: ", line, sizeof(line)) != 0) {
            return -1;
        }
        if (parse_unsigned_ll(line, &choice) == 0 && choice >= 1U && choice <= (unsigned long long) item_count) {
            *out_choice = (int) choice;
            return 0;
        }
        printf("Invalid selection.\n");
    }
}

static int prompt_optional_u64(const char *label, unsigned long long current, unsigned long long *out_value) {
    char line[128];
    unsigned long long v;
    char prompt[256];

    snprintf(prompt, sizeof(prompt), "%s [%llu]: ", label, current);
    if (prompt_read_line(prompt, line, sizeof(line)) != 0) {
        return -1;
    }
    if (line[0] == '\0') {
        *out_value = current;
        return 0;
    }
    if (parse_unsigned_ll(line, &v) != 0 || v == 0U) {
        printf("Invalid number.\n");
        return -1;
    }
    *out_value = v;
    return 0;
}

static int prompt_optional_double(const char *label, double current, double *out_value) {
    char line[128];
    double v;
    char prompt[256];

    snprintf(prompt, sizeof(prompt), "%s [%.3f]: ", label, current);
    if (prompt_read_line(prompt, line, sizeof(line)) != 0) {
        return -1;
    }
    if (line[0] == '\0') {
        *out_value = current;
        return 0;
    }
    if (parse_double_value(line, &v) != 0 || v <= 0.0) {
        printf("Invalid number.\n");
        return -1;
    }
    *out_value = v;
    return 0;
}

static int prompt_optional_nonnegative_double(const char *label, double current, double *out_value) {
    char line[128];
    double v;
    char prompt[256];

    snprintf(prompt, sizeof(prompt), "%s [%.3f]: ", label, current);
    if (prompt_read_line(prompt, line, sizeof(line)) != 0) {
        return -1;
    }
    if (line[0] == '\0') {
        *out_value = current;
        return 0;
    }
    if (parse_double_value(line, &v) != 0 || !isfinite(v) || v < 0.0) {
        printf("Invalid number.\n");
        return -1;
    }
    *out_value = v;
    return 0;
}

static int prompt_optional_int(const char *label, int current, int *out_value) {
    char line[128];
    int v;
    char prompt[256];

    snprintf(prompt, sizeof(prompt), "%s [%d]: ", label, current);
    if (prompt_read_line(prompt, line, sizeof(line)) != 0) {
        return -1;
    }
    if (line[0] == '\0') {
        *out_value = current;
        return 0;
    }
    if (parse_int_value(line, &v) != 0 || v <= 0) {
        printf("Invalid number.\n");
        return -1;
    }
    *out_value = v;
    return 0;
}

static int run_interactive_setup(cli_options_t *opts,
                                 char *lib_buf,
                                 size_t lib_buf_len,
                                 char *kat_buf,
                                 size_t kat_buf_len,
                                 char *json_buf,
                                 size_t json_buf_len) {
    static const char *const test_items[] = {"hash", "sig", "kem", "kex", "all"};
    static const char *const mode_items[] = {"correctness", "performance", "memory", "stability", "all"};
    static const char *const cycles_items[] = {"on", "off"};
    int test_choice;
    int mode_choice;
    int cycles_choice;
    unsigned long long u64_tmp;
    double d_tmp;
    int i_tmp;
    int hash_selected;
    int correctness_selected;
    int performance_selected;
    int stability_selected;

    printf("NGCC Benchmark Interactive Mode\n");
    printf("--------------------------------\n");

    while (1) {
        if (prompt_read_line("Library path: ", lib_buf, lib_buf_len) != 0) {
            return -1;
        }
        if (lib_buf[0] != '\0') {
            break;
        }
        printf("Library path is required.\n");
    }
    opts->lib_path = lib_buf;

    if (prompt_menu_choice("Select test target:", test_items, sizeof(test_items) / sizeof(test_items[0]), &test_choice) != 0) {
        return -1;
    }
    if (parse_test_mask(test_items[test_choice - 1], &opts->test_mask) != 0) {
        return -1;
    }

    if (prompt_menu_choice("Select mode:", mode_items, sizeof(mode_items) / sizeof(mode_items[0]), &mode_choice) != 0) {
        return -1;
    }
    if (parse_mode_mask(mode_items[mode_choice - 1], &opts->mode_mask) != 0) {
        return -1;
    }

    hash_selected = (opts->test_mask & TEST_MASK_HASH) != 0;
    correctness_selected = (opts->mode_mask & MODE_MASK_CORRECTNESS) != 0;
    performance_selected = (opts->mode_mask & MODE_MASK_PERFORMANCE) != 0;
    stability_selected = (opts->mode_mask & MODE_MASK_STABILITY) != 0;

    if (prompt_optional_u64("Message length bytes", (unsigned long long) opts->msg_len, &u64_tmp) != 0) {
        return -1;
    }
    opts->msg_len = (size_t) u64_tmp;

    if (hash_selected) {
        if (prompt_optional_int("Digest length bits", opts->digest_len_bits > 0 ? opts->digest_len_bits : 256, &i_tmp) != 0) {
            return -1;
        }
        opts->digest_len_bits = i_tmp;
    }

    if (performance_selected) {
        if (prompt_optional_u64("Performance iterations", opts->iterations, &opts->iterations) != 0) {
            return -1;
        }
    }

    if (stability_selected) {
        d_tmp = opts->duration_hours;
        if (prompt_optional_double("Stability duration hours", d_tmp, &d_tmp) != 0) {
            return -1;
        }
        opts->duration_hours = d_tmp;

        if (prompt_optional_u64("Stability max cases", opts->stability_max_cases, &opts->stability_max_cases) != 0) {
            return -1;
        }

        d_tmp = opts->stability_sample_ms;
        if (prompt_optional_double("Stability sample window ms", d_tmp, &d_tmp) != 0) {
            return -1;
        }
        opts->stability_sample_ms = d_tmp;

        printf("\nStability thresholds (percent, press Enter to keep default):\n");
        if (prompt_optional_nonnegative_double("  stable throughput cv", opts->stability_thresholds.stable_throughput_cv_percent,
                                               &opts->stability_thresholds.stable_throughput_cv_percent) != 0) {
            return -1;
        }
        if (prompt_optional_nonnegative_double("  stable cycles cv", opts->stability_thresholds.stable_cycles_cv_percent,
                                               &opts->stability_thresholds.stable_cycles_cv_percent) != 0) {
            return -1;
        }
        if (prompt_optional_nonnegative_double("  stable time cv", opts->stability_thresholds.stable_time_cv_percent,
                                               &opts->stability_thresholds.stable_time_cv_percent) != 0) {
            return -1;
        }
        if (prompt_optional_nonnegative_double("  stable memory growth", opts->stability_thresholds.stable_memory_growth_percent,
                                               &opts->stability_thresholds.stable_memory_growth_percent) != 0) {
            return -1;
        }
        if (prompt_optional_nonnegative_double("  stable error rate", opts->stability_thresholds.stable_error_rate_percent,
                                               &opts->stability_thresholds.stable_error_rate_percent) != 0) {
            return -1;
        }
        if (prompt_optional_nonnegative_double("  warning throughput cv", opts->stability_thresholds.warning_throughput_cv_percent,
                                               &opts->stability_thresholds.warning_throughput_cv_percent) != 0) {
            return -1;
        }
        if (prompt_optional_nonnegative_double("  warning cycles cv", opts->stability_thresholds.warning_cycles_cv_percent,
                                               &opts->stability_thresholds.warning_cycles_cv_percent) != 0) {
            return -1;
        }
        if (prompt_optional_nonnegative_double("  warning time cv", opts->stability_thresholds.warning_time_cv_percent,
                                               &opts->stability_thresholds.warning_time_cv_percent) != 0) {
            return -1;
        }
        if (prompt_optional_nonnegative_double("  warning memory growth", opts->stability_thresholds.warning_memory_growth_percent,
                                               &opts->stability_thresholds.warning_memory_growth_percent) != 0) {
            return -1;
        }
        if (prompt_optional_nonnegative_double("  warning error rate", opts->stability_thresholds.warning_error_rate_percent,
                                               &opts->stability_thresholds.warning_error_rate_percent) != 0) {
            return -1;
        }
    }

    if (prompt_menu_choice("Cycle counter output:", cycles_items, sizeof(cycles_items) / sizeof(cycles_items[0]), &cycles_choice) != 0) {
        return -1;
    }
    opts->cycles_enabled = (cycles_choice == 1) ? 1 : 0;

    if (correctness_selected) {
        if (prompt_read_line("Optional KAT file (blank to skip): ", kat_buf, kat_buf_len) != 0) {
            return -1;
        }
        if (kat_buf[0] != '\0') {
            opts->kat_path = kat_buf;
        }
    }

    if (prompt_read_line("Optional JSON output path (blank to skip): ", json_buf, json_buf_len) != 0) {
        return -1;
    }
    if (json_buf[0] != '\0') {
        opts->json_out_path = json_buf;
    }

    return 0;
}

static int parse_cli_options(int argc, char **argv, cli_options_t *opts) {
    int ch;
    int option_index = 0;
    static const struct option long_options[] = {
        {"lib", required_argument, NULL, 'l'},
        {"test", required_argument, NULL, 't'},
        {"mode", required_argument, NULL, 'm'},
        {"iterations", required_argument, NULL, 'i'},
        {"duration-hours", required_argument, NULL, 'd'},
        {"stability-max-cases", required_argument, NULL, 's'},
        {"stability-sample-ms", required_argument, NULL, OPT_STABILITY_SAMPLE_MS},
        {"msg-len", required_argument, NULL, 'g'},
        {"digest-len-bits", required_argument, NULL, 'b'},
        {"cycles", required_argument, NULL, 'c'},
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
            case 'i':
                if (parse_unsigned_ll(optarg, &opts->iterations) != 0 || opts->iterations == 0) {
                    fprintf(stderr, "invalid --iterations value: %s\n", optarg);
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
            case 'g': {
                unsigned long long msg_len_val;
                if (parse_unsigned_ll(optarg, &msg_len_val) != 0 || msg_len_val == 0) {
                    fprintf(stderr, "invalid --msg-len value: %s\n", optarg);
                    return -1;
                }
                opts->msg_len = (size_t) msg_len_val;
                break;
            }
            case 'b':
                if (parse_int_value(optarg, &opts->digest_len_bits) != 0 || opts->digest_len_bits <= 0) {
                    fprintf(stderr, "invalid --digest-len-bits value: %s\n", optarg);
                    return -1;
                }
                break;
            case 'c':
                if (parse_cycles(optarg, &opts->cycles_enabled) != 0) {
                    fprintf(stderr, "invalid --cycles value: %s\n", optarg);
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
            default:
                print_usage(argv[0]);
                return -1;
        }
    }

    return 0;
}

static int validate_options(const cli_options_t *opts) {
    int hash_selected;
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

    hash_selected = (opts->test_mask & TEST_MASK_HASH) != 0;
    if (hash_selected && opts->digest_len_bits <= 0) {
        fprintf(stderr, "error: --digest-len-bits is required when hash test is selected\n");
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

static int write_json_report(const cli_options_t *opts,
                             const run_report_t *report,
                             int overall_failed) {
    FILE *fp;
    time_t now;
    struct tm tm_now;
    char timestamp[64];
    size_t i;

    if (opts == NULL || report == NULL || opts->json_out_path == NULL) {
        return 0;
    }

    fp = fopen(opts->json_out_path, "w");
    if (fp == NULL) {
        fprintf(stderr, "error: failed to open json report: %s\n", opts->json_out_path);
        return -1;
    }

    now = time(NULL);
    if (localtime_r(&now, &tm_now) == NULL) {
        memset(&tm_now, 0, sizeof(tm_now));
    }
    if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S%z", &tm_now) == 0) {
        strcpy(timestamp, "unknown");
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"schema_version\": 3,\n");
    fprintf(fp, "  \"timestamp\": ");
    json_write_escaped(fp, timestamp);
    fprintf(fp, ",\n");

    fprintf(fp, "  \"library\": ");
    json_write_escaped(fp, opts->lib_path);
    fprintf(fp, ",\n");

    fprintf(fp, "  \"options\": {\n");
    fprintf(fp, "    \"test_mask\": %u,\n", opts->test_mask);
    fprintf(fp, "    \"mode_mask\": %u,\n", opts->mode_mask);
    fprintf(fp, "    \"iterations\": %llu,\n", opts->iterations);
    fprintf(fp, "    \"duration_hours\": %.6f,\n", opts->duration_hours);
    fprintf(fp, "    \"stability_max_cases\": %llu,\n", opts->stability_max_cases);
    fprintf(fp, "    \"stability_sample_ms\": %.6f,\n", opts->stability_sample_ms);
    fprintf(fp, "    \"msg_len\": %llu,\n", (unsigned long long) opts->msg_len);
    fprintf(fp, "    \"digest_len_bits\": %d,\n", opts->digest_len_bits);
    fprintf(fp, "    \"cycles\": ");
    json_write_escaped(fp, opts->cycles_enabled ? "on" : "off");
    fprintf(fp, ",\n");
    fprintf(fp, "    \"stability_thresholds\": {\n");
    fprintf(fp, "      \"stable_throughput_cv_percent\": %.6f,\n", opts->stability_thresholds.stable_throughput_cv_percent);
    fprintf(fp, "      \"stable_cycles_cv_percent\": %.6f,\n", opts->stability_thresholds.stable_cycles_cv_percent);
    fprintf(fp, "      \"stable_time_cv_percent\": %.6f,\n", opts->stability_thresholds.stable_time_cv_percent);
    fprintf(fp, "      \"stable_memory_growth_percent\": %.6f,\n", opts->stability_thresholds.stable_memory_growth_percent);
    fprintf(fp, "      \"stable_error_rate_percent\": %.6f,\n", opts->stability_thresholds.stable_error_rate_percent);
    fprintf(fp, "      \"warning_throughput_cv_percent\": %.6f,\n", opts->stability_thresholds.warning_throughput_cv_percent);
    fprintf(fp, "      \"warning_cycles_cv_percent\": %.6f,\n", opts->stability_thresholds.warning_cycles_cv_percent);
    fprintf(fp, "      \"warning_time_cv_percent\": %.6f,\n", opts->stability_thresholds.warning_time_cv_percent);
    fprintf(fp, "      \"warning_memory_growth_percent\": %.6f,\n", opts->stability_thresholds.warning_memory_growth_percent);
    fprintf(fp, "      \"warning_error_rate_percent\": %.6f\n", opts->stability_thresholds.warning_error_rate_percent);
    fprintf(fp, "    },\n");
    fprintf(fp, "    \"kat\": ");
    if (opts->kat_path != NULL) {
        json_write_escaped(fp, opts->kat_path);
    } else {
        fprintf(fp, "null");
    }
    fprintf(fp, "\n");
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"tests\": {\n");
    for (i = 0; i < sizeof(report->tests) / sizeof(report->tests[0]); ++i) {
        const test_report_t *test = &report->tests[i];

        fprintf(fp, "    ");
        json_write_escaped(fp, test->name);
        fprintf(fp, ": {\n");
        fprintf(fp, "      \"selected\": %s,\n", test->selected ? "true" : "false");
        fprintf(fp, "      \"correctness\": ");
        json_write_escaped(fp, status_to_text(test->correctness_status));
        fprintf(fp, ",\n");
        fprintf(fp, "      \"performance\": ");
        json_write_escaped(fp, status_to_text(test->performance_status));
        fprintf(fp, ",\n");
        fprintf(fp, "      \"stability\": ");
        json_write_escaped(fp, status_to_text(test->stability_status));
        fprintf(fp, ",\n");

        fprintf(fp, "      \"kat\": ");
        if (test->kat_used) {
            fprintf(fp, "{");
            fprintf(fp, "\"total\":%llu,", test->kat_total);
            fprintf(fp, "\"passed\":%llu,", test->kat_passed);
            fprintf(fp, "\"failed\":%llu", test->kat_failed);
            fprintf(fp, "}");
        } else {
            fprintf(fp, "null");
        }
        fprintf(fp, ",\n");

        fprintf(fp, "      \"performance_metrics\": ");
        if (test->performance_status == STATUS_PASS) {
            fprintf(fp, "{");
            fprintf(fp, "\"iterations\":%llu,", test->performance.iterations);
            fprintf(fp, "\"warmup_iterations\":%llu,", test->performance.warmup_iterations);
            fprintf(fp, "\"elapsed_ms\":%.6f,", test->performance.elapsed_ms);
            fprintf(fp, "\"total_time_ms\":%.6f,", test->performance.total_time_ms);
            fprintf(fp, "\"ops_per_sec\":%.6f,", test->performance.ops_per_sec);
            fprintf(fp, "\"bytes_per_sec\":%.6f,", test->performance.bytes_per_sec);
            fprintf(fp, "\"bytes_per_op\":%.6f,", test->performance.bytes_per_op);
            fprintf(fp, "\"cycles_available\":%s,", test->performance.cycles_available ? "true" : "false");
            fprintf(fp, "\"cycles_per_op\":%.6f,", test->performance.cycles_per_op);
            fprintf(fp, "\"time_ms_min\":%.6f,", test->performance.time_ms_min);
            fprintf(fp, "\"time_ms_mean\":%.6f,", test->performance.time_ms_mean);
            fprintf(fp, "\"time_ms_median\":%.6f,", test->performance.time_ms_median);
            fprintf(fp, "\"time_ms_max\":%.6f,", test->performance.time_ms_max);
            fprintf(fp, "\"time_ms_stddev\":%.6f,", test->performance.time_ms_stddev);
            fprintf(fp, "\"time_ms_cv_percent\":%.6f,", test->performance.time_ms_cv_percent);
            fprintf(fp, "\"cycles_min\":%.6f,", test->performance.cycles_min);
            fprintf(fp, "\"cycles_median\":%.6f,", test->performance.cycles_median);
            fprintf(fp, "\"cycles_max\":%.6f,", test->performance.cycles_max);
            fprintf(fp, "\"cycles_stddev\":%.6f,", test->performance.cycles_stddev);
            fprintf(fp, "\"cycles_cv_percent\":%.6f", test->performance.cycles_cv_percent);
            fprintf(fp, "}");
        } else {
            fprintf(fp, "null");
        }
        fprintf(fp, ",\n");

        fprintf(fp, "      \"stability_metrics\": ");
        if (test->stability_status != STATUS_SKIPPED) {
            fprintf(fp, "{");
            fprintf(fp, "\"cases_run\":%llu,", test->stability.cases_run);
            fprintf(fp, "\"elapsed_seconds\":%.6f,", test->stability.elapsed_seconds);
            fprintf(fp, "\"interrupted\":%s,", test->stability.interrupted ? "true" : "false");
            fprintf(fp, "\"failed\":%s,", test->stability.failed ? "true" : "false");
            fprintf(fp, "\"status\":");
            json_write_escaped(fp, test->stability.status);
            fprintf(fp, ",");
            fprintf(fp, "\"throughput_mean_ops\":%.6f,", test->stability.throughput_mean_ops);
            fprintf(fp, "\"throughput_stddev_ops\":%.6f,", test->stability.throughput_stddev_ops);
            fprintf(fp, "\"throughput_cv_percent\":%.6f,", test->stability.throughput_cv_percent);
            fprintf(fp, "\"throughput_min_ops\":%.6f,", test->stability.throughput_min_ops);
            fprintf(fp, "\"throughput_max_ops\":%.6f,", test->stability.throughput_max_ops);
            fprintf(fp, "\"throughput_mean_bytes\":%.6f,", test->stability.throughput_mean_bytes);
            fprintf(fp, "\"throughput_stddev_bytes\":%.6f,", test->stability.throughput_stddev_bytes);
            fprintf(fp, "\"throughput_cv_percent_bytes\":%.6f,", test->stability.throughput_cv_percent_bytes);
            fprintf(fp, "\"throughput_min_bytes\":%.6f,", test->stability.throughput_min_bytes);
            fprintf(fp, "\"throughput_max_bytes\":%.6f,", test->stability.throughput_max_bytes);
            fprintf(fp, "\"bytes_per_case\":%.6f,", test->stability.bytes_per_case);
            fprintf(fp, "\"cycles_available\":%s,", test->stability.cycles_available ? "true" : "false");
            fprintf(fp, "\"cycles_mean\":%.6f,", test->stability.cycles_mean);
            fprintf(fp, "\"cycles_stddev\":%.6f,", test->stability.cycles_stddev);
            fprintf(fp, "\"cycles_cv_percent\":%.6f,", test->stability.cycles_cv_percent);
            fprintf(fp, "\"cycles_min\":%.6f,", test->stability.cycles_min);
            fprintf(fp, "\"cycles_max\":%.6f,", test->stability.cycles_max);
            fprintf(fp, "\"time_mean_ms\":%.6f,", test->stability.time_mean_ms);
            fprintf(fp, "\"time_stddev_ms\":%.6f,", test->stability.time_stddev_ms);
            fprintf(fp, "\"time_cv_percent\":%.6f,", test->stability.time_cv_percent);
            fprintf(fp, "\"time_min_ms\":%.6f,", test->stability.time_min_ms);
            fprintf(fp, "\"time_max_ms\":%.6f,", test->stability.time_max_ms);
            fprintf(fp, "\"memory_start_bytes\":%llu,", (unsigned long long) test->stability.memory_start_bytes);
            fprintf(fp, "\"memory_end_bytes\":%llu,", (unsigned long long) test->stability.memory_end_bytes);
            fprintf(fp, "\"memory_min_bytes\":%llu,", (unsigned long long) test->stability.memory_min_bytes);
            fprintf(fp, "\"memory_max_bytes\":%llu,", (unsigned long long) test->stability.memory_max_bytes);
            fprintf(fp, "\"memory_growth_percent\":%.6f,", test->stability.memory_growth_percent);
            fprintf(fp, "\"total_executions\":%llu,", test->stability.total_executions);
            fprintf(fp, "\"error_count\":%llu,", test->stability.error_count);
            fprintf(fp, "\"error_rate_percent\":%.6f,", test->stability.error_rate_percent);
            fprintf(fp, "\"performance_stable\":%s,", test->stability.performance_stable ? "true" : "false");
            fprintf(fp, "\"memory_stable\":%s,", test->stability.memory_stable ? "true" : "false");
            fprintf(fp, "\"correctness_stable\":%s,", test->stability.correctness_stable ? "true" : "false");
            fprintf(fp, "\"is_stable\":%s,", test->stability.is_stable ? "true" : "false");
            fprintf(fp, "\"failure_reasons\":");
            json_write_escaped(fp, test->stability.failure_reasons);
            fprintf(fp, "}");
        } else {
            fprintf(fp, "null");
        }
        fprintf(fp, "\n");
        fprintf(fp, "    }%s\n", (i + 1 == sizeof(report->tests) / sizeof(report->tests[0])) ? "" : ",");
    }
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"memory\": {\n");
    fprintf(fp, "    \"status\": ");
    json_write_escaped(fp, status_to_text(report->memory_status));
    fprintf(fp, ",\n");
    fprintf(fp, "    \"baseline_bytes\": %llu,\n", (unsigned long long) report->memory_baseline_bytes);
    fprintf(fp, "    \"peak_bytes\": %llu\n", (unsigned long long) report->memory_peak_bytes);
    fprintf(fp, "  },\n");

    fprintf(fp, "  \"overall\": {\n");
    fprintf(fp, "    \"status\": ");
    json_write_escaped(fp, overall_failed ? "FAIL" : "PASS");
    fprintf(fp, "\n");
    fprintf(fp, "  }\n");
    fprintf(fp, "}\n");

    fclose(fp);
    printf("[report] json=%s\n", opts->json_out_path);
    return 0;
}

static int run_correctness_for_test(const ngcc_api_t *api,
                                    ngcc_test_kind_t kind,
                                    const char *name,
                                    const cli_options_t *opts,
                                    test_report_t *report) {
    int rc;

    if (opts->kat_path != NULL) {
        unsigned long long total = 0;
        unsigned long long passed = 0;
        unsigned long long failed = 0;
        int kat_rc = 1;

        switch (kind) {
            case NGCC_TEST_HASH:
                kat_rc = ngcc_hash_correctness_kat_file(api,
                                                        opts->digest_len_bits,
                                                        opts->kat_path,
                                                        &total,
                                                        &passed,
                                                        &failed);
                break;
            case NGCC_TEST_SIG:
                kat_rc = ngcc_sig_correctness_kat_file(api,
                                                       opts->kat_path,
                                                       &total,
                                                       &passed,
                                                       &failed);
                break;
            case NGCC_TEST_KEM:
                kat_rc = ngcc_kem_correctness_kat_file(api,
                                                       opts->kat_path,
                                                       &total,
                                                       &passed,
                                                       &failed);
                break;
            case NGCC_TEST_KEX:
                kat_rc = ngcc_kex_correctness_kat_file(api,
                                                       opts->kat_path,
                                                       &total,
                                                       &passed,
                                                       &failed);
                break;
            default:
                kat_rc = -1;
                break;
        }

        if (kat_rc == 0 || kat_rc < 0) {
            printf("[%s][correctness] %s total=%llu passed=%llu failed=%llu source=kat\n",
                   name,
                   kat_rc == 0 ? "PASS" : "FAIL",
                   total,
                   passed,
                   failed);
            if (report != NULL) {
                report->correctness_status = (kat_rc == 0) ? STATUS_PASS : STATUS_FAIL;
                report->kat_used = 1;
                report->kat_total = total;
                report->kat_passed = passed;
                report->kat_failed = failed;
            }
            return kat_rc;
        }

        printf("[%s][correctness] KAT_NO_VECTOR fallback=random\n", name);
    }

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
    if (report != NULL) {
        report->correctness_status = (rc == 0) ? STATUS_PASS : STATUS_FAIL;
    }
    return rc;
}

static int run_performance_for_test(const ngcc_api_t *api,
                                    ngcc_test_kind_t kind,
                                    const char *name,
                                    const cli_options_t *opts,
                                    test_report_t *report) {
    ngcc_perf_config_t cfg;
    ngcc_perf_result_t result;
    int rc;

    cfg.iterations = opts->iterations;
    cfg.cycles_enabled = opts->cycles_enabled;
    cfg.bytes_per_op = 0;

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
        if (report != NULL) {
            report->performance_status = STATUS_FAIL;
        }
        return rc;
    }

    printf("[%s][performance] ops=%llu warmup=%llu elapsed_ms=%.3f total_ms=%.3f ops/s=%.3f bytes/op=%.3f bytes/s=%.3f%s\n",
           name,
           result.iterations,
           result.warmup_iterations,
           result.elapsed_ms,
           result.total_time_ms,
           result.ops_per_sec,
           result.bytes_per_op,
           result.bytes_per_sec,
           result.cycles_available ? "" : " cycles=unavailable");
    printf("[%s][performance][time] min_ms=%.6f mean_ms=%.6f median_ms=%.6f max_ms=%.6f stddev_ms=%.6f cv=%.3f%%\n",
           name,
           result.time_ms_min,
           result.time_ms_mean,
           result.time_ms_median,
           result.time_ms_max,
           result.time_ms_stddev,
           result.time_ms_cv_percent);
    if (result.cycles_available) {
        printf("[%s][performance][cycles] min=%.3f mean=%.3f median=%.3f max=%.3f stddev=%.3f cv=%.3f%%\n",
               name,
               result.cycles_min,
               result.cycles_per_op,
               result.cycles_median,
               result.cycles_max,
               result.cycles_stddev,
               result.cycles_cv_percent);
    }

    if (report != NULL) {
        report->performance = result;
        report->performance_status = STATUS_PASS;
    }
    return 0;
}

static int run_stability_for_test(const ngcc_api_t *api,
                                  ngcc_test_kind_t kind,
                                  const char *name,
                                  const cli_options_t *opts,
                                  test_report_t *report) {
    ngcc_stability_result_t result;
    int rc;
    int unstable_fail = 0;

    rc = ngcc_run_stability(api,
                            kind,
                            opts->digest_len_bits,
                            opts->msg_len,
                            opts->cycles_enabled,
                            opts->stability_sample_ms,
                            opts->duration_hours,
                            opts->stability_max_cases,
                            &opts->stability_thresholds,
                            &result);

    if (rc == 0) {
        const char *status = result.interrupted ? "STOPPED" : result.status;
        if (!result.interrupted && strcmp(result.status, "UNSTABLE") == 0) {
            unstable_fail = 1;
        }
        printf("[%s][stability] %s cases=%llu elapsed_s=%.3f\n",
               name,
               status,
               result.cases_run,
               result.elapsed_seconds);
        printf("[%s][stability][throughput] mean=%.3f stddev=%.3f cv=%.3f%% min=%.3f max=%.3f\n",
               name,
               result.throughput_mean_ops,
               result.throughput_stddev_ops,
               result.throughput_cv_percent,
               result.throughput_min_ops,
               result.throughput_max_ops);
        printf("[%s][stability][throughput_bytes] mean=%.3f stddev=%.3f cv=%.3f%% min=%.3f max=%.3f bytes/case=%.3f\n",
               name,
               result.throughput_mean_bytes,
               result.throughput_stddev_bytes,
               result.throughput_cv_percent_bytes,
               result.throughput_min_bytes,
               result.throughput_max_bytes,
               result.bytes_per_case);
        if (result.cycles_available) {
            printf("[%s][stability][cycles] mean=%.3f stddev=%.3f cv=%.3f%% min=%.3f max=%.3f\n",
                   name,
                   result.cycles_mean,
                   result.cycles_stddev,
                   result.cycles_cv_percent,
                   result.cycles_min,
                   result.cycles_max);
        } else {
            printf("[%s][stability][cycles] unavailable\n", name);
        }
        printf("[%s][stability][time] mean_ms=%.6f stddev_ms=%.6f cv=%.3f%% min_ms=%.6f max_ms=%.6f\n",
               name,
               result.time_mean_ms,
               result.time_stddev_ms,
               result.time_cv_percent,
               result.time_min_ms,
               result.time_max_ms);
        printf("[%s][stability][memory] start=%llu end=%llu min=%llu max=%llu growth=%.3f%%\n",
               name,
               (unsigned long long) result.memory_start_bytes,
               (unsigned long long) result.memory_end_bytes,
               (unsigned long long) result.memory_min_bytes,
               (unsigned long long) result.memory_max_bytes,
               result.memory_growth_percent);
        printf("[%s][stability][errors] total=%llu failed=%llu rate=%.6f%% status=%s\n",
               name,
               result.total_executions,
               result.error_count,
               result.error_rate_percent,
               result.status);
        if (result.failure_reasons[0] != '\0') {
            printf("[%s][stability][reason] %s\n", name, result.failure_reasons);
        }
        if (report != NULL) {
            report->stability = result;
            if (result.interrupted) {
                report->stability_status = STATUS_STOPPED;
            } else if (unstable_fail) {
                report->stability_status = STATUS_FAIL;
            } else {
                report->stability_status = STATUS_PASS;
            }
        }
    } else {
        printf("[%s][stability] FAIL cases=%llu elapsed_s=%.3f\n",
               name,
               result.cases_run,
               result.elapsed_seconds);
        if (report != NULL) {
            report->stability = result;
            report->stability_status = STATUS_FAIL;
        }
    }

    if (rc == 0 && unstable_fail) {
        return -1;
    }
    return rc;
}

static int run_memory_mode(const ngcc_api_t *api,
                           const cli_options_t *opts,
                           run_report_t *report) {
    uint64_t baseline_bytes;
    uint64_t peak_bytes;
    size_t i;
    int failed = 0;

    baseline_bytes = ngcc_mem_current_rss_bytes();

    for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
        if ((opts->test_mask & k_tests[i].mask) == 0) {
            continue;
        }
        if (run_correctness_for_test(api, k_tests[i].kind, k_tests[i].name, opts, NULL) != 0) {
            failed = 1;
        }
    }

    peak_bytes = ngcc_mem_peak_rss_bytes();
    printf("[memory] baseline_bytes=%llu peak_bytes=%llu\n",
           (unsigned long long) baseline_bytes,
           (unsigned long long) peak_bytes);

    report->memory_baseline_bytes = baseline_bytes;
    report->memory_peak_bytes = peak_bytes;
    report->memory_status = failed ? STATUS_FAIL : STATUS_PASS;
    return failed ? -1 : 0;
}

int main(int argc, char **argv) {
    cli_options_t opts;
    ngcc_library_t lib;
    run_report_t report;
    size_t i;
    int failed = 0;
    char interactive_lib[1024];
    char interactive_kat[1024];
    char interactive_json[1024];

    init_default_options(&opts);
    memset(&report, 0, sizeof(report));
    memset(interactive_lib, 0, sizeof(interactive_lib));
    memset(interactive_kat, 0, sizeof(interactive_kat));
    memset(interactive_json, 0, sizeof(interactive_json));

    for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
        report.tests[i].name = k_tests[i].name;
        report.tests[i].correctness_status = STATUS_SKIPPED;
        report.tests[i].performance_status = STATUS_SKIPPED;
        report.tests[i].stability_status = STATUS_SKIPPED;
    }
    report.memory_status = STATUS_SKIPPED;

    if (argc == 1) {
        if (run_interactive_setup(&opts,
                                  interactive_lib,
                                  sizeof(interactive_lib),
                                  interactive_kat,
                                  sizeof(interactive_kat),
                                  interactive_json,
                                  sizeof(interactive_json)) != 0) {
            return 1;
        }
    } else {
        int parse_rc = parse_cli_options(argc, argv, &opts);
        if (parse_rc > 0) {
            return 0;
        }
        if (parse_rc < 0) {
            return 1;
        }
    }

    if (validate_options(&opts) != 0) {
        if (argc != 1) {
            print_usage(argv[0]);
        }
        return 1;
    }

    if (ngcc_load_library(opts.lib_path, &lib) != 0) {
        fprintf(stderr, "error: failed to load library: %s\n", opts.lib_path);
        return 1;
    }

    for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
        report.tests[i].selected = ((opts.test_mask & k_tests[i].mask) != 0);
    }

    if (opts.mode_mask & MODE_MASK_CORRECTNESS) {
        for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
            if ((opts.test_mask & k_tests[i].mask) == 0) {
                continue;
            }
            if (run_correctness_for_test(&lib.api, k_tests[i].kind, k_tests[i].name, &opts, &report.tests[i]) != 0) {
                failed = 1;
            }
        }
    }

    if (opts.mode_mask & MODE_MASK_PERFORMANCE) {
        for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
            if ((opts.test_mask & k_tests[i].mask) == 0) {
                continue;
            }
            if (run_performance_for_test(&lib.api, k_tests[i].kind, k_tests[i].name, &opts, &report.tests[i]) != 0) {
                failed = 1;
            }
        }
    }

    if (opts.mode_mask & MODE_MASK_MEMORY) {
        if (run_memory_mode(&lib.api, &opts, &report) != 0) {
            failed = 1;
        }
    }

    if (opts.mode_mask & MODE_MASK_STABILITY) {
        for (i = 0; i < sizeof(k_tests) / sizeof(k_tests[0]); ++i) {
            if ((opts.test_mask & k_tests[i].mask) == 0) {
                continue;
            }
            if (run_stability_for_test(&lib.api, k_tests[i].kind, k_tests[i].name, &opts, &report.tests[i]) != 0) {
                failed = 1;
            }
        }
    }

    if (opts.json_out_path != NULL) {
        if (write_json_report(&opts, &report, failed) != 0) {
            failed = 1;
        }
    }

    ngcc_unload_library(&lib);
    return failed ? 1 : 0;
}
