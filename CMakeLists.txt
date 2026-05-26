# SPDX-License-Identifier: BSD-2-Clause
# CadmIA - IA-DEVS Simulation Library

# Integrate vcpkg if present
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

cmake_minimum_required(VERSION 3.28)

project(CadmIA
    VERSION 0.3.0
    DESCRIPTION "A modern C++23 header-only library for IA-DEVS simulation"
    LANGUAGES CXX
)

# Only set policies and defaults if this is the main project
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Use modern C++ standards
    set(CMAKE_CXX_STANDARD 23 CACHE STRING "C++ standard")
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    
    # macOS deployment target
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum OS X deployment version")
    
    # Generate compile_commands.json for tooling
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    
    # Use folder organization in IDEs
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

# ============================================================================
# Project Options
# ============================================================================

option(PROFILER_ENABLED "Enable profiling instrumentation" OFF)
option(CADMIA_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})
option(CADMIA_BUILD_EXAMPLES "Build examples" ${PROJECT_IS_TOP_LEVEL})

# ============================================================================
# Dependencies
# ============================================================================

# Catch2 for testing
if(CADMIA_BUILD_TESTS)
    find_package(Catch2 3 CONFIG REQUIRED)
endif()

# ============================================================================
# Main Library Target
# ============================================================================

add_library(cadmia INTERFACE)
add_library(cadmia::cadmia ALIAS cadmia)

target_include_directories(cadmia INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_features(cadmia INTERFACE cxx_std_23)

# Compiler warnings and flags (only for targets linking to cadmia)
target_compile_options(cadmia INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall -Wextra -pedantic>
    $<$<AND:$<CXX_COMPILER_ID:GNU,Clang,AppleClang>,$<CONFIG:Debug>>:-g -O0>
    # Required for correct directed-rounding interval arithmetic (fesetround must not be
    # folded away by the optimizer) and to keep signed overflow well-defined.
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-frounding-math -fno-unsafe-math-optimizations -fno-strict-overflow>
)

# Profiling instrumentation (if enabled)
if(PROFILER_ENABLED)
    message(STATUS "Profiling instrumentation enabled")
    target_compile_options(cadmia INTERFACE
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fprofile-instr-generate>
    )
    target_link_options(cadmia INTERFACE
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-fprofile-instr-generate>
    )
endif()

# ============================================================================
# Testing
# ============================================================================

if(CADMIA_BUILD_TESTS)
    include(CTest)
    enable_testing()
    
    # Load Catch2's CTest integration module for v3
    # Prefer the package-provided module path
    if(DEFINED Catch2_DIR)
        list(APPEND CMAKE_MODULE_PATH "${Catch2_DIR}")
    endif()

    # Try the straightforward include first
    include(Catch2 OPTIONAL RESULT_VARIABLE _CATCH2_INC_RES)

    # If not found, try a set of likely paths directly
    if(NOT _CATCH2_INC_RES)
        foreach(_cand IN ITEMS
            "${Catch2_DIR}/Catch2.cmake"
            "${Catch2_DIR}/extras/Catch2.cmake"
            "${Catch2_DIR}/../lib/cmake/Catch2/Catch2.cmake"
            "${Catch2_DIR}/../share/catch2/Catch2.cmake"
            "${Catch2_DIR}/Catch.cmake"
            "${Catch2_DIR}/extras/Catch.cmake"
            "${Catch2_DIR}/../lib/cmake/Catch2/Catch.cmake"
            "${Catch2_DIR}/../share/catch2/Catch.cmake"
        )
            if(EXISTS "${_cand}")
                include("${_cand}")
                break()
            endif()
        endforeach()
    endif()
    
    # Code coverage for Debug builds (Unix only, excluding macOS due to issues)
    if(CMAKE_BUILD_TYPE MATCHES Debug AND UNIX AND NOT APPLE)
        target_compile_options(cadmia INTERFACE
            $<$<CONFIG:Debug>:-ftest-coverage -fprofile-arcs>
        )
        target_link_options(cadmia INTERFACE
            $<$<CONFIG:Debug>:-ftest-coverage -fprofile-arcs>
        )
    endif()
    
    # Add test subdirectories (unit and concepts)
    add_subdirectory(test)
endif()

# ============================================================================
# Examples
# ============================================================================

if(CADMIA_BUILD_EXAMPLES)
    # Add examples subdirectory when it has content
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/example/CMakeLists.txt")
        add_subdirectory(example)
    endif()
endif()

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)

# Install header files
install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/cadmia/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cadmia
    FILES_MATCHING PATTERN "*.hpp"
)

# Install library target
install(
    TARGETS cadmia
    EXPORT cadmiaTargets
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Install export set
install(
    EXPORT cadmiaTargets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cadmia
    NAMESPACE cadmia::
    FILE cadmiaTargets.cmake
)

# Generate and install package configuration files
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cadmiaConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cadmiaConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cadmia
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cadmiaConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/cadmiaConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/cadmiaConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/cadmia
)

# ============================================================================
# Code Formatting (clang-format)
# ============================================================================

find_program(CLANG_FORMAT
    NAMES clang-format clang-format-18 clang-format-17 clang-format-16 clang-format-15
    DOC "Path to clang-format executable"
)

if(CLANG_FORMAT)
    # Explicitly list directories to format (no globbing at configure time)
    set(FORMAT_PATTERNS
        "${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/example/**/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/**/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/unit/**/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/test/concepts/**/*.cpp"
    )
    
    # Collect files to format
    file(GLOB_RECURSE CADMIUM_FORMAT_FILES 
        CONFIGURE_DEPENDS
        ${FORMAT_PATTERNS}
    )

    # Target: format - Apply formatting in-place
    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i -style=file ${CADMIUM_FORMAT_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Applying clang-format to project sources"
        VERBATIM
    )

    # Target: check-format - Verify formatting compliance
    add_custom_target(check-format
        COMMAND ${CLANG_FORMAT} -style=file --dry-run -Werror ${CADMIUM_FORMAT_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking clang-format compliance"
        VERBATIM
    )
else()
    message(STATUS "clang-format not found; 'format' and 'check-format' targets unavailable")
endif()

# ============================================================================
# Documentation (LaTeX) - Optional
# ============================================================================

find_program(PDFLATEX_EXECUTABLE
    NAMES pdflatex
    DOC "Path to pdflatex executable"
)

if(PDFLATEX_EXECUTABLE)
    set(DOC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/doc")
    set(DOC_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/doc")
    
    # Create output directory for PDF
    file(MAKE_DIRECTORY ${DOC_BINARY_DIR})
    
    # Target: build-doc - Compile LaTeX documentation
    add_custom_target(build-doc
        COMMAND ${PDFLATEX_EXECUTABLE} -output-directory=${DOC_BINARY_DIR} -interaction=nonstopmode main.tex
        COMMAND ${PDFLATEX_EXECUTABLE} -output-directory=${DOC_BINARY_DIR} -interaction=nonstopmode main.tex
        WORKING_DIRECTORY ${DOC_SOURCE_DIR}
        COMMENT "Building LaTeX documentation (main.pdf)"
        VERBATIM
    )
    
    message(STATUS "LaTeX found; 'build-doc' target available")
else()
    message(STATUS "pdflatex not found; 'build-doc' target unavailable (optional)")
endif()

