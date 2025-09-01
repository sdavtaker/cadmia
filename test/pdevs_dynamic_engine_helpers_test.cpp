// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2017-2025, Laouen M. L. Belloli, Damian Vicino
 * Carleton University, Universidad de Buenos Aires
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <catch2/catch_test_macros.hpp>

#include <typeindex>
#include <any>

#include <cadmium/basic_model/pdevs/accumulator.hpp>
#include <cadmium/basic_model/pdevs/generator.hpp>
#include <cadmium/modeling/dynamic_message_bag.hpp>
#include <cadmium/modeling/dynamic_models_helpers.hpp>
#include <cadmium/engine/pdevs_dynamic_engine_helpers.hpp>
#include <cadmium/engine/pdevs_engine_helpers.hpp>

/**
  * This test is for some common helper functions used by the dynamic simulator and coordinator classes
  */

TEST_CASE("check_all_bags_empty_on_dynamic_empty_box_is_true", "[pdevs][dynamic][engine][helpers]") {
// This test is supposed to pass only in C++17 compilers with DYNAMIC_ENGINE enabled
#if __cplusplus > 201702 && defined(DYNAMIC_ENGINE)
    // Define a tuple of bags (static version)
    struct test_in_0 : public cadmium::in_port<int> {};
    struct test_in_1 : public cadmium::in_port<double> {};
    using test_input_ports = std::tuple<test_in_0, test_in_1>;
    using input_bags = typename cadmium::make_message_bags<test_input_ports>::type;

    // Create an empty "box" of bags (dynamic version)
    auto empty_box = cadmium::dynamic::modeling::create_empty_message_bags<input_bags>();

    // Check the created box has only empty bags
    CHECK(cadmium::dynamic::engine::all_bags_empty<input_bags>(empty_box));
#else
    WARN("Skipping test: requires C++17 and DYNAMIC_ENGINE defined.");
    SUCCEED();
#endif
}

TEST_CASE("check_all_bags_empty_on_dynamic_non_empty_box_is_false", "[pdevs][dynamic][engine][helpers]") {
// This test is supposed to pass only in C++17 compilers with DYNAMIC_ENGINE enabled
#if __cplusplus > 201702 && defined(DYNAMIC_ENGINE)
    // Create a box with bags with messages
    struct test_in_0 : public cadmium::in_port<int> {};
    struct test_in_1 : public cadmium::in_port<double> {};

    using test_input_ports = std::tuple<test_in_0, test_in_1>;
    using input_bags = typename cadmium::make_message_bags<test_input_ports>::type;

    input_bags bs_tuple;

    // Put some messages in the bags
    cadmium::get_messages<test_in_0>(bs_tuple).push_back(1);
    cadmium::get_messages<test_in_0>(bs_tuple).push_back(2);
    cadmium::get_messages<test_in_1>(bs_tuple).push_back(1.5);
    cadmium::get_messages<test_in_1>(bs_tuple).push_back(2.5);

    // Add the messages from the static bags to a new dynamic one
    cadmium::dynamic::message_bags bs_map;
    cadmium::dynamic::modeling::fill_map_from_bags<input_bags>(bs_tuple, bs_map);

    // Check there is at least a bag that is not empty
    CHECK(!cadmium::dynamic::engine::all_bags_empty<input_bags>(bs_map));
#else
    WARN("Skipping test: requires C++17 and DYNAMIC_ENGINE defined.");
    SUCCEED();
#endif
}
