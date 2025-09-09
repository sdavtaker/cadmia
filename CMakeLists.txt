# Integrate vcpkg if present
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

cmake_minimum_required(VERSION 3.28)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum OS X deployment version")
project(
    CadmIA
    VERSION 0.3.0)

# Enable C++23 standard globally
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pedantic -Wall")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")


#Enabling instrumentation for profiling if required
option(PROFILER_ENABLED "Profiler-enabled" OFF)
if (PROFILER_ENABLED)
    message(STATUS "Instrumenting profiling")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-instr-generate")
endif ()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake-modules")

set(JSON_BuildTests OFF CACHE INTERNAL "")

include(CheckCXXCompilerFlag)
IF (CMAKE_BUILD_TYPE MATCHES Debug)
    message("debug mode")
    IF (NOT UNIX)  # This produces hundreds of warnings in Mac (cannot merge previous GCDA file: corrupt arc tag)
        include(CodeCoverage)
        set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -ftest-coverage -fprofile-arcs")
    ENDIF ()
ENDIF (CMAKE_BUILD_TYPE MATCHES Debug)

find_package(Catch2 3 CONFIG REQUIRED)

add_library(cadmia INTERFACE)
add_library(cadmia::cadmia ALIAS cadmia)

target_include_directories(cadmia INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        )

target_compile_features(cadmia INTERFACE cxx_std_23)

include(CTest)
# Load Catch2's CTest integration module for v3 in a robust way
# Prefer the package-provided module path, but handle vcpkg and system installs.
if(DEFINED Catch2_DIR)
    list(APPEND CMAKE_MODULE_PATH "${Catch2_DIR}")
endif()

# Try the straightforward include first
include(Catch2 OPTIONAL RESULT_VARIABLE _CATCH2_INC_RES)

# If not found, try a set of likely paths directly
if(NOT _CATCH2_INC_RES)
    foreach(_cand
        "${Catch2_DIR}/Catch2.cmake"
        "${Catch2_DIR}/extras/Catch2.cmake"
        "${Catch2_DIR}/../lib/cmake/Catch2/Catch2.cmake"
        "${Catch2_DIR}/../share/catch2/Catch2.cmake"
        "${Catch2_DIR}/Catch.cmake"  # legacy name
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
enable_testing()

# Unit tests (Catch2 v3 discovery)
FILE(GLOB TestSources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} test/*_test.cpp)
foreach (testSrc ${TestSources})
    get_filename_component(testName ${testSrc} NAME_WE)
    add_executable(${testName} ${testSrc})
    target_link_libraries(${testName} PUBLIC cadmia Catch2::Catch2WithMain)
    # Auto-discover Catch2 TEST_CASEs and register them with CTest when available;
    # otherwise, register the binary as a single CTest entry.
    if(COMMAND catch_discover_tests)
        catch_discover_tests(${testName}
            TEST_PREFIX "${testName}."
            DISCOVERY_MODE POST_BUILD)
    else()
        message(WARNING "Catch2.cmake not found; falling back to single test entry for ${testName}.")
        add_test(NAME ${testName} COMMAND ${testName})
    endif()
endforeach ()

# Tests that should compile
FILE(GLOB TestCompileSources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} test-compile/compiles/*_test.cpp)
foreach (testCompSrc ${TestCompileSources})
    get_filename_component(testCompName ${testCompSrc} NAME_WE)
    add_executable(${testCompName} ${testCompSrc})
    set_target_properties(${testCompName} PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    target_include_directories(${testCompName} PUBLIC cadmia test-compile/compiles)
    target_link_libraries(${testCompName} PUBLIC cadmia)
    add_test(NAME ${testCompName}
            COMMAND ${CMAKE_COMMAND} --build . --target ${testCompName}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach ()

# Tests that should fail compilation
FILE(GLOB TestCompileFailSources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} test-compile/compile-fails/*_test.cpp)
foreach (testCmpFailSrc ${TestCompileFailSources})
    get_filename_component(testCmpFailName ${testCmpFailSrc} NAME_WE)
    add_executable(${testCmpFailName} ${testCmpFailSrc})
    set_target_properties(${testCmpFailName} PROPERTIES
            EXCLUDE_FROM_ALL TRUE
            EXCLUDE_FROM_DEFAULT_BUILD TRUE)
    target_include_directories(${testCmpFailName} PUBLIC cadmia test-compile/compiles)
    target_link_libraries(${testCmpFailName} PUBLIC cadmia)
    add_test(NAME ${testCmpFailName}
            COMMAND ${CMAKE_COMMAND} --build . --target ${testCmpFailName}
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    set_tests_properties(${testCmpFailName} PROPERTIES WILL_FAIL TRUE)
endforeach ()

# PDEVS Examples
FILE(GLOB Examples RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} example/pdevs/main-*.cpp)
foreach (exampleSrc ${Examples})
    get_filename_component(exampleName ${exampleSrc} NAME_WE)
    add_executable(${exampleName} ${exampleSrc})
    target_link_libraries(${exampleName} PUBLIC cadmia)
endforeach ()

#CMake packaging to allow other projects to include cadmia
message("packaging cadmia to be consumed")
include(GNUInstallDirs)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/cadmia/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cadmia)


include(CMakePackageConfigHelpers)

install(TARGETS cadmia EXPORT cadmia)

install(EXPORT cadmia
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/cadmia"
    NAMESPACE cadmia::
    FILE cadmiaConfig.cmake
        )

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/cadmiaConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion)

install(
    FILES
    ${CMAKE_BINARY_DIR}/cadmiaConfigVersion.cmake
    DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/cadmia)

# Formatting targets (clang-format)
# Creates:
#  - format: applies formatting in-place according to .clang-format
#  - check-format: verifies formatting (fails if changes would be made)
find_program(CLANG_FORMAT
    NAMES clang-format clang-format-18 clang-format-17 clang-format-16 clang-format-15
    DOC "Path to clang-format executable")

if (CLANG_FORMAT)
    # Collect candidate source files to format
    file(GLOB_RECURSE CADMIUM_FORMAT_FILES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/**/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/*.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/example/**/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test/**/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/test-compile/**/*.cpp
    )

    add_custom_target(format
        COMMAND ${CLANG_FORMAT} -i -style=file ${CADMIUM_FORMAT_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Applying clang-format to project sources"
        VERBATIM)

    add_custom_target(check-format
        COMMAND ${CLANG_FORMAT} -style=file --dry-run -Werror ${CADMIUM_FORMAT_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking clang-format compliance"
        VERBATIM)
else()
    message(STATUS "clang-format not found. 'format' and 'check-format' targets will be unavailable.")
endif()
