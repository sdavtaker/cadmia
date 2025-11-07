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

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>

// IA-DEVS Generator example model, following "Uncertainty on Discrete-Event
// System Simulation" (Generator_IA)
// This header defines a minimal interval utility and the approximated
// transition functions: Δ_int, Δ_ext, Λ and TA for a Generator model.

namespace cadmia::iadevs {

    // IA-DEVS Generator with fixed time type and fixed value interval.
    class generator {
      public:
        // Base types required by IADEVSAtomicModel concept
        using dec3     = cadmia::modeling::decimal<3>; // convenience alias
        using time_t   = dec3;                         // time base type
        using state_t  = dec3;                         // state base type
        using input_t  = dec3;                         // input base type
        using output_t = dec3;                         // output base type

        // Interval aliases for convenience (not required by concept)
        using time_i_t   = cadmia::modeling::interval<time_t>;
        using state_i_t  = cadmia::modeling::interval<state_t>;
        using input_i_t  = cadmia::modeling::interval<input_t>;
        using output_i_t = cadmia::modeling::interval<output_t>;

        // Time is decimal with 3 fractional digits (milliseconds resolution).
        static constexpr int default_rounding_granularity_units = 100; // e.g., 100 → 0.01

        // Approximated functions (operate on intervals), matching Section 4.
        // internal(s) = [0,0] (validate input is within bounds even if ignored)
        [[nodiscard]] static state_i_t internal_transition(const state_i_t &s) {
            validate_state_interval(s, "internal_transition", "state");
            return state_i_t::closed(state_t{}, state_t{});
        }

        // external(state, elapsed, x): validate inputs in [0, 1.005], then sum
        [[nodiscard]] static state_i_t
        external_transition(const state_i_t &state, const time_i_t &elapsed, const input_i_t &x) {
            (void)x; // no inputs used
            validate_state_interval(state, "external_transition", "state");
            validate_state_interval(elapsed, "external_transition", "elapsed");
            const auto sum = state + elapsed;
            validate_state_interval(sum, "external_transition", "state result");
            return sum;
        }

        // output(s) = [1.997, 2.003]
        [[nodiscard]] static output_i_t output(const state_i_t &) {
            return output_i_t::closed(output_t::from_scaled(1997), output_t::from_scaled(2003));
        }

        // time_advance(s): validate state, then return [earliest, latest] per IA-DEVS
        [[nodiscard]] static time_i_t time_advance(const state_i_t &s) {
            validate_state_interval(s, "time_advance", "state");
            const auto period =
                time_i_t::closed(time_t::from_scaled(997), time_t::from_scaled(1005));
            const auto raw = period - s; // { l - e | l∈period, e∈s }
            return clamp_time_interval(raw);
        }

      private:
        // Helpers
        static constexpr time_t zero_time() noexcept {
            return time_t{};
        }

        // Validate state/time intervals are within [0, 1.005]. Throw if out of bounds.
        static void validate_state_interval(const state_i_t &in, const char *where,
                                            const char *name) {
            if (in.is_empty())
                return; // empty carries its own semantics
            const state_t z{};
            const state_t ub = state_t::from_scaled(1005);
            if (in.lower < z || in.upper < z || in.lower > ub || in.upper > ub) {
                throw std::invalid_argument(std::string(where) + ": " + name +
                                            " outside [0, 1.005]");
            }
        }

        // Apply bounds to R^+_I for time advances: intersect with [0, +inf)
        [[nodiscard]] static time_i_t clamp_time_interval(const time_i_t &in) {
            if (in.is_empty())
                return in;
            time_i_t out{};
            const time_t z{};
            // Lower bound clamp
            out.lower = (in.lower < z) ? z : in.lower;
            // If upper < 0, clamp to 0 as well
            out.upper = (in.upper < z) ? z : in.upper;
            // Closure at 0 becomes closed if we clamped through 0 (intersection semantics)
            out.lower_closed = (in.lower < z) ? true : in.lower_closed;
            out.upper_closed = (in.upper < z) ? true : in.upper_closed;
            return out;
        }
    };

} // namespace cadmia::iadevs
