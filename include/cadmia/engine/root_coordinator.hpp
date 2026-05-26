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

#include <cadmia/engine/coordinator.hpp>
#include <cadmia/modeling/coupled.hpp>

#include <any>
#include <deque>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace cadmia::engine {

    /**
     * Raised when BFS simulation exceeds max_branches — indicates exponential blowup.
     */
    struct SimulationLimitError : std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    /**
     * One entry in the simulation log.
     * kind: "atomic", "coupled", or "skip"
     */
    template <typename T> struct LogEntry {
        int step{};
        std::string branch;
        std::string kind; // "atomic" | "coupled" | "skip"
        std::optional<std::string> parent_branch;
        std::string time;                   // str(limit interval)
        std::string component;              // engine_name, empty for skip
        std::optional<std::string> output;  // str(component_output), nullopt for skip/passive
        std::optional<std::any> raw_output; // typed output value for caller inspection
    };

    /**
     * RootCoordinator — Algorithm 4 from VWD21.
     *
     * Drives the top-level BFS simulation loop over a CoupledModel.
     * Returns a vector<LogEntry> recording every step across all branches.
     */
    template <typename T> class RootCoordinator {
      public:
        using time_i_t = cadmia::modeling::interval<T>;
        using model_t  = cadmia::modeling::CoupledModel<T>;
        using coord_t  = coordinator<T>;
        using log_t    = LogEntry<T>;

        /**
         * Simulate coupled_model starting at time t.
         *
         * max_steps:   total steps across all branches; stops gracefully when hit.
         * max_branches: active queue limit; throws SimulationLimitError if exceeded.
         */
        [[nodiscard]] std::vector<log_t> simulate(const model_t &coupled_model, const time_i_t &t,
                                                  int max_steps = 10000, int max_branches = 1000) {
            auto root_coord =
                std::make_unique<coord_t>(std::make_shared<const model_t>(coupled_model));
            root_coord->init(t);

            struct Branch {
                std::string branch_id;
                std::unique_ptr<coord_t> coordinator;
                std::optional<std::string> parent_branch_id;
                int step{};
            };

            std::deque<Branch> queue;
            queue.push_back({"0", std::move(root_coord), std::nullopt, 0});

            std::vector<log_t> log;
            int total_steps = 0;

            while (!queue.empty()) {
                if (total_steps >= max_steps)
                    break;

                // Passive — discard
                if (queue.front().coordinator->t_next().is_empty()) {
                    queue.pop_front();
                    continue;
                }

                const auto t_current = queue.front().coordinator->t_next();
                auto actions         = queue.front().coordinator->compute_branches(t_current);

                if (actions.empty()) {
                    queue.pop_front();
                    continue;
                }

                if (actions.size() == 1) {
                    // No branching — execute in place
                    auto &branch       = queue.front();
                    const auto &action = actions[0];
                    auto [comp_out, _] = branch.coordinator->execute_branch(action);
                    ++total_steps;

                    log.push_back(make_entry(branch.step, branch.branch_id, branch.parent_branch_id,
                                             action, comp_out, *branch.coordinator));

                    branch.step += 1;

                    if (branch.coordinator->t_next().is_empty())
                        queue.pop_front();

                } else {
                    // Branching — pop original, clone state for each action
                    if (queue.size() - 1 + actions.size() >
                        static_cast<std::size_t>(max_branches)) {
                        throw SimulationLimitError("Simulation exceeded max_branches limit (" +
                                                   std::to_string(max_branches) + ") after " +
                                                   std::to_string(total_steps) + " steps");
                    }

                    Branch original = std::move(queue.front());
                    queue.pop_front();

                    for (int i = 0; i < static_cast<int>(actions.size()); ++i) {
                        if (total_steps >= max_steps)
                            break;

                        const auto &action = actions[i];
                        // Clone the original (pre-execution) coordinator for each branch
                        auto clone_eng = original.coordinator->clone();
                        auto *clone    = dynamic_cast<coord_t *>(clone_eng.get());
                        if (clone == nullptr)
                            throw std::logic_error(
                                "root_coordinator: clone() returned unexpected engine type");
                        clone_eng.release();
                        auto clone_ptr = std::unique_ptr<coord_t>(clone);

                        auto [comp_out, _2] = clone_ptr->execute_branch(action);
                        ++total_steps;

                        const std::string new_id = original.branch_id + "." + std::to_string(i);
                        log.push_back(make_entry(original.step, new_id,
                                                 std::optional<std::string>{original.branch_id},
                                                 action, comp_out, *clone_ptr));

                        if (!clone_ptr->t_next().is_empty()) {
                            queue.push_back(Branch{new_id, std::move(clone_ptr),
                                                   std::optional<std::string>{original.branch_id},
                                                   original.step + 1});
                        }
                    }
                }
            }

            return log;
        }

      private:
        // Format an endpoint value as string. Uses raw_value() for decimal types,
        // falls back to std::to_string for arithmetic types.
        template <typename U> static std::string val_str(const U &v) {
            if constexpr (requires { v.raw_value(); })
                return std::to_string(v.raw_value());
            else
                return std::to_string(v);
        }

        static std::string interval_str(const time_i_t &iv) {
            std::string s;
            s += iv.lower_closed ? "[" : "(";
            if (iv.lower_inf_sign == -1)
                s += "-inf";
            else if (iv.lower_inf_sign == +1)
                s += "+inf";
            else
                s += val_str(iv.lower);
            s += ", ";
            if (iv.upper_inf_sign == +1)
                s += "+inf";
            else if (iv.upper_inf_sign == -1)
                s += "-inf";
            else
                s += val_str(iv.upper);
            s += iv.upper_closed ? "]" : ")";
            return s;
        }

        log_t make_entry(int step, const std::string &branch_id,
                         const std::optional<std::string> &parent_id, const BranchAction<T> &action,
                         const std::optional<std::any> &comp_out, const coord_t &coord) {
            std::string kind;
            if (action.engine_name.empty()) {
                kind = "skip";
            } else {
                const auto &eng = coord.engines().at(action.engine_name);
                kind = (dynamic_cast<const coord_t *>(eng.get()) != nullptr) ? "coupled" : "atomic";
            }

            return log_t{
                .step          = step,
                .branch        = branch_id,
                .kind          = kind,
                .parent_branch = parent_id,
                .time          = interval_str(action.limit),
                .component     = action.engine_name,
                .output        = comp_out ? std::optional<std::string>{"<output>"} : std::nullopt,
                .raw_output    = comp_out,
            };
        }
    };

} // namespace cadmia::engine
