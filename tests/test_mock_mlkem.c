#define _POSIX_C_SOURCE 200809L

#include "test_support.h"

#include <stdio.h>

#define CHECK(cond, fmt, ...)                                                     \
    do {                                                                          \
        if (!(cond)) {                                                            \
            fprintf(stderr, "FAIL: " fmt "\n", ##__VA_ARGS__);                    \
            return 1;                                                             \
        }                                                                         \
    } while (0)

static int run_expect(char *const argv[],
                      const char *needle1,
                      const char *needle2,
                      const char *needle3) {
    test_command_result_t result;

    CHECK(test_run_command(argv, &result) == 0, "command execution failed");
    if (result.exit_code != 0 ||
        (needle1 != NULL && !test_output_contains(result.output, needle1)) ||
        (needle2 != NULL && !test_output_contains(result.output, needle2)) ||
        (needle3 != NULL && !test_output_contains(result.output, needle3))) {
        fprintf(stderr, "%s\n", result.output);
        test_free_command_result(&result);
        CHECK(0, "unexpected mock mlkem output");
    }
    test_free_command_result(&result);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3, "usage: test_mock_mlkem BENCH MOCK_LIB");

    {
        char *const cmd[] = {argv[1], "--lib", argv[2], "--test", "kem", "--mode", "correctness", NULL};
        CHECK(run_expect(cmd, "[kem][correctness] PASS", NULL, NULL) == 0, "aggregate correctness failed");
    }
    {
        char *const cmd[] = {argv[1], "--lib", argv[2], "--test", "kem-keygen", "--mode", "correctness", NULL};
        CHECK(run_expect(cmd, "[kem-keygen][correctness] PASS", NULL, NULL) == 0, "keygen correctness failed");
    }
    {
        char *const cmd[] = {argv[1], "--lib", argv[2], "--test", "kem-encap", "--mode", "correctness", NULL};
        CHECK(run_expect(cmd, "[kem-encap][correctness] PASS", NULL, NULL) == 0, "encap correctness failed");
    }
    {
        char *const cmd[] = {argv[1], "--lib", argv[2], "--test", "kem-decap", "--mode", "correctness", NULL};
        CHECK(run_expect(cmd, "[kem-decap][correctness] PASS", NULL, NULL) == 0, "decap correctness failed");
    }
    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "kem", "--mode", "performance",
            "--iterations", "1000", "--cycles", "off", NULL
        };
        CHECK(run_expect(cmd, "[kem][performance] ops=", "[kem][performance][throughput]", "[kem][performance][time]") == 0,
              "aggregate performance failed");
    }
    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "kem-keygen", "--mode", "performance",
            "--iterations", "1000", "--cycles", "off", NULL
        };
        CHECK(run_expect(cmd, "[kem-keygen][performance] ops=", "[kem-keygen][performance][throughput]",
                         "[kem-keygen][performance][time]") == 0,
              "keygen performance failed");
    }
    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "kem-encap", "--mode", "performance",
            "--iterations", "1000", "--cycles", "off", NULL
        };
        CHECK(run_expect(cmd, "[kem-encap][performance] ops=", "[kem-encap][performance][throughput]",
                         "[kem-encap][performance][time]") == 0,
              "encap performance failed");
    }
    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "kem-decap", "--mode", "performance",
            "--iterations", "1000", "--cycles", "off", NULL
        };
        CHECK(run_expect(cmd, "[kem-decap][performance] ops=", "[kem-decap][performance][throughput]",
                         "[kem-decap][performance][time]") == 0,
              "decap performance failed");
    }

    printf("mock mlkem regression passed\n");
    return 0;
}
