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


#include<catch2/catch_test_macros.hpp>

#include<cadmium/modeling/message_bag.hpp>
#include<cadmium/basic_model/pdevs/generator.hpp>
#include<cadmium/concept/concept_helpers.hpp>

#include<stdexcept>

const float init_period = 0.1f;
const float init_output_message = 1.0f;
template<typename TIME>
using floating_generator_base=cadmium::basic_models::pdevs::generator<float, TIME>;
using floating_generator_defs=cadmium::basic_models::pdevs::generator_defs<float>;
template<typename TIME>
struct floating_generator : public floating_generator_base<TIME> {
    float period() const override {
        return init_period;
    }
    float output_message() const override {
        return init_output_message;
    }
};


TEST_CASE("it_is_atomic_test", "[floating_generator]") {
    REQUIRE(cadmium::old_concept::is_atomic<floating_generator>::value());
}

TEST_CASE("it_is_constructable_test", "[floating_generator]") {
    REQUIRE_NOTHROW(floating_generator<float>{});
}
TEST_CASE("time_advance_is_the_init_one_even_after_internal_transition_test", "[floating_generator]") {
    auto g = floating_generator<float>();
    REQUIRE(init_period == g.time_advance());
    g.internal_transition();
    REQUIRE(init_period == g.time_advance());
}

TEST_CASE("it_throws_on_call_to_confluence_transition_test", "[floating_generator]") {
    auto g = floating_generator<float>();
    typename cadmium::make_message_bags<floating_generator<float>::input_ports>::type bags;
    REQUIRE_THROWS_AS(g.confluence_transition(5.0, bags), std::logic_error);
}

TEST_CASE("it_throws_on_call_to_external_transition_test", "[floating_generator]") {
    auto g = floating_generator<float>();
    typename cadmium::make_message_bags<floating_generator<float>::input_ports>::type bags;
    REQUIRE_THROWS_AS(g.external_transition(5.0, bags), std::logic_error);
}

TEST_CASE("output_function_returns_init_message_test", "[floating_generator]") {
    auto g = floating_generator<float>();
    auto o = g.output();
    auto o_m = cadmium::get_messages<typename floating_generator_defs::out>(o);
    REQUIRE(o_m.size() == 1);
    REQUIRE(o_m.at(0) == init_output_message);
}
