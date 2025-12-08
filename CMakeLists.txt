cmake_minimum_required(VERSION 3.14)
project(LiteBCH VERSION 1.0.0 LANGUAGES C CXX)

# Standard C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Library Code
add_library(litebch src/LiteBCH.cpp)
add_library(litebch::litebch ALIAS litebch)

target_include_directories(litebch PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

if(NOT EMSCRIPTEN)
    target_include_directories(litebch PUBLIC 
    # Fix for broken environment (Native Only)
    "/Library/Developer/CommandLineTools/SDKs/MacOSX15.2.sdk/usr/include/c++/v1"
    )
endif()

# Configuration Options
option(LITEBCH_MAX_PERFORMANCE "Enable maximum performance optimizations (-O3, unroll)" ON)
option(LITEBCH_ENABLE_SIMD "Enable SIMD instructions (-msimd128 for WASM, -march=native for host)" OFF)

if(LITEBCH_MAX_PERFORMANCE)
    add_compile_options(-O3 -funroll-loops)
endif()

if(LITEBCH_ENABLE_SIMD)
    if(EMSCRIPTEN)
        add_compile_options(-msimd128)
    else()
        add_compile_options(-march=native)
    endif()
endif()


# Compiler Options (Optimization by default for Release)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Installation (for standard make install)
include(GNUInstallDirs)
install(TARGETS litebch EXPORT LiteBCHTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# Testing & Examples
option(LITEBCH_BUILD_TESTS "Build LiteBCH tests" ON)
option(LITEBCH_BUILD_EXAMPLES "Build LiteBCH examples" ON)

if(LITEBCH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(LITEBCH_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# WASM Build
option(LITEBCH_BUILD_WASM "Build WASM bindings using Emscripten" OFF)

if(LITEBCH_BUILD_WASM)
    if(NOT EMSCRIPTEN)
        message(WARNING "LITEBCH_BUILD_WASM is ON but Emscripten toolchain not found. Build may fail.")
    endif()

    # Create the WASM module
    set(CPPTRACE_UNWIND_WITH_NOTHING ON CACHE BOOL "" FORCE)
    add_executable(litebch_wasm src/wasm_bindings.cpp)
    
    # Link with the main library
    # Note: When building with emcmake, the main library objects will be compiled 
    # compatible with WASM automatically.
    target_link_libraries(litebch_wasm PRIVATE litebch::litebch)

    # Emscripten Link Options
    # -lembind: Enable Embind
    # -s MODULARIZE=1: Output a factory function
    # -s EXPORT_NAME='createLiteBCHModule': Name of the factory
    target_link_options(litebch_wasm PRIVATE
        "SHELL:-lembind" 
        "SHELL:-sMODULARIZE=1"
        "SHELL:-sEXPORT_NAME='createLiteBCHModule'"
        "SHELL:-sALLOW_MEMORY_GROWTH=1"
        "SHELL:-fexceptions"
        "SHELL:-sDISABLE_EXCEPTION_CATCHING=0"
        "SHELL:-sEXPORTED_FUNCTIONS=['_malloc','_free']"
        "SHELL:-sEXPORTED_RUNTIME_METHODS=['HEAPU8']"
    )
    
    # Set output name to litebch.js (and litebch.wasm)
    set_target_properties(litebch_wasm PROPERTIES 
        OUTPUT_NAME "litebch"
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
endif()
