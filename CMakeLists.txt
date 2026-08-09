cmake_minimum_required(VERSION 3.16)
project(samskriti VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Sources ────────────────────────────────────────────────────
set(CORE_SOURCES
    src/soul.cpp
    src/bonds.cpp
    src/world.cpp
)

# ── Static library: libsamskriti.a ────────────────────────────
add_library(samskriti_static STATIC ${CORE_SOURCES})
target_include_directories(samskriti_static PUBLIC include)
set_target_properties(samskriti_static PROPERTIES OUTPUT_NAME samskriti)

# ── Shared library: libsamskriti.so / .dylib ──────────────────
add_library(samskriti_shared SHARED ${CORE_SOURCES})
target_include_directories(samskriti_shared PUBLIC include)
set_target_properties(samskriti_shared PROPERTIES OUTPUT_NAME samskriti)

# ── Compiler warnings ─────────────────────────────────────────
foreach(tgt samskriti_static samskriti_shared)
    target_compile_options(${tgt} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -Wpedantic>
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
    )
endforeach()

# ── Release optimisation ─────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ── Tests ─────────────────────────────────────────────────────
option(SAMSKRITI_BUILD_TESTS "Build test suite" ON)
if(SAMSKRITI_BUILD_TESTS)
    enable_testing()
    add_executable(samskriti_tests tests/test_samskriti.cpp ${CORE_SOURCES})
    target_include_directories(samskriti_tests PRIVATE include)
    target_compile_options(samskriti_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra>
    )
    add_test(NAME samskriti_tests COMMAND samskriti_tests)
endif()

# ── Benchmark ─────────────────────────────────────────────────
option(SAMSKRITI_BUILD_BENCH "Build performance benchmark" ON)
if(SAMSKRITI_BUILD_BENCH)
    add_executable(bench_performance tests/bench_performance.cpp ${CORE_SOURCES})
    target_include_directories(bench_performance PRIVATE include)
    target_compile_options(bench_performance PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-O3>
        $<$<CXX_COMPILER_ID:MSVC>:/O2>
    )
endif()

# ── Examples ──────────────────────────────────────────────────
option(SAMSKRITI_BUILD_EXAMPLES "Build examples" ON)
if(SAMSKRITI_BUILD_EXAMPLES)
    add_executable(minimal examples/minimal.cpp ${CORE_SOURCES})
    target_include_directories(minimal PRIVATE include)
endif()

# ── Install ───────────────────────────────────────────────────
install(TARGETS samskriti_static samskriti_shared
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)
install(FILES include/samskriti.h DESTINATION include)
