#include <stdio.h>
#include <string.h>
#include <time.h>

#include "json_report.h"

/* ── Helpers (file-local) ──────────────────────────────────────── */

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

/* ── Public API ────────────────────────────────────────────────── */

int write_json_report(const cli_options_t *opts,
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
