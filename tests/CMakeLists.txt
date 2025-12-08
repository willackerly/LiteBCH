# Unit Tests
add_executable(unit_tests unit_tests.cpp)
target_link_libraries(unit_tests PRIVATE litebch::litebch)
add_test(NAME unit_tests COMMAND unit_tests)


# Kernel Comparison Benchmark
add_library(kernel_bch STATIC external/kernel_bch/bch_codec.c)
target_include_directories(kernel_bch PRIVATE external/kernel_bch)



# Comprehensive Decoder Test
add_executable(comprehensive_test comprehensive_test.cpp)
target_link_libraries(comprehensive_test PRIVATE litebch::litebch)
if(EMSCRIPTEN)
    target_link_options(comprehensive_test PRIVATE 
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:-sINITIAL_MEMORY=64MB"
    )
endif()


# --- AFF3CT Optional Verification ---
option(ENABLE_AFF3CT_TESTS "Enable AFF3CT verification in comprehensive_test" OFF)

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

