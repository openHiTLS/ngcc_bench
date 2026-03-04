#include <math.h>
#include <stdio.h>
#include <string.h>

#include "interactive.h"
#include "cli_parser.h"

/* ── Prompt helpers (file-local) ───────────────────────────────── */

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

/* ── Interactive setup ─────────────────────────────────────────── */

int run_interactive_setup(cli_options_t *opts,
                          char *lib_buf,
                          size_t lib_buf_len,
                          char *kat_buf,
                          size_t kat_buf_len,
                          char *json_buf,
                          size_t json_buf_len) {
    static const char *const test_items[] = {
        "hash", "dsa", "dsa-keygen", "dsa-sig", "dsa-verify",
        "kem", "kem-keygen", "kem-encap", "kem-decap", "kex", "all"
    };
    static const char *const mode_items[] = {"correctness", "performance", "memory", "stability", "all"};
    int test_choice;
    int mode_choice;
    double d_tmp;
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

    stability_selected = (opts->mode_mask & MODE_MASK_STABILITY) != 0;

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
