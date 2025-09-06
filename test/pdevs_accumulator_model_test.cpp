// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2013-2025, Damian Vicino
 * Carleton University, Universite de Nice-Sophia Antipolis
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
#include <cadmia/modeling/message_bag.hpp>
#include <cadmia/basic_model/pdevs/accumulator.hpp>
#include <cadmia/concept/concept_helpers.hpp>
#include <cmath>
#include <stdexcept>

template<typename TIME>
using floating_accumulator = cadmium::basic_models::pdevs::accumulator<float, TIME>;
using floating_accumulator_defs = cadmium::basic_models::pdevs::accumulator_defs<float>;

TEST_CASE("pdevs_accumulator is atomic", "[pdevs_accumulator]") {
    REQUIRE(cadmium::old_concept::is_atomic<floating_accumulator>::value());
}

TEST_CASE("pdevs_accumulator is constructible", "[pdevs_accumulator]") {
    REQUIRE_NOTHROW(floating_accumulator<float>{});
}

TEST_CASE("Time advance is infinite after internal transition", "[pdevs_accumulator]") {
    auto a = floating_accumulator<float>();
    a.state = std::make_tuple(1.0f, true);
    REQUIRE(a.time_advance() == 0.0f);
    a.internal_transition();
    REQUIRE(std::isinf(a.time_advance()));
    REQUIRE(std::get<float>(a.state) == 0.0f);
    REQUIRE(std::get<bool>(a.state) == false);
}

TEST_CASE("Throws on internal transition at non-reset state", "[pdevs_accumulator]") {
    auto a = floating_accumulator<float>();
    a.state = std::make_tuple(1.0f, false);
    REQUIRE(std::isinf(a.time_advance()));
    REQUIRE_THROWS_AS(a.internal_transition(), std::logic_error);
}

TEST_CASE("Throws on external transition on reset state", "[pdevs_accumulator]") {
    auto a = floating_accumulator<float>();
    a.state = std::make_tuple(1.0f, true);
    REQUIRE(a.time_advance() == 0.0f);

    typename cadmium::make_message_bags<floating_accumulator<float>::input_ports>::type bags;
    cadmium::get_messages<floating_accumulator_defs::add>(bags).push_back(5.0f);
    REQUIRE_THROWS_AS(a.external_transition(1.0f, bags), std::logic_error);
}

TEST_CASE("Output function throws when not in reset state", "[pdevs_accumulator]") {
    auto a = floating_accumulator<float>();
    a.state = std::make_tuple(1.0f, false);
    REQUIRE_THROWS_AS(a.output(), std::logic_error);
}

TEST_CASE("Output function returns accumulated value and confluence transition works", "[pdevs_accumulator]") {
    auto a = floating_accumulator<float>();
    a.state = std::make_tuple(10.0f, false);

    // Add 5.0
    typename cadmium::make_message_bags<floating_accumulator<float>::input_ports>::type bags_one;
    cadmium::get_messages<floating_accumulator_defs::add>(bags_one).push_back(5.0f);
    a.external_transition(10.0f, bags_one);
    REQUIRE(std::get<float>(a.state) == 15.0f);
    REQUIRE(std::get<bool>(a.state) == false);

    // Add 3.0 and 7.0
    typename cadmium::make_message_bags<floating_accumulator<float>::input_ports>::type bags_two;
    cadmium::get_messages<floating_accumulator_defs::add>(bags_two).push_back(3.0f);
    cadmium::get_messages<floating_accumulator_defs::add>(bags_two).push_back(7.0f);
    a.external_transition(9.0f, bags_two);
    REQUIRE(std::get<float>(a.state) == 25.0f);
    REQUIRE(std::get<bool>(a.state) == false);

    // Add 3.0 and reset
    typename cadmium::make_message_bags<floating_accumulator<float>::input_ports>::type bags_three;
    cadmium::get_messages<floating_accumulator_defs::add>(bags_three).push_back(3.0f);
    cadmium::get_messages<floating_accumulator_defs::reset>(bags_three).emplace_back();
    a.external_transition(2.0f, bags_three);
    REQUIRE(std::get<float>(a.state) == 28.0f);
    REQUIRE(std::get<bool>(a.state) == true);

    // Validate output
    auto outmb1 = a.output();
    REQUIRE(cadmium::get_messages<floating_accumulator_defs::sum>(outmb1).size() == 1);
    REQUIRE(cadmium::get_messages<floating_accumulator_defs::sum>(outmb1)[0] == 28.0f);

    // Confluence transition
    typename cadmium::make_message_bags<floating_accumulator<float>::input_ports>::type bags_four;
    cadmium::get_messages<floating_accumulator_defs::add>(bags_four).push_back(2.0f);
    a.confluence_transition(0.0f, bags_four);
    REQUIRE(std::get<float>(a.state) == 2.0f);
    REQUIRE(std::get<bool>(a.state) == false);
}
