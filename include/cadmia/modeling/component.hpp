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
#include <cadmia/engine/simulator.hpp>
#include <cadmia/modeling/interval.hpp>

#include <functional>
#include <memory>
#include <stdexcept>

namespace cadmia::modeling {

    // Forward declaration — defined in coupled.hpp
    template <typename T> class CoupledModel;

    /**
     * ComponentSpec<T> describes one child engine inside a CoupledModel.
     *
     * Either atomic (holds a factory that creates an initialized IADEVSEngine<T>)
     * or coupled (holds a shared CoupledModel<T> for recursive coordinator construction).
     *
     * Mirrors Cadpya's ComponentSpec. Use the factory methods:
     *   ComponentSpec<T>::atomic(factory)
     *   ComponentSpec<T>::coupled(coupled_model)
     *
     * For atomic components, prefer the free function make_atomic_component<M>()
     * which builds the correctly typed factory for you.
     */
    template <typename T> class ComponentSpec {
      public:
        using time_i_t = cadmia::modeling::interval<T>;
        using engine_t = cadmia::engine::IADEVSEngine<T>;
        // Factory: given simulation start time t, returns a fully initialized engine.
        // The factory captures initial_state and initial_elapsed in its closure.
        using engine_factory_t = std::function<std::unique_ptr<engine_t>(time_i_t t)>;

        [[nodiscard]] bool is_atomic() const noexcept {
            return factory_ != nullptr;
        }
        [[nodiscard]] bool is_coupled() const noexcept {
            return coupled_model_ != nullptr;
        }

        [[nodiscard]] const engine_factory_t &factory() const {
            if (!factory_)
                throw std::logic_error("ComponentSpec: not an atomic component");
            return factory_;
        }

        [[nodiscard]] const CoupledModel<T> &coupled_model() const {
            if (!coupled_model_)
                throw std::logic_error("ComponentSpec: not a coupled component");
            return *coupled_model_;
        }

        static ComponentSpec atomic(engine_factory_t f) {
            if (!f)
                throw std::invalid_argument("ComponentSpec::atomic: factory must not be null");
            ComponentSpec s;
            s.factory_ = std::move(f);
            return s;
        }

        static ComponentSpec coupled(std::shared_ptr<CoupledModel<T>> model) {
            if (!model)
                throw std::invalid_argument("ComponentSpec::coupled: model must not be null");
            ComponentSpec s;
            s.coupled_model_ = std::move(model);
            return s;
        }

      private:
        engine_factory_t factory_{};
        std::shared_ptr<CoupledModel<T>> coupled_model_{};
    };

    /**
     * Convenience factory: build an atomic ComponentSpec<T> for model type M.
     *
     * Creates a factory closure that constructs and initializes a simulator<M>
     * given the simulation start time t. Captures initial_state and initial_elapsed
     * by value so the ComponentSpec is self-contained.
     */
    template <cadmia::IADEVSAtomicModel M>
    ComponentSpec<typename M::time_t>
    make_atomic_component(cadmia::modeling::interval<typename M::state_t> initial_state,
                          cadmia::modeling::interval<typename M::time_t> initial_elapsed) {
        using T        = typename M::time_t;
        using time_i_t = cadmia::modeling::interval<T>;

        auto factory = [initial_state, initial_elapsed](
                           time_i_t t) -> std::unique_ptr<cadmia::engine::IADEVSEngine<T>> {
            auto sim = std::make_unique<cadmia::engine::simulator<M>>();
            sim->init(initial_state, initial_elapsed, t);
            return sim;
        };
        return ComponentSpec<T>::atomic(std::move(factory));
    }

} // namespace cadmia::modeling
