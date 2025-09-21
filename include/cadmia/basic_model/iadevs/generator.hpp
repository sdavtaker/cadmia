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
        // Time is decimal with 3 fractional digits (milliseconds resolution).
        static constexpr int default_rounding_granularity_units = 100; // e.g., 100 → 0.01
        using dec3    = cadmia::modeling::decimal<3>;                  // convenience alias
        using time_t  = cadmia::modeling::interval<dec3>;              // time in R^+_I (interval)
        using state_t = cadmia::modeling::interval<dec3>;              // state in R^+_I (interval)
        struct q_interval {                                            // Q_I = S_I × R^+_I
            state_t state;
            time_t elapsed;
        };

        // Clearer alias for Q_I tuple
        using q_interval_t = q_interval; // _t alias

        // Alias for numeric output interval
        using output_value_t = cadmia::modeling::interval<dec3>;
        using input_value_t  = cadmia::modeling::interval<dec3>;

        // Approximated functions (operate on intervals), matching Section 4.
        // internal(s) = [0,0] (validate input is within bounds even if ignored)
        [[nodiscard]] static state_t internal_transition(const state_t &s) {
            validate_state_interval(s, "internal_transition", "state");
            return state_t::closed(dec3{}, dec3{});
        }

        // external(q, x): validate inputs in [0, 1.005], then sum; validate result stays within
        // bounds.
        [[nodiscard]] static state_t external_transition(const q_interval_t &q,
                                                         const input_value_t &x) {
            (void)x; // no inputs used
            validate_state_interval(q.state, "external_transition", "q.state");
            validate_state_interval(q.elapsed, "external_transition", "q.elapsed");
            const auto sum = q.state + q.elapsed;
            validate_state_interval(sum, "external_transition", "state result");
            return sum;
        }

        // output(s) = [1.997, 2.003]
        [[nodiscard]] static output_value_t output(const state_t &) {
            return output_value_t::closed(dec3::from_scaled(1997), dec3::from_scaled(2003));
        }

        // time_advance(s): validate state, then return [earliest, latest] per IA-DEVS
        [[nodiscard]] static time_t time_advance(const state_t &s) {
            validate_state_interval(s, "time_advance", "state");
            const auto period = time_t::closed(dec3::from_scaled(997), dec3::from_scaled(1005));
            const auto raw    = period - s; // { l - e | l∈period, e∈s }
            return clamp_time_interval(raw);
        }

      private:
        // Helpers
        static constexpr dec3 zero_time() noexcept {
            return dec3{};
        }

        // Validate state/time intervals are within [0, 1.005]. Throw if out of bounds.
        static void validate_state_interval(const state_t &in, const char *where,
                                            const char *name) {
            if (in.is_empty())
                return; // empty carries its own semantics
            const dec3 z{};
            const dec3 ub = dec3::from_scaled(1005);
            if (in.lower < z || in.upper < z || in.lower > ub || in.upper > ub) {
                throw std::invalid_argument(std::string(where) + ": " + name +
                                            " outside [0, 1.005]");
            }
        }

        // Apply bounds to R^+_I for time advances: intersect with [0, +inf)
        [[nodiscard]] static time_t clamp_time_interval(const time_t &in) {
            if (in.is_empty())
                return in;
            time_t out{};
            const dec3 z{};
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
