# SPDX-License-Identifier: BSD-2-Clause
# Unit tests using Catch2

# Explicitly list test sources (no globbing)
set(TEST_SOURCES
    iadevs_generator_model_test.cpp
    iadevs_processor_model_test.cpp
    logger_test.cpp
    modeling_decimal_test.cpp
    modeling_interval_test.cpp
    tuple_to_ostream_test.cpp
)

foreach(test_src IN LISTS TEST_SOURCES)
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    
    target_link_libraries(${test_name} 
        PRIVATE 
            cadmia::cadmia 
            Catch2::Catch2WithMain
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
