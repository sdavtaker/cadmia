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
                                typedef cadmia::modeling::decimal<3> dec3;                                // 1 ms resolution
                                typedef cadmia::modeling::interval<dec3> time_interval_t;                 // time intervals
                                typedef int job_id_t;                                                     // job identifier
                                typedef cadmia::modeling::interval<job_id_t> job_interval_t;              // interval of jobs

        // IA-DEVS state: TOCJ (time spent on current job) and queue of jobs (FIFO)
        struct state_t {
            time_interval_t tocj;                // in [0, proc_duration]
            std::deque<job_interval_t> qj;       // queue of job-id intervals
        };

        // Q_I element: state and elapsed time since last transition
        struct q_interval {
            state_t state;
            time_interval_t elapsed;
        };
        using q_interval_t = q_interval;

        // Approximated functions
        // internal(s): complete current job, dequeue head, reset TOCJ
        [[nodiscard]] static state_t internal_transition(const state_t &s) {
            if (s.qj.empty()) {
                throw std::invalid_argument("internal_transition: empty queue");
            }
            state_t ns{};
            ns.qj = s.qj;
            ns.qj.pop_front();
            ns.tocj = zero_time_interval();
            return ns;
        }

        // external(q, x): enqueue job identifier interval; start job if idle
        [[nodiscard]] static state_t external_transition(const q_interval_t &q,
                                                         const job_interval_t &x) {
            state_t ns = q.state;
            if (ns.qj.empty()) {
                // If idle, new job starts immediately
                ns.tocj = zero_time_interval();
            } else {
                // Accumulate processed time while busy; elapsed must be within [0, proc]
                validate_time(q.elapsed, "external_transition(elapsed)");
                ns.tocj = clamp_tocj_interval(ns.tocj + q.elapsed);
            }
            ns.qj.push_back(x);
            return ns;
        }

        // output(s): ID of the job being completed (head of queue)
        [[nodiscard]] static job_interval_t output(const state_t &s) {
            if (s.qj.empty()) {
                throw std::invalid_argument("output: empty queue (should not be called when passive)");
            }
            return s.qj.front();
        }

        // time_advance(s): if queue is empty => passive (empty interval), otherwise
        // remaining time to complete current job: [proc,proc] - TOCJ
        [[nodiscard]] static time_interval_t time_advance(const state_t &s) {
            if (s.qj.empty()) {
                // Passive: empty interval denotes no scheduled internal event
                return time_interval_t::empty_interval();
            }
            const auto proc = processing_time_interval();
            const auto raw  = proc - s.tocj;
            return clamp_non_negative(raw);
        }

      private:
        // Configuration: fixed processing time (250 ms)
        [[nodiscard]] static constexpr dec3 processing_time() noexcept {
            return dec3::from_scaled(250); // 0.250 s
        }

        // Helpers
        [[nodiscard]] static constexpr time_interval_t zero_time_interval() noexcept {
            return time_interval_t::closed(dec3{}, dec3{});
        }

        [[nodiscard]] static constexpr time_interval_t processing_time_interval() noexcept {
            const auto p = processing_time();
            return time_interval_t::closed(p, p);
        }

        // Validate time interval is within [0, processing_time]
        static void validate_time(const time_interval_t &t, const char *where) {
            if (t.is_empty())
                return; // empty carries semantics
            const dec3 z{};
            const dec3 ub = processing_time();
            if (t.lower < z || t.upper < z || t.lower > ub || t.upper > ub) {
                throw std::invalid_argument(std::string(where) + ": time outside [0, 0.250]");
            }
        }

        // Clamp TOCJ to [0, processing_time]
        [[nodiscard]] static time_interval_t clamp_tocj_interval(const time_interval_t &in) {
            if (in.is_empty())
                return in;
            time_interval_t out{};
            const dec3 z{};
            const dec3 ub = processing_time();
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
        [[nodiscard]] static time_interval_t clamp_non_negative(const time_interval_t &in) {
            if (in.is_empty())
                return in;
            time_interval_t out{};
            const dec3 z{};
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
