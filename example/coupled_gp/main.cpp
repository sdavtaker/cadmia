// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2025, Damian Vicino
 * Carleton University
 * All rights reserved.
 */

// Example: generator feeding an accumulator via a typed translation.
//
// The generator fires periodically; each output (interval<double>) is translated
// to an integer tick (interval<int>) and forwarded to the accumulator, which
// sums the received ticks and immediately outputs the running total.  A single
// generator means compute_branches never produces more than one branch, so the
// simulation trace stays linear throughout.
//
// This validates the full stack:
//   CoupledModel → Coordinator → RootCoordinator

#include <cadmia/basic_model/iadevs/accumulator.hpp>
#include <cadmia/basic_model/iadevs/generator.hpp>
#include <cadmia/engine/root_coordinator.hpp>
#include <cadmia/modeling/coupled.hpp>

#include <iostream>
#include <string>
#include <unordered_set>

using cadmia::engine::make_translation;
using cadmia::engine::RootCoordinator;
using cadmia::iadevs::accumulator;
using cadmia::iadevs::generator;
using cadmia::modeling::CoupledModel;
using cadmia::modeling::make_atomic_component;

using T        = generator::time_t;
using time_i_t = cadmia::modeling::interval<T>;

int main() {
    const T zero{};
    auto zero_i = time_i_t::closed(zero, zero);

    // Initial states
    auto gen_s0 = cadmia::modeling::interval<generator::state_t>::closed(zero, zero);
    accumulator::state_t as{};
    auto acc_s0 = cadmia::modeling::interval<accumulator::state_t>::closed(as, as);

    // Translation: generator output (interval<double>) → accumulator input (interval<int>)
    // Each generator tick contributes 1 to the running sum.
    auto gen_to_acc = make_translation<generator::output_i_t, accumulator::input_i_t>(
        [](generator::output_i_t y) -> accumulator::input_i_t {
            return accumulator::input_i_t::closed(static_cast<int>(y.lower),
                                                  static_cast<int>(y.upper));
        });

    // Select: lexicographic minimum of imminent component names
    auto select_first = [](const std::unordered_set<std::string> &names) {
        return *std::min_element(names.begin(), names.end());
    };

    // Build model: gen → acc
    CoupledModel<T>::component_map comps;
    comps.emplace("acc", make_atomic_component<accumulator>(acc_s0, zero_i));
    comps.emplace("gen", make_atomic_component<generator>(gen_s0, zero_i));

    CoupledModel<T>::influencer_map infl;
    infl["gen"] = {};
    infl["acc"] = {"gen"};

    CoupledModel<T>::translation_map trans;
    trans[{"gen", "acc"}] = gen_to_acc;

    CoupledModel<T> model(std::move(comps), std::move(infl), std::move(trans), select_first, zero);

    // Run 10 steps
    RootCoordinator<T> rc;
    auto log = rc.simulate(model, zero_i, /*max_steps=*/10);

    // Print log
    std::cout << "step  branch  kind    component  time\n";
    for (const auto &e : log) {
        std::cout << e.step << "     " << e.branch << "       " << e.kind << "   "
                  << (e.component.empty() ? "-" : e.component) << "   " << e.time << "\n";
    }
    return 0;
}
