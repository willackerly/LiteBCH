# Unit Tests
add_executable(unit_tests unit_tests.cpp)
target_link_libraries(unit_tests PRIVATE litebch::litebch)
add_test(NAME unit_tests COMMAND unit_tests)

# Legacy API Compatibility Test
add_executable(legacy_compat_test legacy_compat_test.cpp)
target_link_libraries(legacy_compat_test PRIVATE litebch::litebch)

# Kernel Comparison Benchmark
add_library(kernel_bch STATIC external/kernel_bch/bch_codec.c)
target_include_directories(kernel_bch PRIVATE external/kernel_bch)

add_executable(kernel_bench kernel_bench.cpp)
target_include_directories(kernel_bench PRIVATE external/kernel_bch)
target_link_libraries(kernel_bench PRIVATE litebch::litebch kernel_bch)

# Fast Benchmark
add_executable(fast_bench fast_bench.cpp)
target_link_libraries(fast_bench PRIVATE litebch::litebch)

# WASM Verification Tool
add_executable(verify_wasm_cpp_match verify_wasm_cpp_match.cpp)
target_link_libraries(verify_wasm_cpp_match PRIVATE litebch::litebch)

# Repro Demo Failure
add_executable(repro_demo_failure repro_demo_failure.cpp)
target_link_libraries(repro_demo_failure PRIVATE litebch::litebch)

# Comprehensive Decoder Test
add_executable(comprehensive_test comprehensive_test.cpp)
target_link_libraries(comprehensive_test PRIVATE litebch::litebch)

# Standard Benchmark
add_executable(benchmark benchmark.cpp)
target_link_libraries(benchmark PRIVATE litebch::litebch)

# Validated Regression Supertest
add_executable(supertest supertest.cpp)
target_link_libraries(supertest PRIVATE litebch::litebch)


# Unified Test Suite (Supertest + Performance + WASM Ready)
add_executable(unified_test unified_test.cpp)
target_link_libraries(unified_test PRIVATE litebch::litebch)

# --- AFF3CT Optional Verification ---
option(ENABLE_AFF3CT_TESTS "Enable AFF3CT verification in comprehensive_test" ON)

if(ENABLE_AFF3CT_TESTS)
    # Fix for missing standard headers on some macOS environments
    set(OLD_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    if(APPLE)
        execute_process(COMMAND xcrun --show-sdk-path 
                        OUTPUT_VARIABLE OSX_SDK_PATH 
                        OUTPUT_STRIP_TRAILING_WHITESPACE)
        message(STATUS "Integrating AFF3CT with SDK Path: ${OSX_SDK_PATH}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -I${OSX_SDK_PATH}/usr/include/c++/v1")
    endif()

    include(FetchContent)
    FetchContent_Declare(
        aff3ct
        GIT_REPOSITORY https://github.com/aff3ct/aff3ct.git
        GIT_TAG        master
    )
    # AFF3CT options to minimize build time/size
    set(AFF3CT_COMPILE_EXE OFF CACHE INTERNAL "")
    set(AFF3CT_COMPILE_STATIC_LIB ON CACHE INTERNAL "")
    set(AFF3CT_COMPILE_SHARED_LIB OFF CACHE INTERNAL "")
    
    FetchContent_MakeAvailable(aff3ct)
    
    # Restore flags for comprehensive_test to avoid conflicts
    set(CMAKE_CXX_FLAGS "${OLD_CXX_FLAGS}")

    target_compile_definitions(comprehensive_test PRIVATE AFF3CT_ENABLED)
    target_link_libraries(comprehensive_test PRIVATE aff3ct-static-lib)
    target_include_directories(comprehensive_test PRIVATE ${aff3ct_SOURCE_DIR}/include ${aff3ct_SOURCE_DIR}/lib/cli/src ${aff3ct_SOURCE_DIR}/lib/rang/include ${aff3ct_SOURCE_DIR}/lib/MIPP/src)
endif()

