// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2025, Damian Vicino
 * Carleton University
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

#pragma once

#include <cadmia/modeling/decimal.hpp>
#include <cadmia/modeling/interval.hpp>

#include <compare>
#include <stdexcept>
#include <string>

// IA-DEVS Counter model
// This model accumulates incoming Add events and outputs the accumulated count
// when a Reset event is received. After outputting, the counter resets to 0.
//
// State: (phase, count) where:
//   - phase: either 'passive' (waiting) or 'output' (ready to emit)
//   - count: accumulated value (non-negative integer)
//
// Inputs: Add (increment counter) or Reset (output and reset counter)
// Outputs: integer count value
//
// Behavior:
//   - Add event: increment count, remain passive
//   - Reset event: transition to output phase with ta=0
//   - Internal transition: output count, reset to (passive, 0)

namespace cadmia::iadevs {

    class counter {
      public:
        using dec3 = cadmia::modeling::decimal<3>; // 1 ms resolution

        // Phase enumeration
        enum class phase_t : int { passive = 0, output = 1 };

        // Base types required by IADEVSAtomicModel concept
        struct state_t {
            phase_t phase{phase_t::passive}; // current phase (ordered first)
            int     count{0};                // accumulated count

            // Default constructor
            constexpr state_t() noexcept = default;

            // Preferred constructor: phase first, then count
            constexpr state_t(phase_t p, int c) noexcept : phase(p), count(c) {}

            // Comparison for interval ordering: phase first, then count
            [[nodiscard]] std::strong_ordering operator<=>(const state_t &other) const noexcept {
                if (auto cmp = phase <=> other.phase; cmp != 0) {
                    return cmp;
                }
                return count <=> other.count;
            }

            [[nodiscard]] bool operator==(const state_t &other) const noexcept = default;
        };

        // Input event enumeration
        enum class input_event_t : int { add = 0, reset = 1 };

        using time_t   = dec3;          // time base type
        using input_t  = input_event_t; // input base type
        using output_t = int;           // output base type (count value)

        // Interval aliases for convenience
        using input_i_t  = cadmia::modeling::interval<input_t>;
        using output_i_t = cadmia::modeling::interval<output_t>;
        using state_i_t  = cadmia::modeling::interval<state_t>;
        using time_i_t   = cadmia::modeling::interval<time_t>;

        // Approximated functions

        // internal_transition: after outputting, reset count to 0 and return to passive
        [[nodiscard]] static state_i_t internal_transition(const state_i_t &s) {
            if (s.is_empty()) {
                throw std::invalid_argument("internal_transition: empty state interval");
            }

            const bool lower_passive = (s.lower.phase == phase_t::passive);
            const bool lower_output  = (s.lower.phase == phase_t::output);
            const bool upper_passive = (s.upper.phase == phase_t::passive);
            const bool upper_output  = (s.upper.phase == phase_t::output);

            // Case 1: both endpoints in output phase -> reset to [(passive,0),(passive,0)]
            if (lower_output && upper_output) {
                state_t result_state{phase_t::passive, 0};
                return state_i_t::closed(result_state, result_state);
            }

            // Case 2: mixed phases where lower is passive and upper is output
            // -> reset lower to 0, keep upper count, move both to passive
            if (lower_passive && upper_output) {
                state_t lo{phase_t::passive, 0};
                state_t hi{phase_t::passive, s.upper.count};
                return state_i_t::closed(lo, hi);
            }

            // Case 3: lower is passive and upper is +inf (right-open)
            // -> reset lower to 0 and propagate right-open to +inf in passive phase
            if (lower_passive && s.is_upper_infinite()) {
                state_t lo{phase_t::passive, 0};
                // Preserve right-open to +inf
                return state_i_t::right_open(lo, cadmia::modeling::plus_inf);
            }

            // Case 4: lower is output and upper is +inf (right-open)
            // -> all states are in output phase up to +inf; reset to passive 0
            if (lower_output && s.is_upper_infinite()) {
                state_t result_state{phase_t::passive, 0};
                return state_i_t::closed(result_state, result_state);
            }

            // Any other configuration is invalid for an internal transition
            throw std::invalid_argument(
                "internal_transition: invalid phase interval for internal event");
        }

        // external_transition: process Add, Reset, or uncertain [Add, Reset] interval
        [[nodiscard]] static state_i_t
        external_transition(const state_i_t &state, const time_i_t &elapsed, const input_i_t &x) {
            if (state.is_empty()) {
                throw std::invalid_argument("external_transition: empty state interval");
            }
            if (x.is_empty()) {
                throw std::invalid_argument("external_transition: empty input interval");
            }

            // Elapsed time does not affect behavior (discrete event)
            (void)elapsed;

            // Handle uncertain input interval [Add, Reset]
            if (x.lower == input_event_t::add && x.upper == input_event_t::reset) {
                // Per spec: produce interval spanning passive lower (Add path) and output upper (Reset path)
                // Lower bound: Add path does NOT increment (stays passive, same count)
                // Upper bound: Add path DOES increment (+1) before Reset changes phase to output
                state_t lower_result{phase_t::passive, state.lower.count};
                // Preserve right-open upper if original state upper is infinite
                if (state.is_upper_infinite()) {
                    // Right-open to +inf with output phase at upper bound
                    state_t lo = lower_result;
                    return state_i_t::right_open(lo, cadmia::modeling::plus_inf);
                }
                state_t upper_result{phase_t::output, state.upper.count + 1};
                return state_i_t::closed(lower_result, upper_result);
            }

            // Singleton input interval
            if (x.lower != x.upper) {
                throw std::invalid_argument("external_transition: unsupported input interval form");
            }
            const input_event_t event = x.lower; // == x.upper

            if (event == input_event_t::add) {
                // Add event: increment count, stay passive
                state_t lower_result;
                lower_result.count = state.lower.count + 1;
                lower_result.phase = phase_t::passive;

                state_t upper_result;
                upper_result.count = state.upper.count + 1;
                upper_result.phase = phase_t::passive;

                return state_i_t::closed(lower_result, upper_result);
            } else if (event == input_event_t::reset) {
                // Reset event: keep count, transition to output phase
                state_t lower_result;
                lower_result.count = state.lower.count;
                lower_result.phase = phase_t::output;

                state_t upper_result;
                upper_result.count = state.upper.count;
                upper_result.phase = phase_t::output;

                return state_i_t::closed(lower_result, upper_result);
            } else {
                throw std::invalid_argument("external_transition: unknown event type");
            }
        }

        // output: return the current count when in output phase
        [[nodiscard]] static output_i_t output(const state_i_t &s) {
            if (s.is_empty()) {
                throw std::invalid_argument("output: empty state interval");
            }
            // Validate both endpoints are in output phase
            if (s.lower.phase != phase_t::output || s.upper.phase != phase_t::output) {
                throw std::invalid_argument("output: state must be in output phase");
            }

            return output_i_t::closed(s.lower.count, s.upper.count);
        }

        // time_advance: passive = infinite, output = 0
        [[nodiscard]] static time_i_t time_advance(const state_i_t &s) {
            if (s.is_empty()) {
                return time_i_t::empty_interval();
            }

            // Check phase of both endpoints
            const bool lower_passive = (s.lower.phase == phase_t::passive);
            const bool upper_passive = (s.upper.phase == phase_t::passive);
            const bool lower_output  = (s.lower.phase == phase_t::output);
            const bool upper_output  = (s.upper.phase == phase_t::output);

            // Both passive => infinite time advance (represented as empty interval)
            if (lower_passive && upper_passive) {
                return time_i_t::empty_interval();
            }

            // Both output => immediate time advance [0, 0]
            if (lower_output && upper_output) {
                return time_i_t::closed(time_t{}, time_t{});
            }

            // Mixed phases: lower passive, upper output => [0, +inf) right-open
            if (lower_passive && upper_output) {
                return time_i_t::right_open(time_t{}, cadmia::modeling::plus_inf);
            }

            // Mixed phases: lower output, upper passive => invalid state
            // (should not occur in valid simulation)
            throw std::invalid_argument("time_advance: invalid phase interval (output -> passive)");
        }
    };

} // namespace cadmia::iadevs
