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
#include <catch2/catch_approx.hpp>

#include <sstream>
#include <typeinfo>

#include <cadmia/logger/tuple_to_ostream.hpp>
#include <cadmia/basic_model/pdevs/int_generator_one_sec.hpp>
#include <cadmia/basic_model/pdevs/reset_generator_five_sec.hpp>
#include <cadmia/basic_model/pdevs/generator.hpp>
#include <cadmia/basic_model/pdevs/accumulator.hpp>
#include <cadmia/modeling/coupling.hpp>
#include <cadmia/engine/pdevs_runner.hpp>

/**
  This test suite is running basic models that were tested in other suites before
  The time for "next" in runner is absolute, starting at the time set by init_time.
*/

// message representing ticks
struct test_tick {};

// generator for tick messages
using out_port = cadmium::basic_models::pdevs::generator_defs<test_tick>::out;

template<typename TIME>
using test_tick_generator_base = cadmium::basic_models::pdevs::generator<test_tick, TIME>;

template<typename TIME>
struct test_generator : public test_tick_generator_base<TIME> {
    float period() const override { return 1.0f; } // using float for time in this test, ticking every second
    test_tick output_message() const override { return test_tick(); }
};

// generator of ticks coupled model definition
using iports = std::tuple<>;
struct coupled_out_port : public cadmium::out_port<test_tick> {};
using oports = std::tuple<coupled_out_port>;
using submodels = cadmium::modeling::models_tuple<test_generator>;
using eics = std::tuple<>;
using eocs = std::tuple<
    cadmium::modeling::EOC<test_generator, out_port, coupled_out_port>
>;
using ics = std::tuple<>;

template<typename TIME>
using coupled_generator = cadmium::modeling::pdevs::coupled_model<TIME, iports, oports, submodels, eics, eocs, ics>;

namespace {
    std::ostringstream oss;

    struct oss_test_sink_provider {
        static std::ostream& sink() { return oss; }
    };
}

TEST_CASE("pdevs_runner_of_a_generator_in_a_coupled_for_a_minute_test", "[pdevs_runner][silent]") {
    cadmium::engine::runner<float, coupled_generator, cadmium::logger::not_logger> r{0.0};
    float next_to_end_time = r.run_until(60.0);
    CHECK(next_to_end_time == Catch::Approx(60.0f));
}

TEST_CASE("runner_logs_global_time_advances_test", "[pdevs_runner][logger]") {
    oss.str("");
    oss.clear();
    using log_gt_to_oss = cadmium::logger::logger<
        cadmium::logger::logger_global_time,
        cadmium::logger::formatter<float>,
        oss_test_sink_provider
    >;

    cadmium::engine::runner<float, coupled_generator, log_gt_to_oss> r{0.0};
    r.run_until(3.0);

    auto expected = std::string("0\n"  // runner init time
                                "1\n"  // runner first advance
                                "2\n"  // runner last advance
    );
    CHECK(oss.str() == expected);
}

TEST_CASE("simulation_logs_info_on_setup_and_start_loops_and_end_of_run_test", "[pdevs_runner][logger][info]") {
    oss.str("");
    oss.clear();
    using log_info_to_oss = cadmium::logger::logger<
        cadmium::logger::logger_info,
        cadmium::logger::formatter<float>,
        oss_test_sink_provider
    >;

    cadmium::engine::runner<float, coupled_generator, log_info_to_oss> r{0.0};
    r.run_until(2.0);

    std::ostringstream expected_oss;
    expected_oss << "Preparing model\n"; // setup of model by runner

    // top model is init
    expected_oss << "Coordinator for model ";
    expected_oss << typeid(coupled_generator<float>).name();
    expected_oss << " initialized to time 0\n";

    // generator model is init
    expected_oss << "Simulator for model ";
    expected_oss << typeid(test_generator<float>).name();
    expected_oss << " initialized to time 0\n";

    expected_oss << "Starting run\n"; // starting simulation main loop in runner

    // top model collects outputs
    expected_oss << "Coordinator for model ";
    expected_oss << typeid(coupled_generator<float>).name();
    expected_oss << " collecting output at time 1\n";

    expected_oss << "Simulator for model ";
    expected_oss << typeid(test_generator<float>).name();
    expected_oss << " collecting output at time 1\n";

    // top model advances simulation
    expected_oss << "Coordinator for model ";
    expected_oss << typeid(coupled_generator<float>).name();
    expected_oss << " advancing simulation from time 0 to 1\n";

    expected_oss << "Simulator for model ";
    expected_oss << typeid(test_generator<float>).name();
    expected_oss << " advancing simulation from time 0 to 1\n";

    expected_oss << "Finished run\n"; // finished simulation and exiting runner
    CHECK(oss.str() == expected_oss.str());
}

TEST_CASE("simulation_logs_state_only_show_state_changes_and_initial_state_test", "[pdevs_runner][logger][state]") {
    oss.str("");
    oss.clear();
    using log_state_to_oss = cadmium::logger::logger<
        cadmium::logger::logger_state,
        cadmium::logger::formatter<float>,
        oss_test_sink_provider
    >;

    cadmium::engine::runner<float, coupled_generator, log_state_to_oss> r{0.0};
    r.run_until(3.0);

    std::ostringstream expected_oss;
    for (int i = 0; i < 3; i++) { // initial state and 2 more states
        expected_oss << "State for model ";
        expected_oss << typeid(test_generator<float>).name();
        expected_oss << " is 0\n";
    }

    CHECK(oss.str() == expected_oss.str());
}

TEST_CASE("simulation_logs_messages_generated_in_atomic_models_test", "[pdevs_runner][logger][messages]") {
    oss.str("");
    oss.clear();
    using log_messages_to_oss = cadmium::logger::logger<
        cadmium::logger::logger_messages,
        cadmium::logger::formatter<float>,
        oss_test_sink_provider
    >;

    cadmium::engine::runner<float, coupled_generator, log_messages_to_oss> r{0.0};
    r.run_until(2.0);

    std::ostringstream expected_oss;
    expected_oss << "[";
    expected_oss << typeid(out_port).name();
    expected_oss << ": {obscure message of type ";
    expected_oss << typeid(test_tick).name();
    expected_oss << "}] generated by model ";
    expected_oss << typeid(test_generator<float>).name();
    expected_oss << "\n";

    CHECK(oss.str() == expected_oss.str());
}

TEST_CASE("simulation_logs_local_time_in_simulators_test", "[pdevs_runner][logger][local_time]") {
    oss.str("");
    oss.clear();
    using log_local_time_to_oss = cadmium::logger::logger<
        cadmium::logger::logger_local_time,
        cadmium::logger::formatter<float>,
        oss_test_sink_provider
    >;

    cadmium::engine::runner<float, coupled_generator, log_local_time_to_oss> r{0.0};
    r.run_until(2.0);

    std::ostringstream expected_oss;
    expected_oss << "Elapsed in model ";
    expected_oss << typeid(test_generator<float>).name();
    expected_oss << " is 1s\n";

    CHECK(oss.str() == expected_oss.str());
}

TEST_CASE("simulation_logs_routing_of_eoc_in_coordinator_test", "[pdevs_runner][logger][routing]") {
    oss.str("");
    oss.clear();
    using log_routing_to_oss = cadmium::logger::logger<
        cadmium::logger::logger_message_routing,
        cadmium::logger::formatter<float>,
        oss_test_sink_provider
    >;

    cadmium::engine::runner<float, coupled_generator, log_routing_to_oss> r{0.0};
    r.run_until(2.0);

    std::ostringstream expected_oss;
    // EOC of one event
    expected_oss << "EOC for model ";
    expected_oss << typeid(coupled_generator<float>).name();
    expected_oss << "\n in port ";
    expected_oss << typeid(coupled_out_port).name();
    expected_oss << " has {obscure message of type ";
    expected_oss << typeid(test_tick).name();
    expected_oss << "} routed from ";
    expected_oss << typeid(out_port).name();
    expected_oss << " of model ";
    expected_oss << typeid(test_generator<float>).name();
    expected_oss << " with messages {obscure message of type ";
    expected_oss << typeid(test_tick).name();
    expected_oss << "}\n";
    // empty IC
    expected_oss << "IC for model ";
    expected_oss << typeid(coupled_generator<float>).name();
    expected_oss << "\n";
    // empty EIC
    expected_oss << "EIC for model ";
    expected_oss << typeid(coupled_generator<float>).name();
    expected_oss << "\n";

    CHECK(oss.str() == expected_oss.str());
}

// 2 generators connected to an infinite_counter are coordinated and routing messages correctly
// connecting generators to accumm coupled model definition
template<typename TIME>
using test_accumulator = cadmium::basic_models::pdevs::accumulator<int, TIME>;
using test_accumulator_defs = cadmium::basic_models::pdevs::accumulator_defs<int>;
using reset_tick = cadmium::basic_models::pdevs::accumulator_defs<int>::reset_tick;

using g2a_iports = std::tuple<>;
struct g2a_coupled_out_port : public cadmium::out_port<int> {};
using g2a_oports = std::tuple<g2a_coupled_out_port>;
using g2a_submodels = cadmium::modeling::models_tuple<
    test_accumulator,
    cadmium::basic_models::pdevs::reset_generator_five_sec,
    cadmium::basic_models::pdevs::int_generator_one_sec
>;
using g2a_eics = std::tuple<>;
using g2a_eocs = std::tuple<
    cadmium::modeling::EOC<test_accumulator, test_accumulator_defs::sum, g2a_coupled_out_port>
>;
using g2a_ics = std::tuple<
    cadmium::modeling::IC<
        cadmium::basic_models::pdevs::int_generator_one_sec,
        cadmium::basic_models::pdevs::int_generator_one_sec_defs::out,
        test_accumulator,
        test_accumulator_defs::add
    >,
    cadmium::modeling::IC<
        cadmium::basic_models::pdevs::reset_generator_five_sec,
        cadmium::basic_models::pdevs::reset_generator_five_sec_defs::out,
        test_accumulator,
        test_accumulator_defs::reset
    >
>;

template<typename TIME>
using coupled_g2a_model = cadmium::modeling::pdevs::coupled_model<TIME, g2a_iports, g2a_oports, g2a_submodels, g2a_eics, g2a_eocs, g2a_ics>;

TEST_CASE("simulation_logs_routing_of_all_couplings_in_coordinator_test", "[pdevs_runner][logger][routing]") {
    oss.str("");
    oss.clear();
    using log_routing_to_oss = cadmium::logger::logger<
        cadmium::logger::logger_message_routing,
        cadmium::logger::formatter<float>,
        oss_test_sink_provider
    >;

    cadmium::engine::runner<float, coupled_g2a_model, log_routing_to_oss> r{0.0};
    r.run_until(2.0);

    std::ostringstream expected_oss;
    // empty output on EOC
    expected_oss << "EOC for model ";
    expected_oss << typeid(coupled_g2a_model<float>).name();
    expected_oss << "\n in port ";
    expected_oss << typeid(g2a_coupled_out_port).name();
    expected_oss << " has {} routed from ";
    expected_oss << typeid(cadmium::basic_models::pdevs::accumulator_defs<int>::sum).name();
    expected_oss << " of model ";
    expected_oss << typeid(cadmium::basic_models::pdevs::accumulator<int, float>).name();
    expected_oss << " with messages {}\n";

    // IC routing
    expected_oss << "IC for model ";
    expected_oss << typeid(coupled_g2a_model<float>).name();

    expected_oss << "\n in port ";
    expected_oss << typeid(cadmium::basic_models::pdevs::accumulator_defs<int>::reset).name();
    expected_oss << " of model ";
    expected_oss << typeid(cadmium::basic_models::pdevs::accumulator<int, float>).name();
    expected_oss << " has {} routed from ";
    expected_oss << typeid(cadmium::basic_models::pdevs::reset_generator_five_sec_defs::out).name();
    expected_oss << " of model ";
    expected_oss << typeid(cadmium::basic_models::pdevs::reset_generator_five_sec<float>).name();
    expected_oss << " with messages {}";

    expected_oss << "\n in port ";
    expected_oss << typeid(cadmium::basic_models::pdevs::accumulator_defs<int>::add).name();
    expected_oss << " of model ";
    expected_oss << typeid(cadmium::basic_models::pdevs::accumulator<int, float>).name();
    expected_oss << " has {1} routed from ";
    expected_oss << typeid(cadmium::basic_models::pdevs::int_generator_one_sec_defs::out).name();
    expected_oss << " of model ";
    expected_oss << typeid(cadmium::basic_models::pdevs::int_generator_one_sec<float>).name();
    expected_oss << " with messages {1}\n";

    // EIC routing
    expected_oss << "EIC for model ";
    expected_oss << typeid(coupled_g2a_model<float>).name();
    expected_oss << "\n";

    CHECK(oss.str() == expected_oss.str());
}

