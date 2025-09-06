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
#include <cadmia/logger/tuple_to_ostream.hpp>

#include <cadmia/basic_model/pdevs/accumulator.hpp>
#include <cadmia/basic_model/pdevs/generator.hpp>
#include <cadmia/engine/pdevs_simulator.hpp>



template<typename TIME>
using int_accumulator=cadmium::basic_models::pdevs::accumulator<int, TIME>;
using int_accumulator_defs=cadmium::basic_models::pdevs::accumulator_defs<int>;

TEST_CASE("accumulator_model_simulation_test", "[accumulator]") {
    //construct a simulator for an accumulator
    using simulator_t = cadmium::engine::simulator<int_accumulator, float, cadmium::logger::not_logger>;
    simulator_t s;
    s.init(0.0f);

    REQUIRE(s.next() == std::numeric_limits<float>::infinity());

    using input_ports = int_accumulator<float>::input_ports;
    //crate the tuple for sending the messages
    typename cadmium::make_message_bags<input_ports>::type input_bags = cadmium::make_message_bags<input_ports>::type{};
    typename cadmium::make_message_bags<input_ports>::type empty_input = cadmium::make_message_bags<input_ports>::type{};
    //insert values to add port and reset to empty
    REQUIRE(cadmium::engine::all_bags_empty(input_bags));
    cadmium::get_messages<int_accumulator_defs::add>(input_bags).assign(std::initializer_list<int>{1, 2, 3, 4});
    cadmium::get_messages<int_accumulator_defs::reset>(input_bags).clear();

    //advance simulator
    s.inbox(input_bags);
    s.advance_simulation(3.0f);
    REQUIRE(s.next() == std::numeric_limits<float>::infinity());

    //external input in reset triggers a reset
    cadmium::get_messages<int_accumulator_defs::add>(input_bags).clear();
    cadmium::get_messages<int_accumulator_defs::reset>(input_bags).emplace_back();
    s.inbox(input_bags);
    s.advance_simulation(4.0f); //here time is referring to absolute chronology, we are in simulation context.
}

TEST_CASE("Accumulator simulation behaves as expected", "[accumulator]") {
    using simulator_t = cadmium::engine::simulator<int_accumulator, float, cadmium::logger::not_logger>;
    simulator_t sim;
    sim.init(0.0f);

    REQUIRE(sim.next() == std::numeric_limits<float>::infinity());

    using input_ports = int_accumulator<float>::input_ports;
    auto input_bags = cadmium::make_message_bags<input_ports>::type{};
    auto empty_bags = cadmium::make_message_bags<input_ports>::type{};

    // Add values to accumulator
    cadmium::get_messages<int_accumulator_defs::add>(input_bags) = {1, 2, 3, 4};
    cadmium::get_messages<int_accumulator_defs::reset>(input_bags).clear();

    sim.inbox(input_bags);
    sim.advance_simulation(3.0f);
    REQUIRE(sim.next() == std::numeric_limits<float>::infinity());

    // Trigger reset
    cadmium::get_messages<int_accumulator_defs::add>(input_bags).clear();
    cadmium::get_messages<int_accumulator_defs::reset>(input_bags).push_back({});
    sim.inbox(input_bags);
    sim.advance_simulation(4.0f);
    REQUIRE(sim.next() == 4.0f);

    // Collect output after reset
    sim.collect_outputs(4.0f);
    auto out_bags = sim.outbox();
    REQUIRE_FALSE(cadmium::engine::all_bags_empty(out_bags));
    REQUIRE(cadmium::get_messages<int_accumulator_defs::sum>(out_bags).size() == 1);
    REQUIRE(cadmium::get_messages<int_accumulator_defs::sum>(out_bags)[0] == 10);

    // Internal transition after output
    sim.inbox(empty_bags);
    sim.advance_simulation(4.0f);
    REQUIRE(sim.next() == std::numeric_limits<float>::infinity());

    // Reset again, expect sum to be zero
    sim.inbox(input_bags);
    sim.advance_simulation(5.0f);
    REQUIRE(sim.next() == 5.0f);
    sim.collect_outputs(5.0f);
    out_bags = sim.outbox();
    REQUIRE_FALSE(cadmium::engine::all_bags_empty(out_bags));
    REQUIRE(cadmium::get_messages<int_accumulator_defs::sum>(out_bags).size() == 1);
    REQUIRE(cadmium::get_messages<int_accumulator_defs::sum>(out_bags)[0] == 0);

    sim.inbox(empty_bags);
    sim.advance_simulation(5.0f);
    REQUIRE(sim.next() == std::numeric_limits<float>::infinity());

    // Add again and check sum
    cadmium::get_messages<int_accumulator_defs::add>(input_bags) = {1, 2, 3, 4};
    sim.inbox(input_bags);
    sim.advance_simulation(6.0f);
    REQUIRE(sim.next() == 6.0f);
    sim.collect_outputs(6.0f);
    out_bags = sim.outbox();
    REQUIRE_FALSE(cadmium::engine::all_bags_empty(out_bags));
    REQUIRE(cadmium::get_messages<int_accumulator_defs::sum>(out_bags).size() == 1);
    REQUIRE(cadmium::get_messages<int_accumulator_defs::sum>(out_bags)[0] == 10);

    sim.inbox(empty_bags);
    sim.advance_simulation(6.0f);
    REQUIRE(sim.next() == std::numeric_limits<float>::infinity());
}

TEST_CASE("accumulator_simulation_throws_test", "[accumulator]") {
    using simulator_t = cadmium::engine::simulator<int_accumulator, float, cadmium::logger::not_logger>;
    using input_ports = int_accumulator<float>::input_ports;
    simulator_t s;
    s.init(0.0f);
    REQUIRE(s.next() == std::numeric_limits<float>::infinity());

    auto input_bags = cadmium::make_message_bags<input_ports>::type{};
    auto empty_input = cadmium::make_message_bags<input_ports>::type{};
    cadmium::get_messages<int_accumulator_defs::add>(input_bags) = {1, 2, 3, 4};
    cadmium::get_messages<int_accumulator_defs::reset>(input_bags).emplace_back();

    s.inbox(input_bags);
    s.advance_simulation(3.0f);
    REQUIRE(s.next() == 3.0f);

    // Try to input in the past of current time
    s.inbox(input_bags);
    REQUIRE_THROWS_AS(s.advance_simulation(2.0f), std::domain_error);

    // Try to input later than next scheduled internal event
    s.inbox(input_bags);
    REQUIRE_THROWS_AS(s.advance_simulation(4.0f), std::domain_error);

    // Execute expected internal transition
    s.inbox(empty_input);
    s.advance_simulation(3.0f);
    REQUIRE(s.next() == std::numeric_limits<float>::infinity());
}

const float init_period = 1.0f;
const float init_output_message = 2.0f;
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

TEST_CASE("generator_model_simulation_test", "[generator]") {
    //construct a simulator for a generator of floats
    using simulator_t = cadmium::engine::simulator<floating_generator, float, cadmium::logger::not_logger>;
    simulator_t s;
    s.init(0.0f);
    REQUIRE(s.next() == 1.0f);

    //collecting early output produces a "false output".
    s.collect_outputs(0.5f);
    auto out = s.outbox();
    REQUIRE(cadmium::engine::all_bags_empty(out)); // obtaining an empty bag of messages

    //collecting output
    s.collect_outputs(1.0f);
    out = s.outbox();
    REQUIRE(cadmium::get_messages<floating_generator_defs::out>(out).size() == 1);
    REQUIRE(cadmium::get_messages<floating_generator_defs::out>(out)[0] == 2.0f);

    //advance simulation
    s.advance_simulation(1.0f);

    //check next time is 2.0f
    REQUIRE(s.next() == 2.0f);
}
