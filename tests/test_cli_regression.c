#define _POSIX_C_SOURCE 200809L

#include "test_support.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond, fmt, ...)                                                     \
    do {                                                                          \
        if (!(cond)) {                                                            \
            fprintf(stderr, "FAIL: " fmt "\n", ##__VA_ARGS__);                    \
            return 1;                                                             \
        }                                                                         \
    } while (0)

static int run_and_expect(char *const argv[],
                          int expected_exit,
                          const char *needle1,
                          const char *needle2,
                          const char *needle3) {
    test_command_result_t result;

    CHECK(test_run_command(argv, &result) == 0, "command execution failed");
    if (result.exit_code != expected_exit) {
        fprintf(stderr, "%s\n", result.output);
        test_free_command_result(&result);
        CHECK(0, "unexpected exit code %d", expected_exit);
    }
    if (needle1 != NULL && !test_output_contains(result.output, needle1)) {
        fprintf(stderr, "%s\n", result.output);
        test_free_command_result(&result);
        CHECK(0, "missing output: %s", needle1);
    }
    if (needle2 != NULL && !test_output_contains(result.output, needle2)) {
        fprintf(stderr, "%s\n", result.output);
        test_free_command_result(&result);
        CHECK(0, "missing output: %s", needle2);
    }
    if (needle3 != NULL && !test_output_contains(result.output, needle3)) {
        fprintf(stderr, "%s\n", result.output);
        test_free_command_result(&result);
        CHECK(0, "missing output: %s", needle3);
    }
    test_free_command_result(&result);
    return 0;
}

int main(int argc, char **argv) {
    char tmp_dir[] = "/tmp/ngcc_cli_regression.XXXXXX";
    char kat_path[PATH_MAX];
    char json_path[PATH_MAX];
    char baseline_status_path[PATH_MAX];
    char candidate_status_path[PATH_MAX];
    char candidate_perf_path[PATH_MAX];
    const char *validator_path = "../tests/validate_json_report.py";
    const char *schema_path = "../docs/json_schema_v4.json";
    const char *compare_script_path = "../tests/compare_stability_reports.py";
    const char *kat_content =
        "# compatibility: empty msg, md alias, comments, metadata and 0x prefix\n"
        "COUNT = 0\n"
        "MLEN = 0\n"
        "INPUT =\n"
        "MD = 08\n"
        "\n"
        "; signature aliases\n"
        "COUNT = 1\n"
        "PUBLICKEY = 01\n"
        "MESSAGE = aa\n"
        "SIGNATURE = 03\n"
        "\n"
        "// kem aliases + optional separators\n"
        "COUNT = 2\n"
        "SECRETKEY = 0x02\n"
        "CIPHERTEXT = 08\n"
        "SHAREDSECRET = 07\n"
        "\n"
        "; kex A-side aliases\n"
        "COUNT = 3\n"
        "SK_A = 02\n"
        "PK_B = 01\n"
        "PASS2 = 22\n"
        "STATEA = 03\n"
        "SHAREDSECRETA = 09\n"
        "\n"
        "; kex B-side aliases\n"
        "COUNT = 4\n"
        "SK_B = 02\n"
        "PK_A = 01\n"
        "PASS3 = 33\n"
        "STATEB = 03\n"
        "SHAREDSECRETB = 09\n";
    const char *baseline_compare_json =
        "{\n"
        "  \"tests\": {\n"
        "    \"hash\": {\"stability\": \"PASS\", \"stability_metrics\": {\"status\": \"STABLE\", \"throughput_cv_percent\": 1.0, \"time_cv_percent\": 1.0, \"cycles_cv_percent\": 1.0, \"throughput_mean_ops\": 100.0, \"time_mean_ms\": 1.0, \"cycles_mean\": 10.0, \"memory_growth_percent\": 0.0, \"error_rate_percent\": 0.0}},\n"
        "    \"dsa\": {\"stability\": \"PASS\", \"stability_metrics\": {\"status\": \"STABLE\", \"throughput_cv_percent\": 1.0, \"time_cv_percent\": 1.0, \"cycles_cv_percent\": 1.0, \"throughput_mean_ops\": 100.0, \"time_mean_ms\": 1.0, \"cycles_mean\": 10.0, \"memory_growth_percent\": 0.0, \"error_rate_percent\": 0.0}},\n"
        "    \"kem\": {\"stability\": \"PASS\", \"stability_metrics\": {\"status\": \"STABLE\", \"throughput_cv_percent\": 1.0, \"time_cv_percent\": 1.0, \"cycles_cv_percent\": 1.0, \"throughput_mean_ops\": 100.0, \"time_mean_ms\": 1.0, \"cycles_mean\": 10.0, \"memory_growth_percent\": 0.0, \"error_rate_percent\": 0.0}},\n"
        "    \"kex\": {\"stability\": \"PASS\", \"stability_metrics\": {\"status\": \"STABLE\", \"throughput_cv_percent\": 1.0, \"time_cv_percent\": 1.0, \"cycles_cv_percent\": 1.0, \"throughput_mean_ops\": 100.0, \"time_mean_ms\": 1.0, \"cycles_mean\": 10.0, \"memory_growth_percent\": 0.0, \"error_rate_percent\": 0.0}}\n"
        "  }\n"
        "}\n";
    const char *candidate_status_compare_json =
        "{\n"
        "  \"tests\": {\n"
        "    \"hash\": {\"stability\": \"PASS\", \"stability_metrics\": {\"status\": \"WARNING\", \"throughput_cv_percent\": 1.0, \"time_cv_percent\": 1.0, \"cycles_cv_percent\": 1.0, \"throughput_mean_ops\": 100.0, \"time_mean_ms\": 1.0, \"cycles_mean\": 10.0, \"memory_growth_percent\": 0.0, \"error_rate_percent\": 0.0}},\n"
        "    \"dsa\": {\"stability\": \"PASS\", \"stability_metrics\": {\"status\": \"STABLE\", \"throughput_cv_percent\": 1.0, \"time_cv_percent\": 1.0, \"cycles_cv_percent\": 1.0, \"throughput_mean_ops\": 100.0, \"time_mean_ms\": 1.0, \"cycles_mean\": 10.0, \"memory_growth_percent\": 0.0, \"error_rate_percent\": 0.0}},\n"
        "    \"kem\": {\"stability\": \"PASS\", \"stability_metrics\": {\"status\": \"STABLE\", \"throughput_cv_percent\": 1.0, \"time_cv_percent\": 1.0, \"cycles_cv_percent\": 1.0, \"throughput_mean_ops\": 100.0, \"time_mean_ms\": 1.0, \"cycles_mean\": 10.0, \"memory_growth_percent\": 0.0, \"error_rate_percent\": 0.0}},\n"
        "    \"kex\": {\"stability\": \"PASS\", \"stability_metrics\": {\"status\": \"STABLE\", \"throughput_cv_percent\": 1.0, \"time_cv_percent\": 1.0, \"cycles_cv_percent\": 1.0, \"throughput_mean_ops\": 100.0, \"time_mean_ms\": 1.0, \"cycles_mean\": 10.0, \"memory_growth_percent\": 0.0, \"error_rate_percent\": 0.0}}\n"
        "  }\n"
        "}\n";
    const char *candidate_perf_compare_json =
        "{\n"
        "  \"tests\": {\n"
        "    \"hash\": {\"stability\": \"PASS\", \"stability_metrics\": {\"status\": \"STABLE\", \"throughput_cv_percent\": 1.0, \"time_cv_percent\": 1.0, \"cycles_cv_percent\": 1.0, \"throughput_mean_ops\": 80.0, \"time_mean_ms\": 1.0, \"cycles_mean\": 10.0, \"memory_growth_percent\": 0.0, \"error_rate_percent\": 0.0}},\n"
        "    \"dsa\": {\"stability\": \"PASS\", \"stability_metrics\": {\"status\": \"STABLE\", \"throughput_cv_percent\": 1.0, \"time_cv_percent\": 1.0, \"cycles_cv_percent\": 1.0, \"throughput_mean_ops\": 100.0, \"time_mean_ms\": 1.0, \"cycles_mean\": 10.0, \"memory_growth_percent\": 0.0, \"error_rate_percent\": 0.0}},\n"
        "    \"kem\": {\"stability\": \"PASS\", \"stability_metrics\": {\"status\": \"STABLE\", \"throughput_cv_percent\": 1.0, \"time_cv_percent\": 1.0, \"cycles_cv_percent\": 1.0, \"throughput_mean_ops\": 100.0, \"time_mean_ms\": 1.0, \"cycles_mean\": 10.0, \"memory_growth_percent\": 0.0, \"error_rate_percent\": 0.0}},\n"
        "    \"kex\": {\"stability\": \"PASS\", \"stability_metrics\": {\"status\": \"STABLE\", \"throughput_cv_percent\": 1.0, \"time_cv_percent\": 1.0, \"cycles_cv_percent\": 1.0, \"throughput_mean_ops\": 100.0, \"time_mean_ms\": 1.0, \"cycles_mean\": 10.0, \"memory_growth_percent\": 0.0, \"error_rate_percent\": 0.0}}\n"
        "  }\n"
        "}\n";

    CHECK(argc == 4, "usage: test_cli_regression BENCH MOCK_NGCC MOCK_HASH_ONLY");
    CHECK(test_make_temp_dir(tmp_dir) == 0, "failed to create temp dir");

    CHECK(snprintf(kat_path, sizeof(kat_path), "%s/vectors.kat", tmp_dir) < (int) sizeof(kat_path),
          "kat path too long");
    CHECK(snprintf(json_path, sizeof(json_path), "%s/report.json", tmp_dir) < (int) sizeof(json_path),
          "json path too long");
    CHECK(snprintf(baseline_status_path, sizeof(baseline_status_path), "%s/baseline_status.json", tmp_dir) < (int) sizeof(baseline_status_path),
          "baseline status path too long");
    CHECK(snprintf(candidate_status_path, sizeof(candidate_status_path), "%s/candidate_status.json", tmp_dir) < (int) sizeof(candidate_status_path),
          "candidate status path too long");
    CHECK(snprintf(candidate_perf_path, sizeof(candidate_perf_path), "%s/candidate_perf.json", tmp_dir) < (int) sizeof(candidate_perf_path),
          "candidate perf path too long");
    CHECK(test_write_file(kat_path, kat_content) == 0, "failed to write kat file");
    CHECK(test_write_file(baseline_status_path, baseline_compare_json) == 0, "failed to write baseline compare json");
    CHECK(test_write_file(candidate_status_path, candidate_status_compare_json) == 0, "failed to write candidate status compare json");
    CHECK(test_write_file(candidate_perf_path, candidate_perf_compare_json) == 0, "failed to write candidate perf compare json");

    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "all", "--mode", "correctness",
            "--digest-len-bits", "8", "--kat", kat_path, NULL
        };
        CHECK(run_and_expect(cmd, 0,
                             "[hash][correctness] PASS",
                             "[dsa][correctness] PASS",
                             "[kem][correctness] PASS") == 0,
              "correctness regression failed");
    }

    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "hash", "--mode", "performance",
            "--digest-len-bits", "8", "--msg-len", "64", "--iterations", "64",
            "--cycles", "off", NULL
        };
        CHECK(run_and_expect(cmd, 0,
                             "[hash][performance] ops=",
                             "bytes/s=",
                             "bytes/op=") == 0,
              "performance regression failed");
    }

    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "kem", "--mode", "stability",
            "--stability-sample-ms", "1",
            "--stable-throughput-cv-percent", "6",
            "--warning-throughput-cv-percent", "5", NULL
        };
        CHECK(run_and_expect(cmd, 1,
                             "warning thresholds must be >= stable thresholds",
                             NULL,
                             NULL) == 0,
              "threshold validation regression failed");
    }

    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "kem", "--mode", "stability",
            "--stability-sample-ms", "0", NULL
        };
        CHECK(run_and_expect(cmd, 1,
                             "invalid --stability-sample-ms value",
                             NULL,
                             NULL) == 0,
              "sample window validation regression failed");
    }

    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "kem", "--mode", "stability",
            "--duration-hours", "0.0001",
            "--stability-max-cases", "128",
            "--stability-sample-ms", "0.5",
            "--cycles", "off",
            "--stable-throughput-cv-percent", "100",
            "--stable-cycles-cv-percent", "100",
            "--stable-time-cv-percent", "100",
            "--stable-memory-growth-percent", "100",
            "--warning-throughput-cv-percent", "120",
            "--warning-cycles-cv-percent", "120",
            "--warning-time-cv-percent", "120",
            "--warning-memory-growth-percent", "120",
            "--json-out", json_path, NULL
        };
        CHECK(run_and_expect(cmd, 0,
                             "[kem][stability]",
                             "[kem][stability][throughput_bytes]",
                             NULL) == 0,
              "stability regression failed");
    }

    CHECK(test_file_contains(json_path, "\"schema_version\": 4"), "json missing schema version");
    CHECK(test_file_contains(json_path, "\"stability_sample_ms\": 0.500000"), "json missing sample window");
    CHECK(test_file_contains(json_path, "\"stability_thresholds\""), "json missing thresholds");
    CHECK(test_file_contains(json_path, "\"throughput_mean_bytes\":"), "json missing throughput metric");
    CHECK(test_file_contains(json_path, "\"sample_count\":"), "json missing sample count");
    CHECK(test_file_contains(json_path, "\"hash\""), "json missing hash node");
    CHECK(test_file_contains(json_path, "\"dsa\""), "json missing dsa node");
    CHECK(test_file_contains(json_path, "\"kem\""), "json missing kem node");
    CHECK(test_file_contains(json_path, "\"kex\""), "json missing kex node");

    {
        char *const cmd[] = {
            "/usr/bin/env", "python3", (char *) validator_path, json_path, (char *) schema_path, NULL
        };
        CHECK(run_and_expect(cmd, 0, NULL, NULL, NULL) == 0,
              "json validation regression failed");
    }

    {
        char *const cmd[] = {
            "/usr/bin/env", "python3", (char *) compare_script_path,
            baseline_status_path, candidate_status_path, NULL
        };
        CHECK(run_and_expect(cmd, 1,
                             "hash.status: candidate WARNING worse than baseline STABLE",
                             NULL,
                             NULL) == 0,
              "stability status compare regression failed");
    }

    {
        char *const cmd[] = {
            "/usr/bin/env", "python3", (char *) compare_script_path,
            baseline_status_path, candidate_perf_path, NULL
        };
        CHECK(run_and_expect(cmd, 1,
                             "hash.throughput_mean_ops:",
                             NULL,
                             NULL) == 0,
              "stability mean compare regression failed");
    }

    {
        char *const cmd[] = {
            argv[1], "--lib", argv[3], "--test", "hash", "--mode", "correctness",
            "--digest-len-bits", "8", NULL
        };
        CHECK(run_and_expect(cmd, 0,
                             "[hash][correctness] PASS",
                             NULL,
                             NULL) == 0,
              "hash-only correctness regression failed");
    }

    {
        char *const cmd[] = {
            argv[1], "--lib", argv[3], "--test", "dsa-keygen", "--mode", "correctness", NULL
        };
        CHECK(run_and_expect(cmd, 1,
                             "missing symbol",
                             NULL,
                             NULL) == 0,
              "hash-only loader regression failed");
    }

    CHECK(test_remove_tree(tmp_dir) == 0, "failed to remove temp dir");
    printf("cli regression passed\n");
    return 0;
}
