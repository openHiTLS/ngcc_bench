cmake_minimum_required(VERSION 3.16)
project(ngcc_bench C)

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
set(NGCC_BENCH_SOURCES
    ngcc_bench/src/main.c
    ngcc_bench/src/loader.c
    ngcc_bench/src/bench_core.c
    ngcc_bench/src/mem_stat.c
    ngcc_bench/src/bench_hash.c
    ngcc_bench/src/bench_sig.c
    ngcc_bench/src/bench_kem.c
    ngcc_bench/src/bench_kex.c
    ngcc_bench/src/stability.c
    ngcc_bench/src/kat_parser.c
)

add_executable(ngcc_bench ${NGCC_BENCH_SOURCES})
target_include_directories(ngcc_bench PRIVATE
    ${CMAKE_SOURCE_DIR}/ngcc_bench/include
)
target_link_libraries(ngcc_bench dl m)

enable_testing()
add_test(
    NAME ngcc_cli_regression
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/run_cli_regression.sh $<TARGET_FILE:ngcc_bench>
)

message(STATUS "  [主程序]   ngcc_bench <- ngcc_bench/src/")
