# SPDX-License-Identifier: BSD-2-Clause
# Unit tests using Catch2

# Explicitly list test sources (no globbing)
set(TEST_SOURCES
    engine_coordinator_test.cpp
    engine_root_coordinator_test.cpp
    iadevs_accumulator_model_test.cpp
    iadevs_counter_model_test.cpp
    iadevs_generator_model_test.cpp
    iadevs_processor_model_test.cpp
    iadevs_processor_simulator_test.cpp
    modeling_coupled_test.cpp
    modeling_decimal_test.cpp
    modeling_interval_test.cpp
)

foreach(test_src IN LISTS TEST_SOURCES)
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})

    target_link_libraries(${test_name}
        PRIVATE
            cadmia::cadmia
            Catch2::Catch2WithMain
    )

    # Directed-rounding flags are required for correct interval arithmetic in tests.
    # These are intentionally NOT on the cadmia INTERFACE target; modelers who use
    # floating-point intervals must add them to their own targets.
    target_compile_options(${test_name} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-frounding-math -fno-unsafe-math-optimizations -fno-strict-overflow>
    )

    # Auto-discover Catch2 TEST_CASEs and register them with CTest when available
    if(COMMAND catch_discover_tests)
        catch_discover_tests(${test_name}
            TEST_PREFIX "${test_name}."
            DISCOVERY_MODE POST_BUILD
        )
    else()
        message(WARNING "catch_discover_tests not available; using fallback for ${test_name}")
        add_test(NAME ${test_name} COMMAND ${test_name})
    endif()
endforeach()
