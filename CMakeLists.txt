# Integrate vcpkg if present
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

cmake_minimum_required(VERSION 3.28)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "Minimum OS X deployment version")
project(
    cadmium
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

set(Boost_USE_MULTITHREADED ON)
find_package(Boost 1.88.0 COMPONENTS thread REQUIRED) 

find_package(Catch2 CONFIG REQUIRED)

# Adding OpenMP as dependency
find_package(OpenMP)
if (OPENMP_FOUND)
    message(STATUS "OpenMP found")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

add_library(cadmium INTERFACE)
add_library(cadmium::cadmium ALIAS cadmium)

target_include_directories(cadmium INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        )

target_compile_features(cadmium INTERFACE cxx_std_23)

enable_testing()
#run all tests for sequential and concurrent versions
set(EXEC_TYPES "seq" "conc" "par")
foreach (exec_type ${EXEC_TYPES})
    # Unit tests
    FILE(GLOB TestSources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} test/*_test.cpp)
    foreach (testSrc ${TestSources})
        get_filename_component(testName ${testSrc} NAME_WE)
        set(testName "${testName}_${exec_type}")
        if ( testName MATCHES "dynamic.*coordinator" OR testName MATCHES "dynamic.*runner")
            message(STATUS "Skipping test ${testName} for parallel execution")
            continue()
        endif()
        add_executable(${testName} ${testSrc})
        if (exec_type STREQUAL "conc")
            target_compile_definitions(${testName} PUBLIC CADMIUM_EXECUTE_CONCURRENT BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION BOOST_THREAD_PROVIDES_EXECUTORS BOOST_THREAD_USES_MOVE)
        endif ()
        if (exec_type STREQUAL "par" AND OPENMP_FOUND)
            target_compile_definitions(${testName} PUBLIC CPU_PARALLEL)
            target_link_libraries(${testName} PUBLIC OpenMP::OpenMP_CXX)
        endif ()
        target_link_libraries(${testName} PUBLIC cadmium Catch2::Catch2WithMain Boost::thread)
        add_test(${testName} ${testName})
    endforeach (testSrc)

    # Tests that should compile
    FILE(GLOB TestCompileSources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} test-compile/compiles/*_test.cpp)
    foreach (testCompSrc ${TestCompileSources})
        get_filename_component(testCompName ${testCompSrc} NAME_WE)
        set(testCompName "${testCompName}_${exec_type}")
        add_executable(${testCompName} ${testCompSrc})
        set_target_properties(${testCompName} PROPERTIES
                EXCLUDE_FROM_ALL TRUE
                EXCLUDE_FROM_DEFAULT_BUILD TRUE)
        if (exec_type STREQUAL "conc")
            target_compile_definitions(${testCompName} PUBLIC CADMIUM_EXECUTE_CONCURRENT BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION BOOST_THREAD_PROVIDES_EXECUTORS BOOST_THREAD_USES_MOVE)
        endif ()
        if (exec_type STREQUAL "par" AND OPENMP_FOUND)
            target_compile_definitions(${testCompName} PUBLIC CPU_PARALLEL)
            target_link_libraries(${testName} PUBLIC OpenMP::OpenMP_CXX)
        endif ()
        target_include_directories(${testCompName} PUBLIC cadmium ${Boost_INCLUDE_DIRS} test-compile/compiles)
        target_link_libraries(${testCompName} PUBLIC cadmium)
        add_test(NAME ${testCompName}
                COMMAND ${CMAKE_COMMAND} --build . --target ${testCompName}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
    endforeach (testCompSrc)

    # Tests that should fail compilation
    FILE(GLOB TestCompileFailSources RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} test-compile/compile-fails/*_test.cpp)
    foreach (testCmpFailSrc ${TestCompileFailSources})
        get_filename_component(testCmpFailName ${testCmpFailSrc} NAME_WE)
        set(testCmpFailName "${testCmpFailName}_${exec_type}")
        add_executable(${testCmpFailName} ${testCmpFailSrc})
        set_target_properties(${testCmpFailName} PROPERTIES
                EXCLUDE_FROM_ALL TRUE
                EXCLUDE_FROM_DEFAULT_BUILD TRUE)
        if (exec_type STREQUAL "conc")
            target_compile_definitions(${testCmpFailName} PUBLIC CADMIUM_EXECUTE_CONCURRENT BOOST_THREAD_PROVIDES_FUTURE_CONTINUATION BOOST_THREAD_PROVIDES_EXECUTORS BOOST_THREAD_USES_MOVE)
        endif ()
        if (exec_type STREQUAL "par" AND OPENMP_FOUND)
            target_compile_definitions(${testCmpFailName} PUBLIC CPU_PARALLEL)
            target_link_libraries(${testName} PUBLIC OpenMP::OpenMP_CXX)
        endif ()
        target_include_directories(${testCmpFailName} PUBLIC cadmium ${Boost_INCLUDE_DIRS} test-compile/compiles)
        target_link_libraries(${testCmpFailName} PUBLIC cadmium)
        add_test(NAME ${testCmpFailName}
                COMMAND ${CMAKE_COMMAND} --build . --target ${testCmpFailName}
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
        set_tests_properties(${testCmpFailName} PROPERTIES WILL_FAIL TRUE)
    endforeach (testCmpFailSrc)
endforeach (exec_type)

# PDEVS Examples
FILE(GLOB Examples RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} example/pdevs/main-*.cpp)
foreach (exampleSrc ${Examples})
    get_filename_component(exampleName ${exampleSrc} NAME_WE)
    add_executable(${exampleName} ${exampleSrc})
    target_link_libraries(${exampleName} PUBLIC cadmium Boost::thread)
    target_include_directories(${exampleName} PUBLIC ${Boost_INCLUDE_DIRS})
endforeach (exampleSrc)

#CMake packaging to allow other projects to include cadmium
message("packaging cadmium to be consumed")
include(GNUInstallDirs)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/cadmium/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/cadmium)


include(CMakePackageConfigHelpers)

install(TARGETS cadmium EXPORT cadmium)

install(EXPORT cadmium
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/cadmium"
        NAMESPACE cadmium::
        FILE cadmiumConfig.cmake
        )

write_basic_package_version_file(
    ${CMAKE_BINARY_DIR}/cadmiumConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion)

install(
    FILES
    ${CMAKE_BINARY_DIR}/cadmiumConfigVersion.cmake
    DESTINATION
    ${CMAKE_INSTALL_LIBDIR}/cmake/cadmium)
