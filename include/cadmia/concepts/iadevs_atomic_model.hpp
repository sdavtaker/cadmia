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

#include <concepts>
#include <type_traits>

/**
 * Modern C++23 concepts for IA-DEVS (Interval-Based DEVS) atomic models.
 *
 * An IA-DEVS atomic model extends classical DEVS by using intervals to represent
 * uncertainty in state, time, and values. As described in "Uncertainty on
 * Discrete-Event System Simulation" (VWD21), an IA-DEVS atomic model requires:
 *
 * - Base type aliases: state_t, time_t, input_t, output_t (not interval types)
 * - Static methods that operate on intervals of these base types:
 *   - internal_transition(state_interval) → state_interval
 *   - external_transition(state_interval, time_interval, input_interval) → state_interval
 *   - output(state_interval) → output_interval
 *   - time_advance(state_interval) → time_interval
 *
 * The concept validates both the base type declarations and the function signatures,
 * ensuring proper parameter counts, types based on declared types, and return types.
 *
 * Reference: https://cell-devs-02.sce.carleton.ca/publications/2021/VWD21/
 */

namespace cadmia {

    /**
     * Concept: IADEVSAtomicModel
     *
     * Defines the requirements for an IA-DEVS atomic model type.
     * A model must provide base type aliases:
     *   - state_t: base state type (scalar or struct)
     *   - time_t: base time type (scalar)
     *   - input_t: base input type (scalar)
     *   - output_t: base output type (scalar)
     *
     * And static methods operating on intervals:
     *   - internal_transition(interval<state_t>) → interval<state_t>
     *   - external_transition(interval<state_t>, interval<time_t>, interval<input_t>) →
     * interval<state_t>
     *   - output(interval<state_t>) → interval<output_t>
     *   - time_advance(interval<state_t>) → interval<time_t>
     *
     * The concept validates:
     * 1. Required type aliases exist and are copyable
     * 2. Required static methods exist with correct signatures
     * 3. Functions receive and return proper interval types
     */
    template <typename M>
    concept IADEVSAtomicModel =
        requires {
            // Required type aliases
            typename M::state_t;
            typename M::time_t;
            typename M::input_t;
            typename M::output_t;
        } &&
        // Base types must be copyable
        std::copyable<typename M::state_t> && std::copyable<typename M::time_t> &&
        std::copyable<typename M::input_t> && std::copyable<typename M::output_t> &&
        // Validate function signatures with interval parameters and returns
        requires(const cadmia::modeling::interval<typename M::state_t> &state_interval,
                 const cadmia::modeling::interval<typename M::time_t> &time_interval,
                 const cadmia::modeling::interval<typename M::input_t> &input_interval) {
            // internal_transition: interval<state_t> → interval<state_t>
            {
                M::internal_transition(state_interval)
            } -> std::convertible_to<cadmia::modeling::interval<typename M::state_t>>;

            // external_transition: (interval<state_t>, interval<time_t>, interval<input_t>) →
            // interval<state_t>
            {
                M::external_transition(state_interval, time_interval, input_interval)
            } -> std::convertible_to<cadmia::modeling::interval<typename M::state_t>>;

            // output: interval<state_t> → interval<output_t>
            {
                M::output(state_interval)
            } -> std::convertible_to<cadmia::modeling::interval<typename M::output_t>>;

            // time_advance: interval<state_t> → interval<time_t>
            {
                M::time_advance(state_interval)
            } -> std::convertible_to<cadmia::modeling::interval<typename M::time_t>>;
        };

} // namespace cadmia
