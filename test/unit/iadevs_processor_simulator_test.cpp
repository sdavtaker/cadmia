// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2025, Damian Vicino
 * Carleton University
 * All rights reserved.
 */

#include <catch2/catch_test_macros.hpp>

#include <cadmia/engine/simulator.hpp>
#include <cadmia/basic_model/iadevs/processor.hpp>

using cadmia::iadevs::processor;
using simulator_t = cadmia::engine::simulator<processor>;

namespace {
    // Helper: dec3 from scaled integer
    inline processor::dec3 ts(int raw) { return processor::dec3::from_scaled(raw); }

    // Helper: job interval [id,id]
    inline simulator_t::input_i_t job(int id) {
        return simulator_t::input_i_t::closed(id, id);
    }

    // Build state interval with queue of given ids and tocj lower/upper
    inline simulator_t::state_i_t make_state(const std::initializer_list<int> &lower_q,
                                             const std::initializer_list<int> &upper_q,
                                             int tocj_low_scaled, int tocj_up_scaled) {
        processor::state_t sl{}; processor::state_t sr{};
        sl.tocj = ts(tocj_low_scaled); sr.tocj = ts(tocj_up_scaled);
        sl.qj.clear(); sr.qj.clear();
        for (int v : lower_q) sl.qj.push_back(v);
        for (int v : upper_q) sr.qj.push_back(v);
        return simulator_t::state_i_t::closed(sl, sr);
    }
}

SCENARIO("Simulator init and immediate internal event", "[simulator][processor]") {
    GIVEN("a processor with one job and punctual time [1000,1000]") {
        simulator_t sim;
        auto q_state = make_state({1}, {1}, 0, 0); // tocj = [0,0]
        auto q_time  = simulator_t::time_i_t::closed(ts(0), ts(0)); // elapsed = [0,0]
        auto t_init  = simulator_t::time_i_t::closed(ts(1000), ts(1000));

        WHEN("init is called") {
            sim.init(q_state, q_time, t_init);
            THEN("t_last = t_init - q_time = [1000,1000] and t_next = t_last + TA(state) = [1250,1250]") {
                REQUIRE(sim.t_last().lower == ts(1000));
                REQUIRE(sim.t_last().upper == ts(1000));
                REQUIRE(sim.t_next().lower == ts(1250));
                REQUIRE(sim.t_next().upper == ts(1250));
            }
            AND_WHEN("star is called at t_next") {
                auto opt = sim.star(sim.t_next());
                THEN("output contains head job interval [1,1]") {
                    REQUIRE(opt.has_value());
                    REQUIRE(opt->lower == 1);
                    REQUIRE(opt->upper == 1);
                }
                THEN("state dequeues job and becomes empty => internal_transition returns empty interval next time") {
                    // After star, state_ should have dequeued first job (processor::internal_transition logic)
                    // Our simulator state holds interval; with single job it becomes empty interval.
                    // time advance recomputed
                    REQUIRE(sim.state().is_empty());
                }
            }
        }
    }
}

SCENARIO("Simulator external event ingestion then internal event", "[simulator][processor]") {
    GIVEN("an initially idle processor (empty queues) at time [0,0]") {
        simulator_t sim;
        auto q_state = make_state({}, {}, 0, 0); // empty queues
        auto q_time  = simulator_t::time_i_t::closed(ts(0), ts(0));
        auto t0      = simulator_t::time_i_t::closed(ts(0), ts(0));
        sim.init(q_state, q_time, t0);
        REQUIRE(sim.state().lower.qj.empty());

        WHEN("an external job [5,5] arrives at time [50,50]") {
            auto t_ext = simulator_t::time_i_t::closed(ts(50), ts(50));
            sim.x(job(5), t_ext);
            THEN("state now has the job enqueued and t_last updated to [50,50]") {
                REQUIRE(sim.state().lower.qj.size() == 1);
                REQUIRE(sim.t_last().lower == ts(50));
            }
            AND_WHEN("star is called at t_next") {
                auto opt = sim.star(sim.t_next());
                THEN("output contains [5,5]") {
                    REQUIRE(opt.has_value());
                    REQUIRE(opt->lower == 5);
                    REQUIRE(opt->upper == 5);
                }
            }
        }
    }
}

SCENARIO("Simulator init with various time interval forms", "[simulator][processor]") {
    GIVEN("a processor with two jobs and tocj=[0,0]") {
        simulator_t sim;
        auto q_state = make_state({1,2}, {1,2}, 0, 0);
        auto q_time  = simulator_t::time_i_t::closed(ts(0), ts(0));

        WHEN("initialized with punctual time [1000,1000]") {
            auto t_punctual = simulator_t::time_i_t::closed(ts(1000), ts(1000));
            sim.init(q_state, q_time, t_punctual);
            REQUIRE(sim.t_last().lower == ts(1000));
            REQUIRE(sim.t_next().lower == ts(1250));
        }
        WHEN("initialized with interval (1000, 1100)") {
            auto t_open = simulator_t::time_i_t::open(ts(1000), ts(1100));
            sim.init(q_state, q_time, t_open);
            // t_last = t - q_time == t interval (since q_time = [0,0])
            REQUIRE(sim.t_last().lower == ts(1000));
            REQUIRE_FALSE(sim.t_last().lower_closed);
            REQUIRE(sim.t_last().upper == ts(1100));
            REQUIRE_FALSE(sim.t_last().upper_closed);
        }
        WHEN("initialized with right-open [0, 1000)") {
            auto t_right_open = simulator_t::time_i_t::right_open(ts(0), ts(1000));
            sim.init(q_state, q_time, t_right_open);
            REQUIRE(sim.t_last().lower == ts(0));
            REQUIRE(sim.t_last().lower_closed);
            REQUIRE(sim.t_last().upper == ts(1000));
            REQUIRE_FALSE(sim.t_last().upper_closed);
        }
        WHEN("initialized with left-open (0, 1000]") {
            auto t_left_open = simulator_t::time_i_t::left_open(ts(0), ts(1000));
            sim.init(q_state, q_time, t_left_open);
            REQUIRE(sim.t_last().lower == ts(0));
            REQUIRE_FALSE(sim.t_last().lower_closed);
            REQUIRE(sim.t_last().upper == ts(1000));
            REQUIRE(sim.t_last().upper_closed);
        }
    }
}
