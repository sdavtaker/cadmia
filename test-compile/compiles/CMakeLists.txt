# SPDX-License-Identifier: BSD-2-Clause
# Compile-time tests that should successfully compile

# Explicitly list sources that should compile (no globbing)
# Note: Tests that reference removed PDEVS basic models have been excluded
# Note: IA-DEVS concept checks are now done via static_assert in main test files
set(COMPILE_TEST_SOURCES
    iadevs_trivial_model_concept_check_test.cpp
)

foreach(test_src IN LISTS COMPILE_TEST_SOURCES)
    get_filename_component(test_name ${test_src} NAME_WE)
    
    add_executable(${test_name} ${test_src})
    
    set_target_properties(${test_name} PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        EXCLUDE_FROM_DEFAULT_BUILD TRUE
    )
    
    target_include_directories(${test_name} 
        PRIVATE 
            ${CMAKE_CURRENT_SOURCE_DIR}
    )
    
    target_link_libraries(${test_name} 
        PRIVATE 
            cadmia::cadmia
    )
    
    # Test passes if the build succeeds
    add_test(
        NAME ${test_name}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${test_name}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
endforeach()
