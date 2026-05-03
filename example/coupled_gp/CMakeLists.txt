# SPDX-License-Identifier: BSD-2-Clause
# Example: generator feeding a processor via a typed translation

add_executable(coupled_gp main.cpp)
target_link_libraries(coupled_gp PRIVATE cadmia::cadmia)
