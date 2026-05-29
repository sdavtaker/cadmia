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

#include <cadmia/modeling/interval.hpp>

#include <compare>
#include <limits>
#include <stdexcept>

// IA-DEVS Accumulator model.
//
// Accumulates integer inputs and immediately outputs the running sum.
//
// State: (phase, total) where phase is PASSIVE or OUTPUT.
// Input: int  (any non-zero value means "add"; convention mirrors CDBoost)
// Output: int (running total)
//
// Behavior:
//   external_transition: total += x, phase = OUTPUT
//   internal_transition: phase = PASSIVE, total preserved
//   output:              interval<int> closed(total.lower, total.upper)
//   time_advance:
//     (PASSIVE, PASSIVE) → passive (empty interval)
//     (OUTPUT,  OUTPUT)  → [0, 0]
//     (PASSIVE, OUTPUT)  → [0, +inf)   (uncertain which phase)
//     (OUTPUT,  PASSIVE) → invalid (violates ordering)

namespace cadmia::iadevs {

    class accumulator {
      public:
        using dec3 = double;

        enum class phase_t : int { passive = 0, output = 1 };

        struct state_t {
            phase_t phase{phase_t::passive};
            int total{0};

            constexpr state_t() noexcept = default;
            constexpr state_t(phase_t p, int t) noexcept : phase(p), total(t) {}

            [[nodiscard]] std::strong_ordering operator<=>(const state_t &other) const noexcept {
                if (auto cmp = phase <=> other.phase; cmp != 0)
                    return cmp;
                return total <=> other.total;
            }
            bool operator==(const state_t &) const noexcept = default;
        };

        using time_t   = dec3;
        using input_t  = int;
        using output_t = int;

        using state_i_t  = cadmia::modeling::interval<state_t>;
        using time_i_t   = cadmia::modeling::interval<time_t>;
        using input_i_t  = cadmia::modeling::interval<input_t>;
        using output_i_t = cadmia::modeling::interval<output_t>;

        // ── Model functions (static, satisfies IADEVSAtomicModel) ─────────────

        static state_i_t internal_transition(const state_i_t &s) {
            // Phase → PASSIVE, total preserved
            return state_i_t::closed(state_t{phase_t::passive, s.lower.total},
                                     state_t{phase_t::passive, s.upper.total});
        }

        static state_i_t external_transition(const state_i_t &s, const time_i_t & /*elapsed*/,
                                             const input_i_t &x) {
            // total += x, phase → OUTPUT
            return state_i_t::closed(state_t{phase_t::output, s.lower.total + x.lower},
                                     state_t{phase_t::output, s.upper.total + x.upper});
        }

        static output_i_t output(const state_i_t &s) {
            return output_i_t::closed(s.lower.total, s.upper.total);
        }

        static time_i_t time_advance(const state_i_t &s) {
            if (s.is_empty())
                return time_i_t::empty_interval();

            const auto lo_phase = s.lower.phase;
            const auto hi_phase = s.upper.phase;

            if (lo_phase == phase_t::passive && hi_phase == phase_t::passive)
                return time_i_t::empty_interval(); // passive

            if (lo_phase == phase_t::output && hi_phase == phase_t::output)
                return time_i_t::closed(time_t{}, time_t{}); // [0, 0]

            if (lo_phase == phase_t::passive && hi_phase == phase_t::output)
                return time_i_t::right_open(time_t{},
                                            std::numeric_limits<time_t>::infinity()); // [0, +inf)

            // output < passive violates interval ordering
            throw std::logic_error("accumulator: lower phase OUTPUT > upper phase PASSIVE");
        }
    };

} // namespace cadmia::iadevs
