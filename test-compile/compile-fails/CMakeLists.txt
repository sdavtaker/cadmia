# SPDX-License-Identifier: BSD-2-Clause
# Compile-time tests that should fail to compile

# Explicitly list sources that should fail compilation (no globbing)
set(COMPILE_FAIL_TEST_SOURCES
    iadevs_missing_external_transition_fails_test.cpp
    iadevs_missing_internal_transition_fails_test.cpp
    iadevs_missing_output_fails_test.cpp
    iadevs_missing_state_type_fails_test.cpp
    iadevs_missing_time_advance_fails_test.cpp
)

foreach(test_src IN LISTS COMPILE_FAIL_TEST_SOURCES)
    get_filename_component(test_name ${test_src} NAME_WE)
    
    add_executable(${test_name} ${test_src})
    
    set_target_properties(${test_name} PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
    )
    
    target_include_directories(${test_name} 
        PRIVATE 
            ${CMAKE_CURRENT_SOURCE_DIR}/../compiles
    )
    
    target_link_libraries(${test_name} 
        PRIVATE 
            cadmia::cadmia
    )
    
    # Test passes if the build fails
    add_test(
        NAME ${test_name}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${test_name}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    
    set_tests_properties(${test_name} PROPERTIES 
        WILL_FAIL TRUE
    )
endforeach()
