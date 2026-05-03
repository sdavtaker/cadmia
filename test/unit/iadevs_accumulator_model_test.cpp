// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2025, Damian Vicino
 * Carleton University
 * All rights reserved.
 */

#include <cadmia/basic_model/iadevs/accumulator.hpp>
#include <cadmia/engine/simulator.hpp>

#include <catch2/catch_test_macros.hpp>

using cadmia::iadevs::accumulator;
using sim_t = cadmia::engine::simulator<accumulator>;

using phase_t    = accumulator::phase_t;
using state_t    = accumulator::state_t;
using state_i_t  = accumulator::state_i_t;
using time_i_t   = accumulator::time_i_t;
using input_i_t  = accumulator::input_i_t;
using output_i_t = accumulator::output_i_t;
using dec3       = accumulator::dec3;

namespace {
    state_i_t passive_state(int total_lo, int total_hi) {
        return state_i_t::closed(state_t{phase_t::passive, total_lo},
                                 state_t{phase_t::passive, total_hi});
    }
    state_i_t output_state(int total_lo, int total_hi) {
        return state_i_t::closed(state_t{phase_t::output, total_lo},
                                 state_t{phase_t::output, total_hi});
    }
    time_i_t t(int ms) {
        return time_i_t::closed(dec3::from_scaled(ms), dec3::from_scaled(ms));
    }
    input_i_t job(int lo, int hi) {
        return input_i_t::closed(lo, hi);
    }
} // namespace

TEST_CASE("Accumulator initial state is passive with total 0", "[accumulator]") {
    auto s = passive_state(0, 0);
    REQUIRE(s.lower.phase == phase_t::passive);
    REQUIRE(s.lower.total == 0);

    auto ta = accumulator::time_advance(s);
    CHECK(ta.is_empty()); // passive
}

TEST_CASE("Accumulator external_transition sets OUTPUT phase and adds input", "[accumulator]") {
    auto s0  = passive_state(0, 0);
    auto inp = job(3, 5);
    auto s1  = accumulator::external_transition(s0, t(0), inp);

    CHECK(s1.lower.phase == phase_t::output);
    CHECK(s1.upper.phase == phase_t::output);
    CHECK(s1.lower.total == 3);
    CHECK(s1.upper.total == 5);
}

TEST_CASE("Accumulator output returns current total interval", "[accumulator]") {
    auto s = output_state(7, 12);
    auto y = accumulator::output(s);
    CHECK(y.lower == 7);
    CHECK(y.upper == 12);
}

TEST_CASE("Accumulator time_advance OUTPUT→OUTPUT returns [0,0]", "[accumulator]") {
    auto s  = output_state(5, 5);
    auto ta = accumulator::time_advance(s);
    CHECK_FALSE(ta.is_empty());
    CHECK(ta.lower == dec3{});
    CHECK(ta.upper == dec3{});
    CHECK(ta.lower_closed);
    CHECK(ta.upper_closed);
}

TEST_CASE("Accumulator internal_transition moves phase to PASSIVE preserving total",
          "[accumulator]") {
    auto s0 = output_state(4, 9);
    auto s1 = accumulator::internal_transition(s0);
    CHECK(s1.lower.phase == phase_t::passive);
    CHECK(s1.upper.phase == phase_t::passive);
    CHECK(s1.lower.total == 4);
    CHECK(s1.upper.total == 9);

    // After internal transition: passive again
    CHECK(accumulator::time_advance(s1).is_empty());
}

TEST_CASE("Accumulator accumulates across multiple external events", "[accumulator]") {
    auto s = passive_state(0, 0);
    s      = accumulator::external_transition(s, t(0), job(2, 2));
    CHECK(s.lower.total == 2);
    s = accumulator::internal_transition(s); // outputs, returns to passive
    s = accumulator::external_transition(s, t(100), job(3, 3));
    CHECK(s.lower.total == 5); // 2 + 3
    CHECK(s.upper.total == 5);
}

TEST_CASE("Accumulator simulator: init, external, internal round-trip", "[accumulator]") {
    sim_t sim;
    auto init_state = passive_state(0, 0);
    auto zero       = time_i_t::closed(dec3{}, dec3{});
    sim.init(init_state, zero, zero);

    // t_next should be passive (empty)
    CHECK(sim.t_next().is_empty());

    // External event: add [1, 1] at t = [0, 0]
    sim.x(job(1, 1), zero);

    // Now should be in OUTPUT phase — t_next = [0, 0]
    CHECK_FALSE(sim.t_next().is_empty());

    // Internal (star) event
    auto result = sim.star(sim.t_next());
    REQUIRE(result.has_value());
    CHECK(result->lower == 1);
    CHECK(result->upper == 1);

    // Back to passive
    CHECK(sim.t_next().is_empty());
}
