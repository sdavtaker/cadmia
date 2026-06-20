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

#include <any>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace cadmia::engine {

    /**
     * Type-erased translation function: maps one engine's output interval (as std::any)
     * to another engine's input interval (as std::any). Stored in CoupledModel per (src, dst) pair.
     */
    using translation_fn = std::function<std::any(std::any)>;

    /**
     * Wraps a typed Z-function (SrcOutput -> DstInput) into a translation_fn.
     * The user writes typed lambdas; this erases the boundary at CoupledModel construction time.
     * Throws std::bad_any_cast at runtime if the wrong output type arrives — catches wiring errors.
     */
    template <typename SrcOutput, typename DstInput>
    translation_fn make_translation(std::function<DstInput(SrcOutput)> z) {
        return [z](std::any y) -> std::any { return std::any{z(std::any_cast<SrcOutput>(y))}; };
    }

    /**
     * Identity translation for same-type couplings (source_output_t == dest_input_t).
     * The most common case in portless single-type coupled models.
     */
    inline translation_fn identity_translation() {
        return [](std::any y) -> std::any { return y; };
    }

    /**
     * Abstract engine base class, parameterized on time scalar type T.
     *
     * Both simulator<M> and Coordinator inherit from this, enabling the coordinator
     * to store a heterogeneous collection of child engines as
     * std::vector<std::unique_ptr<IADEVSEngine<T>>>.
     *
     * Input/output interval types are erased via std::any. The concrete subclass
     * downcasts in engine_x() and upcasts in engine_star().
     */
    template <typename T> class IADEVSEngine {
      public:
        using time_i_t = cadmia::modeling::interval<T>;

        virtual ~IADEVSEngine() = default;

        /**
         * Returns the next scheduled internal event time interval.
         * Returns empty_interval() when the engine is passive.
         */
        [[nodiscard]] virtual time_i_t t_next() const = 0;

        /**
         * Returns the time of the last processed event.
         */
        [[nodiscard]] virtual time_i_t t_last() const = 0;

        /**
         * Type-erased *-function (Algorithm 1/2).
         * Returns {formatted_output_string, output_interval_as_any}, or {nullopt, nullopt}
         * if passive. The string is formatted while the output type is still concrete,
         * so callers never need to downcast std::any to produce a log entry.
         */
        [[nodiscard]] virtual std::pair<std::optional<std::string>, std::optional<std::any>>
        engine_star(const time_i_t &t) = 0;

        /**
         * Type-erased x-function (Algorithm 1/2).
         * x_val must hold the correct input_i_t for this engine; throws std::bad_any_cast
         * otherwise.
         */
        virtual void engine_x(std::any x_val, const time_i_t &t) = 0;

        /**
         * Exclude the given limit interval from this engine's t_next lower bound.
         * Used by Coordinator for skip branches (Algorithm 3):
         *   - Punctual limit [v,v]: open the lower bound at v.
         *   - Non-punctual limit: advance lower bound to limit.upper.
         */
        virtual void advance_t_next_past_limit(const time_i_t &limit) = 0;

        /**
         * Deep-copy this engine. Used by RootCoordinator for BFS branch isolation.
         */
        [[nodiscard]] virtual std::unique_ptr<IADEVSEngine<T>> clone() const = 0;

        /**
         * Structural equality: true iff other has the same concrete type and identical
         * state, t_last, and t_next. Used by RootCoordinator to deduplicate BFS branches
         * whose futures are deterministic and identical.
         */
        [[nodiscard]] virtual bool engine_equals(const IADEVSEngine<T> &other) const = 0;
    };

} // namespace cadmia::engine
