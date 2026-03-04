#include <stdio.h>
#include <string.h>
#include <time.h>

#include "json_report.h"

/* ── JSON writer helpers ──────────────────────────────────────── */

typedef struct {
    FILE *fp;
    int indent;
    int needs_comma;
} json_writer_t;

static void jw_init(json_writer_t *w, FILE *fp) {
    w->fp = fp;
    w->indent = 0;
    w->needs_comma = 0;
}

static void jw_indent(json_writer_t *w) {
    int i;
    for (i = 0; i < w->indent; ++i) {
        fputs("  ", w->fp);
    }
}

static void jw_comma(json_writer_t *w) {
    if (w->needs_comma) {
        fputs(",\n", w->fp);
    }
    w->needs_comma = 1;
}

static void jw_write_escaped(FILE *fp, const char *s) {
    const unsigned char *p = (const unsigned char *) s;
    fputc('"', fp);
    if (p != NULL) {
        while (*p != '\0') {
            switch (*p) {
                case '\\': fputs("\\\\", fp); break;
                case '"':  fputs("\\\"", fp); break;
                case '\n': fputs("\\n", fp);  break;
                case '\r': fputs("\\r", fp);  break;
                case '\t': fputs("\\t", fp);  break;
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

static void jw_begin_object(json_writer_t *w, const char *key) {
    jw_comma(w);
    jw_indent(w);
    if (key != NULL) {
        jw_write_escaped(w->fp, key);
        fputs(": {\n", w->fp);
    } else {
        fputs("{\n", w->fp);
    }
    w->indent++;
    w->needs_comma = 0;
}

static void jw_end_object(json_writer_t *w) {
    w->indent--;
    fputc('\n', w->fp);
    jw_indent(w);
    fputc('}', w->fp);
    w->needs_comma = 1;
}

static void jw_key_str(json_writer_t *w, const char *key, const char *val) {
    jw_comma(w);
    jw_indent(w);
    jw_write_escaped(w->fp, key);
    fputs(": ", w->fp);
    jw_write_escaped(w->fp, val);
}

static void jw_key_null(json_writer_t *w, const char *key) {
    jw_comma(w);
    jw_indent(w);
    jw_write_escaped(w->fp, key);
    fputs(": null", w->fp);
}

static void jw_key_double(json_writer_t *w, const char *key, double val) {
    jw_comma(w);
    jw_indent(w);
    jw_write_escaped(w->fp, key);
    fprintf(w->fp, ": %.6f", val);
}

static void jw_key_llu(json_writer_t *w, const char *key, unsigned long long val) {
    jw_comma(w);
    jw_indent(w);
    jw_write_escaped(w->fp, key);
    fprintf(w->fp, ": %llu", val);
}

static void jw_key_int(json_writer_t *w, const char *key, int val) {
    jw_comma(w);
    jw_indent(w);
    jw_write_escaped(w->fp, key);
    fprintf(w->fp, ": %d", val);
}

static void jw_key_bool(json_writer_t *w, const char *key, int val) {
    jw_comma(w);
    jw_indent(w);
    jw_write_escaped(w->fp, key);
    fprintf(w->fp, ": %s", val ? "true" : "false");
}

/* ── Status helper ────────────────────────────────────────────── */

static const char *status_to_text(run_status_t status) {
    switch (status) {
        case STATUS_PASS:    return "PASS";
        case STATUS_FAIL:    return "FAIL";
        case STATUS_STOPPED: return "STOPPED";
        case STATUS_SKIPPED:
        default:             return "SKIPPED";
    }
}

/* ── Public API ────────────────────────────────────────────────── */

int write_json_report(const cli_options_t *opts,
                      const run_report_t *report,
                      int overall_failed) {
    FILE *fp;
    time_t now;
    struct tm tm_now;
    char timestamp[64];
    size_t i;
    json_writer_t w;

    if (opts == NULL || report == NULL || opts->json_out_path == NULL) {
        return 0;
    }

    fp = fopen(opts->json_out_path, "w");
    if (fp == NULL) {
        fprintf(stderr, "[ERROR][report] failed to open json report: %s\n", opts->json_out_path);
        return -1;
    }

    now = time(NULL);
    if (localtime_r(&now, &tm_now) == NULL) {
        memset(&tm_now, 0, sizeof(tm_now));
    }
    if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S%z", &tm_now) == 0) {
        strcpy(timestamp, "unknown");
    }

    jw_init(&w, fp);
    jw_begin_object(&w, NULL);

    jw_key_int(&w, "schema_version", 3);
    jw_key_str(&w, "timestamp", timestamp);
    jw_key_str(&w, "library", opts->lib_path);

    /* options */
    jw_begin_object(&w, "options");
    jw_key_llu(&w, "test_mask", opts->test_mask);
    jw_key_llu(&w, "mode_mask", opts->mode_mask);
    jw_key_llu(&w, "iterations", opts->iterations);
    jw_key_double(&w, "duration_hours", opts->duration_hours);
    jw_key_llu(&w, "stability_max_cases", opts->stability_max_cases);
    jw_key_double(&w, "stability_sample_ms", opts->stability_sample_ms);
    jw_key_llu(&w, "msg_len", (unsigned long long) opts->msg_len);
    jw_key_int(&w, "digest_len_bits", opts->digest_len_bits);
    jw_key_str(&w, "cycles", opts->cycles_enabled ? "on" : "off");

    jw_begin_object(&w, "stability_thresholds");
    jw_key_double(&w, "stable_throughput_cv_percent", opts->stability_thresholds.stable_throughput_cv_percent);
    jw_key_double(&w, "stable_cycles_cv_percent", opts->stability_thresholds.stable_cycles_cv_percent);
    jw_key_double(&w, "stable_time_cv_percent", opts->stability_thresholds.stable_time_cv_percent);
    jw_key_double(&w, "stable_memory_growth_percent", opts->stability_thresholds.stable_memory_growth_percent);
    jw_key_double(&w, "stable_error_rate_percent", opts->stability_thresholds.stable_error_rate_percent);
    jw_key_double(&w, "warning_throughput_cv_percent", opts->stability_thresholds.warning_throughput_cv_percent);
    jw_key_double(&w, "warning_cycles_cv_percent", opts->stability_thresholds.warning_cycles_cv_percent);
    jw_key_double(&w, "warning_time_cv_percent", opts->stability_thresholds.warning_time_cv_percent);
    jw_key_double(&w, "warning_memory_growth_percent", opts->stability_thresholds.warning_memory_growth_percent);
    jw_key_double(&w, "warning_error_rate_percent", opts->stability_thresholds.warning_error_rate_percent);
    jw_end_object(&w); /* stability_thresholds */

    if (opts->kat_path != NULL) {
        jw_key_str(&w, "kat", opts->kat_path);
    } else {
        jw_key_null(&w, "kat");
    }
    jw_end_object(&w); /* options */

    /* tests */
    jw_begin_object(&w, "tests");
    for (i = 0; i < sizeof(report->tests) / sizeof(report->tests[0]); ++i) {
        const test_report_t *test = &report->tests[i];

        jw_begin_object(&w, test->name);
        jw_key_bool(&w, "selected", test->selected);
        jw_key_str(&w, "correctness", status_to_text(test->correctness_status));
        jw_key_str(&w, "performance", status_to_text(test->performance_status));
        jw_key_str(&w, "stability", status_to_text(test->stability_status));

        /* kat */
        if (test->kat_used) {
            jw_begin_object(&w, "kat");
            jw_key_llu(&w, "total", test->kat_total);
            jw_key_llu(&w, "passed", test->kat_passed);
            jw_key_llu(&w, "failed", test->kat_failed);
            jw_end_object(&w);
        } else {
            jw_key_null(&w, "kat");
        }

        /* performance_metrics */
        if (test->performance_status == STATUS_PASS) {
            jw_begin_object(&w, "performance_metrics");
            /* basic config */
            jw_key_llu(&w, "iterations", test->performance.iterations);
            jw_key_llu(&w, "warmup_iterations", test->performance.warmup_iterations);
            jw_key_double(&w, "elapsed_ms", test->performance.elapsed_ms);
            jw_key_double(&w, "bytes_per_op", test->performance.bytes_per_op);
            /* cpu cycles */
            jw_key_double(&w, "cycles_per_op", test->performance.cycles_per_op);
            jw_key_double(&w, "cycles_stddev", test->performance.cycles_stddev);
            jw_key_double(&w, "cycles_min", test->performance.cycles_min);
            jw_key_double(&w, "cycles_max", test->performance.cycles_max);
            jw_key_double(&w, "cycles_median", test->performance.cycles_median);
            jw_key_double(&w, "cycles_per_byte", test->performance.cycles_per_byte);
            jw_key_double(&w, "cycles_cv_percent", test->performance.cycles_cv_percent);
            /* throughput */
            jw_key_double(&w, "ops_per_sec", test->performance.ops_per_sec);
            jw_key_double(&w, "bytes_per_sec", test->performance.bytes_per_sec);
            /* time */
            jw_key_double(&w, "time_ms_mean", test->performance.time_ms_mean);
            jw_key_double(&w, "time_ms_median", test->performance.time_ms_median);
            jw_key_double(&w, "time_ms_stddev", test->performance.time_ms_stddev);
            jw_key_double(&w, "time_ms_cv_percent", test->performance.time_ms_cv_percent);
            jw_end_object(&w);
        } else {
            jw_key_null(&w, "performance_metrics");
        }

        /* stability_metrics */
        if (test->stability_status != STATUS_SKIPPED) {
            jw_begin_object(&w, "stability_metrics");
            jw_key_llu(&w, "cases_run", test->stability.cases_run);
            jw_key_double(&w, "elapsed_seconds", test->stability.elapsed_seconds);
            jw_key_bool(&w, "interrupted", test->stability.interrupted);
            jw_key_bool(&w, "failed", test->stability.failed);
            jw_key_str(&w, "status", test->stability.status);
            jw_key_double(&w, "throughput_mean_ops", test->stability.throughput_mean_ops);
            jw_key_double(&w, "throughput_stddev_ops", test->stability.throughput_stddev_ops);
            jw_key_double(&w, "throughput_cv_percent", test->stability.throughput_cv_percent);
            jw_key_double(&w, "throughput_min_ops", test->stability.throughput_min_ops);
            jw_key_double(&w, "throughput_max_ops", test->stability.throughput_max_ops);
            jw_key_double(&w, "throughput_mean_bytes", test->stability.throughput_mean_bytes);
            jw_key_double(&w, "throughput_stddev_bytes", test->stability.throughput_stddev_bytes);
            jw_key_double(&w, "throughput_cv_percent_bytes", test->stability.throughput_cv_percent_bytes);
            jw_key_double(&w, "throughput_min_bytes", test->stability.throughput_min_bytes);
            jw_key_double(&w, "throughput_max_bytes", test->stability.throughput_max_bytes);
            jw_key_double(&w, "bytes_per_case", test->stability.bytes_per_case);
            jw_key_bool(&w, "cycles_available", test->stability.cycles_available);
            jw_key_double(&w, "cycles_mean", test->stability.cycles_mean);
            jw_key_double(&w, "cycles_stddev", test->stability.cycles_stddev);
            jw_key_double(&w, "cycles_cv_percent", test->stability.cycles_cv_percent);
            jw_key_double(&w, "cycles_min", test->stability.cycles_min);
            jw_key_double(&w, "cycles_max", test->stability.cycles_max);
            jw_key_double(&w, "time_mean_ms", test->stability.time_mean_ms);
            jw_key_double(&w, "time_stddev_ms", test->stability.time_stddev_ms);
            jw_key_double(&w, "time_cv_percent", test->stability.time_cv_percent);
            jw_key_double(&w, "time_min_ms", test->stability.time_min_ms);
            jw_key_double(&w, "time_max_ms", test->stability.time_max_ms);
            jw_key_llu(&w, "memory_start_bytes", (unsigned long long) test->stability.memory_start_bytes);
            jw_key_llu(&w, "memory_end_bytes", (unsigned long long) test->stability.memory_end_bytes);
            jw_key_llu(&w, "memory_min_bytes", (unsigned long long) test->stability.memory_min_bytes);
            jw_key_llu(&w, "memory_max_bytes", (unsigned long long) test->stability.memory_max_bytes);
            jw_key_llu(&w, "memory_peak_rss_bytes", (unsigned long long) test->stability.memory_peak_rss_bytes);
            jw_key_double(&w, "memory_growth_percent", test->stability.memory_growth_percent);
            jw_key_llu(&w, "total_executions", test->stability.total_executions);
            jw_key_llu(&w, "error_count", test->stability.error_count);
            jw_key_double(&w, "error_rate_percent", test->stability.error_rate_percent);
            jw_key_bool(&w, "performance_stable", test->stability.performance_stable);
            jw_key_bool(&w, "memory_stable", test->stability.memory_stable);
            jw_key_bool(&w, "correctness_stable", test->stability.correctness_stable);
            jw_key_bool(&w, "is_stable", test->stability.is_stable);
            jw_key_str(&w, "failure_reasons", test->stability.failure_reasons);
            jw_end_object(&w);
        } else {
            jw_key_null(&w, "stability_metrics");
        }

        jw_end_object(&w); /* test */
    }
    jw_end_object(&w); /* tests */

    /* memory */
    jw_begin_object(&w, "memory");
    jw_key_str(&w, "status", status_to_text(report->memory_status));

    jw_begin_object(&w, "static_mem");
    jw_key_llu(&w, "text_bytes", (unsigned long long) report->static_mem.text_size);
    jw_key_llu(&w, "data_bytes", (unsigned long long) report->static_mem.data_size);
    jw_key_llu(&w, "bss_bytes", (unsigned long long) report->static_mem.bss_size);
    jw_key_llu(&w, "rodata_bytes", (unsigned long long) report->static_mem.rodata_size);
    jw_key_llu(&w, "total_bytes", (unsigned long long) report->static_mem.total);
    jw_end_object(&w); /* static_mem */

    jw_key_llu(&w, "heap_baseline_bytes", (unsigned long long) report->memory_heap_baseline_bytes);
    jw_key_llu(&w, "heap_peak_bytes", (unsigned long long) report->memory_heap_peak_bytes);
    jw_end_object(&w);

    /* overall */
    jw_begin_object(&w, "overall");
    jw_key_str(&w, "status", overall_failed ? "FAIL" : "PASS");
    jw_end_object(&w);

    jw_end_object(&w); /* root */
    fputc('\n', fp);

    fclose(fp);
    printf("[report] json=%s\n", opts->json_out_path);
    return 0;
}
