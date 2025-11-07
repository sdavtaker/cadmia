# SPDX-License-Identifier: BSD-2-Clause
# Compile-time tests that should fail to compile

# Explicitly list sources that should fail compilation (no globbing)
set(COMPILE_FAIL_TEST_SOURCES
    iadevs_missing_external_transition_fails_test.cpp
    iadevs_missing_internal_transition_fails_test.cpp
    iadevs_missing_output_fails_test.cpp
    iadevs_missing_state_type_fails_test.cpp
    iadevs_missing_time_advance_fails_test.cpp
    pdevs_coordinator_of_no_coupled_fails_compile_test.cpp
    pdevs_failure_to_map_types_in_EIC_fails_compile_test.cpp
    pdevs_failure_to_map_types_in_EOC_fails_compile_test.cpp
    pdevs_failure_to_map_types_in_IC_fails_compile_test.cpp
    pdevs_inports_not_a_tuple_in_atomic_model_fail_compile_test.cpp
    pdevs_inports_not_a_tuple_in_coupled_model_fail_compile_test.cpp
    pdevs_missing_atomic_model_simulator_fails_compile_test.cpp
    pdevs_missing_confluence_transition_fails_compile_test.cpp
    pdevs_missing_external_transition_fails_compile_test.cpp
    pdevs_missing_input_ports_in_atomic_fails_compile_test.cpp
    pdevs_missing_internal_transition_fails_compile_test.cpp
    pdevs_missing_output_function_fails_compile_test.cpp
    pdevs_missing_output_ports_fails_compile_test.cpp
    pdevs_missing_state_type_fails_compile_test.cpp
    pdevs_missing_state_variable_fails_compile_test.cpp
    pdevs_missing_time_advance_function_fails_compile_test.cpp
    pdevs_outports_not_a_tuple_in_atomic_model_fail_compile_test.cpp
    pdevs_outports_not_a_tuple_in_coupled_model_fail_compile_test.cpp
    pdevs_repeated_input_ports_fail_compile_test.cpp
    pdevs_repeated_output_ports_fail_compile_test.cpp
    pdevs_submodel_is_not_a_model_fails_compile_test.cpp
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
