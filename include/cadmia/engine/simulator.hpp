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

#include <cadmia/concepts/iadevs_atomic_model.hpp>
#include <cadmia/engine/engine.hpp>
#include <cadmia/modeling/interval.hpp>

#include <any>
#include <concepts>
#include <memory>
#include <optional>
#include <stdexcept>

namespace cadmia::engine {

    /**
     * IA-DEVS Simulator (skeleton)
     *
     * Implements Algorithm 1 from "Uncertainty on Discrete-Event System Simulation" (VWD21).
     * Template parameter M must satisfy the IADEVSAtomicModel concept. This class manages
     * the simulation of a single atomic IA-DEVS model, tracking state intervals and time
     * bounds as the simulation progresses.
     *
     * Public interface (from Algorithm 1):
     * - init: Initialize simulation with total state interval and current time
     * - star (*-function): Process internal event (advance simulation due to scheduled event)
     * - x (x-function): Process external event (ingest input at given time)
     *
     * Private helper:
     * - bound_ts: Apply time-bound functions to computed intervals (placeholder for now)
     */
    template <cadmia::IADEVSAtomicModel M>
    class simulator : public IADEVSEngine<typename M::time_t> {
      public:
        using model_t    = M;
        using state_t    = typename M::state_t;
        using time_t     = typename M::time_t;
        using input_t    = typename M::input_t;
        using output_t   = typename M::output_t;
        using state_i_t  = cadmia::modeling::interval<state_t>;
        using time_i_t   = cadmia::modeling::interval<time_t>;
        using input_i_t  = cadmia::modeling::interval<input_t>;
        using output_i_t = cadmia::modeling::interval<output_t>;

        // Default constructor
        constexpr simulator() noexcept = default;

        // Explicit constructor with atomic model (for future use)
        constexpr explicit simulator(const M &) noexcept {}

        /**
         * init-function: Initialize the simulator with a total state interval and time.
         *
         * According to Algorithm 1:
         * - q_state: sequential state interval component of total state q
         * - q_time: elapsed time interval component of total state q
         * - t: current simulation time interval
         * - state = q_state
         * - t_last = t - q_time
         * - t_next = t_last + TA(state)
         */
        constexpr void init(const state_i_t &q_state, const time_i_t &q_time,
                            const time_i_t &t) noexcept {
            state_  = q_state;
            t_last_ = t - q_time;
            t_next_ = t_last_ + M::time_advance(state_);
        }

        /**
         * *-function (star): Advance simulation due to internal event.
         *
         * According to Algorithm 1:
         * - Validate t ⊆ t_next (else raise error)
         * - Compute output: y = Λ(state)
         * - Update state: state = Δ_int(state)
         * - Update times: t_last = t, t_next = t_last + TA(state)
         * - Return Optional<output_i_t> containing y
         */
        [[nodiscard]] constexpr std::optional<output_i_t> star(const time_i_t &t) {
            // Validate t ⊆ t_next (Algorithm 1 invariant check)
            if (!t.is_subset_of(t_next_)) {
                throw std::logic_error("star: time t is not a subset of t_next");
            }

            // Compute output: y = Λ(state)
            output_i_t y = M::output(state_);

            // Apply internal transition: state = Δ_int(state)
            state_ = M::internal_transition(state_);

            // Update times: t_last = t, t_next = t_last + TA(state)
            t_last_ = t;
            t_next_ = t_last_ + M::time_advance(state_);

            // Return Optional containing y
            return std::optional<output_i_t>{y};
        }

        /**
         * x-function: Process external event (Algorithm 1).
         *
         * Computes local elapsed time t_local from t, t_last, t_next.
         * Confluent case (t intersects t_last): clamps t_local lower bound to 0.
         * Applies Δ_ext(⟨state, t_local⟩, x), then updates t_last and t_next.
         */
        constexpr void x(const input_i_t &x, const time_i_t &t) noexcept {
            // Compute local elapsed time interval t_local per Algorithm 1
            time_i_t t_local{};

            // Upper end: t.upper - t_next.lower
            // Handle infinities similarly to interval subtraction endpoint logic
            if (t.upper_inf_sign == +1 || t_next_.lower_inf_sign == -1) {
                t_local.upper_inf_sign = +1;
            } else if (t.upper_inf_sign == -1 || t_next_.lower_inf_sign == +1) {
                t_local.upper_inf_sign = -1;
            } else {
                // Both finite
                t_local.upper          = t.upper - t_next_.lower;
                t_local.upper_inf_sign = 0;
            }
            t_local.upper_closed = t.upper_closed && !t_next_.lower_closed;

            // Lower end depends on overlap with t_last_
            if (t.intersects(t_last_)) {
                // Confluent case: event coincides with last event time
                t_local.lower          = time_t{};
                t_local.lower_inf_sign = 0;
                t_local.lower_closed   = true;
            } else {
                // t.lower - t_last.upper
                if (t.lower_inf_sign == +1 || t_last_.upper_inf_sign == -1) {
                    t_local.lower_inf_sign = +1;
                } else if (t.lower_inf_sign == -1 || t_last_.upper_inf_sign == +1) {
                    t_local.lower_inf_sign = -1;
                } else {
                    t_local.lower          = t.lower - t_last_.upper;
                    t_local.lower_inf_sign = 0;
                }
                t_local.lower_closed = t.lower_closed && t_last_.upper_closed;
            }

            // Apply external transition: state = Δ_ext(<state, t_local>, x)
            state_ = M::external_transition(state_, t_local, x);

            // Update times: t_last = t; t_next = t_last + TA(state)
            t_last_ = t;
            t_next_ = t_last_ + M::time_advance(state_);
        }

        // Typed accessors
        [[nodiscard]] constexpr const state_i_t &state() const noexcept {
            return state_;
        }
        [[nodiscard]] time_i_t t_last() const noexcept override {
            return t_last_;
        }
        [[nodiscard]] time_i_t t_next() const noexcept override {
            return t_next_;
        }

        // IADEVSEngine<time_t> virtual interface — type-erased for coordinator use

        void advance_t_next_past_limit(const time_i_t &limit) override {
            if (t_next_.is_empty() || !t_next_.intersects(limit))
                return;
            if (limit.is_punctual() &&
                time_i_t::endpoint_equal(t_next_.lower, t_next_.lower_inf_sign, limit.lower,
                                         limit.lower_inf_sign)) {
                t_next_.lower_closed = false;
            } else if (!limit.is_punctual()) {
                t_next_.lower          = limit.upper;
                t_next_.lower_inf_sign = limit.upper_inf_sign;
                t_next_.lower_closed   = !limit.upper_closed;
            }
        }

        [[nodiscard]] std::optional<std::any> engine_star(const time_i_t &t) override {
            auto result = star(t);
            if (!result.has_value())
                return std::nullopt;
            return std::any{std::move(*result)};
        }

        void engine_x(std::any x_val, const time_i_t &t) override {
            x(std::any_cast<input_i_t>(x_val), t);
        }

        [[nodiscard]] std::unique_ptr<IADEVSEngine<typename M::time_t>> clone() const override {
            return std::make_unique<simulator<M>>(*this);
        }

      private:
        /**
         * Check if interval a is a subset of interval b (a ⊆ b).
         * Used for validating the invariant in star function.
         */
        // Removed local is_subset; using interval::is_subset_of instead.

        state_i_t state_{}; // Current sequential state interval
        time_i_t t_last_{}; // Time of last processed event
        time_i_t t_next_{}; // Time of next scheduled internal event
    };

} // namespace cadmia::engine
