cmake_minimum_required(VERSION 3.16)
project(ngcc_bench VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ═══════════════════════════════════════════════════════════════════
#  选手 .so 编译
# ═══════════════════════════════════════════════════════════════════
# 用法: cmake .. -DIMPL_TYPE=optimized   (默认 all)
# 可选值: reference | optimized | additional | all
set(IMPL_TYPE "all" CACHE STRING "Implementation type to build: reference|optimized|additional|all")

# 算法类别
set(ALGO_CATEGORIES API_BlockCipher API_CryptHash API_PKC)

# 实现类型 → 目录名映射
set(IMPL_DIR_reference  "Reference_Implementation")
set(IMPL_DIR_optimized  "Optimized_Implementation")
set(IMPL_DIR_additional "Additional_Implementation")

# 实现类型 → 库名后缀映射
set(IMPL_SUFFIX_reference  "ref")
set(IMPL_SUFFIX_optimized  "opt")
set(IMPL_SUFFIX_additional "add")

# 根据 IMPL_TYPE 决定编译哪些
if(IMPL_TYPE STREQUAL "all")
    set(IMPL_LIST reference optimized additional)
else()
    set(IMPL_LIST ${IMPL_TYPE})
endif()

message(STATUS "========================================")
message(STATUS "  IMPL_TYPE = ${IMPL_TYPE}")
message(STATUS "  Building: ${IMPL_LIST}")
message(STATUS "========================================")

# 遍历 算法类别 × 实现类型，自动发现源码并生成 .so
foreach(CATEGORY ${ALGO_CATEGORIES})
    foreach(IMPL ${IMPL_LIST})
        set(DIR_NAME ${IMPL_DIR_${IMPL}})
        set(SRC_DIR "${CMAKE_SOURCE_DIR}/code/${CATEGORY}/Implementations/${DIR_NAME}")

        if(EXISTS "${SRC_DIR}")
            # 收集目录下所有 .c 文件
            file(GLOB IMPL_SOURCES "${SRC_DIR}/*.c")

            if(IMPL_SOURCES)
                # 生成库名: e.g. BlockCipher_ref, CryptHash_opt
                string(REPLACE "API_" "" SHORT_NAME ${CATEGORY})
                set(SUFFIX ${IMPL_SUFFIX_${IMPL}})
                set(LIB_NAME "${SHORT_NAME}_${SUFFIX}")

                add_library(${LIB_NAME} SHARED ${IMPL_SOURCES})

                # 允许选手代码引用同目录下的头文件
                target_include_directories(${LIB_NAME} PRIVATE "${SRC_DIR}")

                # .so 输出到 build/lib/ 目录
                set_target_properties(${LIB_NAME} PROPERTIES
                    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
                )

                message(STATUS "  [选手库] ${LIB_NAME} <- ${SRC_DIR}")
            else()
                message(STATUS "  [跳过]   ${CATEGORY}/${DIR_NAME} (无 .c 文件)")
            endif()
        else()
            message(STATUS "  [跳过]   ${CATEGORY}/${DIR_NAME} (目录不存在)")
        endif()
    endforeach()
endforeach()

# ═══════════════════════════════════════════════════════════════════
#  测试框架编译
# ═══════════════════════════════════════════════════════════════════
set(TEST_SOURCES
    tests/src/main.c
    tests/src/algo_loader.c
    tests/src/correctness_test.c
    tests/src/kat_parser.c
    tests/src/performance_test.c
    tests/src/memory_test.c
    tests/src/stability_test.c
    tests/src/report.c
)

# 仅在源文件存在时才构建测试工具（初期开发可能还没有全部文件）
set(EXISTING_TEST_SOURCES "")
foreach(SRC ${TEST_SOURCES})
    if(EXISTS "${CMAKE_SOURCE_DIR}/${SRC}")
        list(APPEND EXISTING_TEST_SOURCES ${SRC})
    endif()
endforeach()

if(EXISTING_TEST_SOURCES)
    add_executable(ngcc_test_framework ${EXISTING_TEST_SOURCES})

    target_include_directories(ngcc_test_framework PRIVATE
        ${CMAKE_SOURCE_DIR}/tests/include
        ${CMAKE_SOURCE_DIR}/tests/utils
    )

    # dl: dlopen/dlsym  m: math
    target_link_libraries(ngcc_test_framework dl m)

    # 传递路径宏，供运行时定位 code/ 和 reports/
    target_compile_definitions(ngcc_test_framework PRIVATE
        CODE_DIR="${CMAKE_SOURCE_DIR}/code"
        REPORT_DIR="${CMAKE_SOURCE_DIR}/reports"
    )

    message(STATUS "  [测试工具] ngcc_test_framework <- tests/src/")
else()
    message(STATUS "  [跳过]   ngcc_test_framework (tests/src/ 下无源文件)")
endif()

# ═══════════════════════════════════════════════════════════════════
#  NGCC Benchmark 主程序 (ngcc_bench/)
# ═══════════════════════════════════════════════════════════════════

# ── Sanitizer 选项 ─────────────────────────────────────────────────
option(ENABLE_ASAN  "Enable AddressSanitizer"            OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer"   OFF)

if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
    message(STATUS "  [sanitizer] AddressSanitizer enabled")
endif()

if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined)
    add_link_options(-fsanitize=undefined)
    message(STATUS "  [sanitizer] UndefinedBehaviorSanitizer enabled")
endif()

# ── 共享库源文件（main.c 除外）──────────────────────────────────────
set(NGCC_BENCH_LIB_SOURCES
    ngcc_bench/src/cli_parser.c
    ngcc_bench/src/interactive.c
    ngcc_bench/src/json_report.c
    ngcc_bench/src/loader.c
    ngcc_bench/src/bench_core.c
    ngcc_bench/src/cycle_counter.c
    ngcc_bench/src/stats_util.c
    ngcc_bench/src/mem_stat.c
    ngcc_bench/src/bench_hash.c
    ngcc_bench/src/bench_sig.c
    ngcc_bench/src/bench_kem.c
    ngcc_bench/src/bench_kex.c
    ngcc_bench/src/stability.c
    ngcc_bench/src/kat_parser.c
)

add_executable(ngcc_bench ngcc_bench/src/main.c ${NGCC_BENCH_LIB_SOURCES})
target_include_directories(ngcc_bench PRIVATE
    ${CMAKE_SOURCE_DIR}/ngcc_bench/include
)
target_link_libraries(ngcc_bench dl m)
target_compile_options(ngcc_bench PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(ngcc_bench PRIVATE NGCC_VERSION="${PROJECT_VERSION}")

add_library(ngcc_test_support STATIC
    tests/test_support.c
)
target_include_directories(ngcc_test_support PRIVATE
    ${CMAKE_SOURCE_DIR}/tests
)
target_compile_options(ngcc_test_support PRIVATE -Wall -Wextra -Wpedantic)

add_library(mock_ngcc SHARED tests/mock/mock_ngcc.c)
add_library(mock_hash_only SHARED tests/mock/mock_hash_only.c)
add_library(mock_mlkem SHARED tests/mock/mock_mlkem.c)
add_library(mock_mldsa SHARED tests/mock/mock_mldsa.c)
add_library(mock_mlkex SHARED tests/mock/mock_mlkex.c)
foreach(MOCK_TARGET mock_ngcc mock_hash_only mock_mlkem mock_mldsa mock_mlkex)
    target_compile_options(${MOCK_TARGET} PRIVATE -Wall -Wextra -Wpedantic)
    set_target_properties(${MOCK_TARGET} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests/mock"
    )
endforeach()

add_executable(ngcc_cli_regression_test tests/test_cli_regression.c)
target_link_libraries(ngcc_cli_regression_test PRIVATE ngcc_test_support)
target_compile_options(ngcc_cli_regression_test PRIVATE -Wall -Wextra -Wpedantic)

add_executable(ngcc_mock_mlkem_test tests/test_mock_mlkem.c)
target_link_libraries(ngcc_mock_mlkem_test PRIVATE ngcc_test_support)
target_compile_options(ngcc_mock_mlkem_test PRIVATE -Wall -Wextra -Wpedantic)

add_executable(ngcc_mock_mldsa_test tests/test_mock_mldsa.c)
target_link_libraries(ngcc_mock_mldsa_test PRIVATE ngcc_test_support)
target_compile_options(ngcc_mock_mldsa_test PRIVATE -Wall -Wextra -Wpedantic)

add_executable(ngcc_mock_mlkex_test tests/test_mock_mlkex.c)
target_link_libraries(ngcc_mock_mlkex_test PRIVATE ngcc_test_support)
target_compile_options(ngcc_mock_mlkex_test PRIVATE -Wall -Wextra -Wpedantic)

add_executable(ngcc_stability_profile tests/ngcc_stability_profile.c)
target_link_libraries(ngcc_stability_profile PRIVATE ngcc_test_support)
target_compile_options(ngcc_stability_profile PRIVATE -Wall -Wextra -Wpedantic)
target_compile_definitions(ngcc_stability_profile PRIVATE
    NGCC_DEFAULT_MOCK_LIB="$<TARGET_FILE:mock_ngcc>"
)

enable_testing()
add_test(
    NAME ngcc_cli_regression
    COMMAND ngcc_cli_regression_test
            $<TARGET_FILE:ngcc_bench>
            $<TARGET_FILE:mock_ngcc>
            $<TARGET_FILE:mock_hash_only>
)
add_test(
    NAME ngcc_mock_mlkem
    COMMAND ngcc_mock_mlkem_test
            $<TARGET_FILE:ngcc_bench>
            $<TARGET_FILE:mock_mlkem>
)
add_test(
    NAME ngcc_mock_mldsa
    COMMAND ngcc_mock_mldsa_test
            $<TARGET_FILE:ngcc_bench>
            $<TARGET_FILE:mock_mldsa>
)
add_test(
    NAME ngcc_mock_mlkex
    COMMAND ngcc_mock_mlkex_test
            $<TARGET_FILE:ngcc_bench>
            $<TARGET_FILE:mock_mlkex>
)

# ── Unit tests ────────────────────────────────────────────────────
add_executable(ngcc_unit_tests tests/test_unit.c ${NGCC_BENCH_LIB_SOURCES})
target_include_directories(ngcc_unit_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/ngcc_bench/include
)
target_link_libraries(ngcc_unit_tests dl m)
target_compile_options(ngcc_unit_tests PRIVATE -Wall -Wextra -Wpedantic)

add_test(NAME ngcc_unit_tests COMMAND ngcc_unit_tests)

message(STATUS "  [主程序]   ngcc_bench <- ngcc_bench/src/")
