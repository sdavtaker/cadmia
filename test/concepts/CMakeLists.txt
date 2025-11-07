# SPDX-License-Identifier: BSD-2-Clause
# Concept compile-time checks (pass and fail) built as separate targets

# Explicitly list concept check sources
set(CONCEPT_PASS_SOURCES
    iadevs_trivial_model_concept_check_pass_test.cpp
)

set(CONCEPT_FAIL_SOURCES
    iadevs_missing_external_transition_fails_test.cpp
    iadevs_missing_internal_transition_fails_test.cpp
    iadevs_missing_output_fails_test.cpp
    iadevs_missing_state_type_fails_test.cpp
    iadevs_missing_time_advance_fails_test.cpp
)

# Targets that should compile successfully
foreach(test_src IN LISTS CONCEPT_PASS_SOURCES)
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    set_target_properties(${test_name} PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
    )
    target_link_libraries(${test_name} PRIVATE cadmia::cadmia)
    add_test(
        NAME ${test_name}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${test_name}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endforeach()

# Targets that must fail to compile
foreach(test_src IN LISTS CONCEPT_FAIL_SOURCES)
    get_filename_component(test_name ${test_src} NAME_WE)
    add_executable(${test_name} ${test_src})
    set_target_properties(${test_name} PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
    )
    target_link_libraries(${test_name} PRIVATE cadmia::cadmia)
    add_test(
        NAME ${test_name}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${test_name}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
    set_tests_properties(${test_name} PROPERTIES WILL_FAIL TRUE)
endforeach()
