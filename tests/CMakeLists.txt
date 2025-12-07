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
