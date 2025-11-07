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

#include <deque>
#include <stdexcept>
#include <string>
#include <type_traits>

// IA-DEVS Processor example model, following "Uncertainty on Discrete-Event
// System Simulation" (Processor_IA)
// This header defines a minimal state holding an interval TOCJ and a FIFO of
// job intervals, and the approximated transition/output/time functions:
// Δ_int, Δ_ext, Λ and TA for a Processor model with fixed processing time.

namespace cadmia::iadevs {

    class processor {
      public:
        using dec3     = cadmia::modeling::decimal<3>; // 1 ms resolution
        using job_id_t = int;                          // job identifier base type
        // Base types required by IADEVSAtomicModel concept
        using input_t  = job_id_t;   // input base type (job ID)
        using output_t = job_id_t;   // output base type (job ID)
        struct state_t {             // state base type
            dec3 tocj;               // in [0, proc_duration]
            std::deque<job_id_t> qj; // queue of job-id intervals

            // Comparison: order by tocj, then queue size, then queue elements
            [[nodiscard]] std::strong_ordering operator<=>(const state_t &other) const noexcept {
                if (auto cmp = tocj <=> other.tocj; cmp != 0) {
                    return cmp;
                }
                if (auto cmp = qj.size() <=> other.qj.size(); cmp != 0) {
                    return cmp;
                }
                for (std::size_t i = 0; i < qj.size(); ++i) {
                    if (auto cmp = qj[i] <=> other.qj[i]; cmp != 0) {
                        return cmp;
                    }
                }
                return std::strong_ordering::equal;
            }

            [[nodiscard]] bool operator==(const state_t &other) const noexcept = default;
        };
        using time_t = dec3; // time base type

        // Interval aliases for convenience (not required by concept)
        using input_i_t  = cadmia::modeling::interval<job_id_t>;
        using output_i_t = cadmia::modeling::interval<job_id_t>;
        using state_i_t  = cadmia::modeling::interval<state_t>;
        using time_i_t   = cadmia::modeling::interval<time_t>;

        // IA-DEVS state: TOCJ (time spent on current job) and queue of jobs (FIFO)

        // Approximated functions
        // internal(s): complete current job, dequeue head, reset TOCJ
        [[nodiscard]] static state_i_t internal_transition(const state_i_t &s) {
            if (s.is_empty()) {
                throw std::invalid_argument("internal_transition: empty state interval");
            }
            // Check if either endpoint has queue size <= 1 => output would be empty after pop
            if (s.lower.qj.size() <= 1 || s.upper.qj.size() <= 1) {
                return state_i_t::empty_interval();
            }

            state_t lower_state;
            lower_state.tocj = zero_time_interval().lower;
            if (s.lower.qj.empty()) {
                // Lower endpoint already empty, stays empty
                lower_state.qj = {};
            } else {
                // Pop from lower endpoint queue
                lower_state.qj = s.lower.qj;
                lower_state.qj.pop_front();
            }

            state_t upper_state;
            upper_state.tocj = zero_time_interval().upper;
            if (s.upper.qj.empty()) {
                // Upper endpoint already empty, stays empty
                upper_state.qj = {};
            } else {
                // Pop from upper endpoint queue
                upper_state.qj = s.upper.qj;
                upper_state.qj.pop_front();
            }

            return state_i_t::closed(lower_state, upper_state);
        }

        // external(state, elapsed, x): enqueue job identifier interval; start job if idle
        [[nodiscard]] static state_i_t
        external_transition(const state_i_t &state, const time_i_t &elapsed, const input_i_t &x) {
            validate_time(elapsed, "external_transition");

            state_t lower_state;
            // Update TOCJ for lower endpoint
            lower_state.tocj = state.lower.tocj + elapsed.lower;
            // Enqueue new job ID for lower endpoint
            lower_state.qj = state.lower.qj;
            lower_state.qj.push_back(x.lower);

            state_t upper_state;
            // Update TOCJ for upper endpoint
            upper_state.tocj = state.upper.tocj + elapsed.upper;
            // Enqueue new job ID for upper endpoint
            upper_state.qj = state.upper.qj;
            upper_state.qj.push_back(x.upper);

            return state_i_t::closed(lower_state, upper_state);
        }

        // output(s): ID of the job being completed (head of queue)
        [[nodiscard]] static output_i_t output(const state_i_t &s) {
            if (s.is_empty()) {
                throw std::invalid_argument(
                    "output: empty queue (should not be called when passive)");
            }
            return output_i_t::closed(s.lower.qj.front(), s.upper.qj.front());
        }

        // time_advance(s):
        // - If both endpoints' queues are empty => passive (empty interval)
        // - If lower endpoint queue is empty and upper is non-empty => right-open
        //   [processing_time - s.upper.tocj, +inf)
        // - Else (both non-empty) => remaining time: [proc,proc] - [tocj_l, tocj_u]
        [[nodiscard]] static time_i_t time_advance(const state_i_t &s) {
            if (s.is_empty()) {
                return time_i_t::empty_interval();
            }
            const bool lower_empty = s.lower.qj.empty();
            const bool upper_empty = s.upper.qj.empty();
            // Both empty -> passive
            if (lower_empty && upper_empty) {
                return time_i_t::empty_interval();
            }
            // Mixed case: lower empty, upper non-empty -> [proc - tocj_upper, +inf)
            if (lower_empty && !upper_empty) {
                const auto proc = processing_time();
                auto lb         = proc - s.upper.tocj;
                // Clamp to non-negative lower bound
                const time_t z{};
                if (lb < z) lb = z;
                return time_i_t::right_open(lb, cadmia::modeling::plus_inf);
            }
            // Safety: if upper empty (should not happen for valid intervals), treat as passive
            if (upper_empty) {
                return time_i_t::empty_interval();
            }
            const auto proc          = processing_time_interval();
            const auto tocj_interval = time_i_t::closed(s.lower.tocj, s.upper.tocj);
            const auto raw           = proc - tocj_interval;
            return clamp_non_negative(raw);
        }

      private:
        // Configuration: fixed processing time (250 ms)
        [[nodiscard]] static constexpr time_t processing_time() noexcept {
            return time_t::from_scaled(250); // 0.250 s
        }

        // Helpers
        [[nodiscard]] static constexpr time_i_t zero_time_interval() noexcept {
            return time_i_t::closed(time_t{}, time_t{});
        }

        [[nodiscard]] static constexpr time_i_t processing_time_interval() noexcept {
            const auto p = processing_time();
            return time_i_t::closed(p, p);
        }

        // Validate time interval is within [0, processing_time]
        static void validate_time(const time_i_t &t, const char *where) {
            if (t.is_empty())
                return; // empty carries semantics
            const time_t z{};
            const time_t ub = processing_time();
            if (t.lower < z || t.upper < z || t.lower > ub || t.upper > ub) {
                throw std::invalid_argument(std::string(where) + ": time outside [0, 0.250]");
            }
        }

        // Clamp TOCJ to [0, processing_time]
        [[nodiscard]] static time_i_t clamp_tocj_interval(const time_i_t &in) {
            if (in.is_empty())
                return in;
            time_i_t out{};
            const time_t z{};
            const time_t ub = processing_time();
            // Initialize with original endpoints
            out.lower = in.lower;
            out.upper = in.upper;
            // Lower bound clamp
            if (out.lower < z) {
                out.lower        = z;
                out.lower_closed = true;
            } else if (out.lower > ub) {
                out.lower        = ub;
                out.lower_closed = true;
            } else {
                out.lower_closed = in.lower_closed;
            }
            // Upper bound clamp
            if (out.upper < z) {
                out.upper        = z;
                out.upper_closed = true;
            } else if (out.upper > ub) {
                out.upper        = ub;
                out.upper_closed = true;
            } else {
                out.upper_closed = in.upper_closed;
            }
            return out;
        }

        // Clamp a generic time interval to non-negative times [0, +inf)
        [[nodiscard]] static time_i_t clamp_non_negative(const time_i_t &in) {
            if (in.is_empty())
                return in;
            time_i_t out{};
            const time_t z{};
            out.lower = in.lower;
            out.upper = in.upper;
            if (out.lower < z) {
                out.lower        = z;
                out.lower_closed = true;
            } else {
                out.lower_closed = in.lower_closed;
            }
            if (out.upper < z) {
                out.upper        = z;
                out.upper_closed = true;
            } else {
                out.upper_closed = in.upper_closed;
            }
            return out;
        }
    };

} // namespace cadmia::iadevs
