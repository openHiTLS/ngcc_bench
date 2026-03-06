#define _POSIX_C_SOURCE 200809L

#include "test_support.h"

#include <limits.h>
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
        CHECK(0, "unexpected mock mlkex output");
    }
    test_free_command_result(&result);
    return 0;
}

int main(int argc, char **argv) {
    char tmp_dir[] = "/tmp/ngcc_mock_mlkex.XXXXXX";
    char kat_path[PATH_MAX];
    const char *kat_content =
        "COUNT = 0\n"
        "SK_A = 02\n"
        "PK_B = 01\n"
        "PASS2 = 22\n"
        "STATEA = 03\n"
        "SHAREDSECRETA = 09\n"
        "\n"
        "COUNT = 1\n"
        "SK_B = 02\n"
        "PK_A = 01\n"
        "PASS3 = 33\n"
        "STATEB = 03\n"
        "SHAREDSECRETB = 09\n";

    CHECK(argc == 3, "usage: test_mock_mlkex BENCH MOCK_LIB");
    CHECK(test_make_temp_dir(tmp_dir) == 0, "failed to create temp dir");
    CHECK(snprintf(kat_path, sizeof(kat_path), "%s/kex_vectors.kat", tmp_dir) < (int) sizeof(kat_path),
          "kat path too long");
    CHECK(test_write_file(kat_path, kat_content) == 0, "failed to write kat file");

    {
        char *const cmd[] = {argv[1], "--lib", argv[2], "--test", "kex", "--mode", "correctness", NULL};
        CHECK(run_expect(cmd, "[kex][correctness] PASS", NULL, NULL) == 0, "correctness failed");
    }
    {
        char *const cmd[] = {
            argv[1], "--lib", argv[2], "--test", "kex", "--mode", "performance",
            "--iterations", "1000", "--cycles", "off", NULL
        };
        CHECK(run_expect(cmd, "[kex][performance] ops=", "[kex][performance][throughput]", "[kex][performance][time]") == 0,
              "performance failed");
    }
    {
        char *const cmd[] = {argv[1], "--lib", argv[2], "--test", "kex", "--mode", "correctness", "--kat", kat_path, NULL};
        CHECK(run_expect(cmd, "[kex][correctness] PASS total=2 passed=2 failed=0 source=kat", NULL, NULL) == 0,
              "kat correctness failed");
    }

    CHECK(test_remove_tree(tmp_dir) == 0, "failed to remove temp dir");
    printf("mock mlkex regression passed\n");
    return 0;
}
