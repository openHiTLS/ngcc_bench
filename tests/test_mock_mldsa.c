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
        CHECK(0, "unexpected mock mldsa output");
    }
    test_free_command_result(&result);
    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc == 3, "usage: test_mock_mldsa BENCH MOCK_LIB");

    {
        char *const cmd[] = {argv[1], "--lib", argv[2], "--test", "dsa", "--mode", "correctness", "--msg-len", "64", NULL};
        CHECK(run_expect(cmd, "[dsa][correctness] PASS", NULL, NULL) == 0, "aggregate correctness failed");
    }
    {
        char *const cmd[] = {argv[1], "--lib", argv[2], "--test", "dsa-keygen", "--mode", "correctness", NULL};
        CHECK(run_expect(cmd, "[dsa-keygen][correctness] PASS", NULL, NULL) == 0, "keygen correctness failed");
    }
    {
        char *const cmd[] = {argv[1], "--lib", argv[2], "--test", "dsa-sig", "--mode", "correctness", "--msg-len", "64", NULL};
        CHECK(run_expect(cmd, "[dsa-sig][correctness] PASS", NULL, NULL) == 0, "sig correctness failed");
    }
    {
        char *const cmd[] = {argv[1], "--lib", argv[2], "--test", "dsa-verify", "--mode", "correctness", "--msg-len", "64", NULL};
        CHECK(run_expect(cmd, "[dsa-verify][correctness] PASS", NULL, NULL) == 0, "verify correctness failed");
    }
    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "dsa", "--mode", "performance",
            "--msg-len", "64", "--iterations", "1000", "--cycles", "off", NULL
        };
        CHECK(run_expect(cmd, "[dsa][performance] ops=", "[dsa][performance][throughput]", "[dsa][performance][time]") == 0,
              "aggregate performance failed");
    }
    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "dsa-keygen", "--mode", "performance",
            "--iterations", "1000", "--cycles", "off", NULL
        };
        CHECK(run_expect(cmd, "[dsa-keygen][performance] ops=", "[dsa-keygen][performance][throughput]",
                         "[dsa-keygen][performance][time]") == 0,
              "keygen performance failed");
    }
    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "dsa-sig", "--mode", "performance",
            "--msg-len", "64", "--iterations", "1000", "--cycles", "off", NULL
        };
        CHECK(run_expect(cmd, "[dsa-sig][performance] ops=", "[dsa-sig][performance][throughput]",
                         "[dsa-sig][performance][time]") == 0,
              "sig performance failed");
    }
    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "dsa-verify", "--mode", "performance",
            "--msg-len", "64", "--iterations", "1000", "--cycles", "off", NULL
        };
        CHECK(run_expect(cmd, "[dsa-verify][performance] ops=", "[dsa-verify][performance][throughput]",
                         "[dsa-verify][performance][time]") == 0,
              "verify performance failed");
    }

    printf("mock mldsa regression passed\n");
    return 0;
}
