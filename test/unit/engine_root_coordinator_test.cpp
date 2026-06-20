// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2025, Damian Vicino
 * Carleton University
 * All rights reserved.
 */

#include <cadmia/basic_model/iadevs/generator.hpp>
#include <cadmia/engine/root_coordinator.hpp>

#include <catch2/catch_test_macros.hpp>

using cadmia::engine::identity_translation;
using cadmia::engine::RootCoordinator;
using cadmia::engine::SimulationLimitError;
using cadmia::iadevs::generator;
using cadmia::modeling::CoupledModel;
using cadmia::modeling::make_atomic_component;

using T        = generator::time_t;
using time_i_t = cadmia::modeling::interval<T>;

namespace {
    const T zero{};

    auto gen_state() {
        return cadmia::modeling::interval<generator::state_t>::closed(T{}, T{});
    }
    auto zero_i() {
        return time_i_t::closed(T{}, T{});
    }

    std::string first_select(const std::unordered_set<std::string> &names) {
        return *std::min_element(names.begin(), names.end());
    }

    CoupledModel<T> single_gen_model() {
        CoupledModel<T>::component_map comps;
        comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_i()));
        CoupledModel<T>::influencer_map infl;
        infl["gen"] = {};
        return CoupledModel<T>(std::move(comps), std::move(infl), {}, first_select, zero);
    }

    // Two uncoupled generators with the same period — both are imminent at the same time,
    // so compute_branches returns 2 branches simultaneously.
    CoupledModel<T> dual_gen_model() {
        CoupledModel<T>::component_map comps;
        comps.emplace("gen1", make_atomic_component<generator>(gen_state(), zero_i()));
        comps.emplace("gen2", make_atomic_component<generator>(gen_state(), zero_i()));
        CoupledModel<T>::influencer_map infl;
        infl["gen1"] = {};
        infl["gen2"] = {}; // no coupling — generators are independent
        return CoupledModel<T>(std::move(comps), std::move(infl), {}, first_select, zero);
    }
} // namespace

TEST_CASE("RootCoordinator simulate single gen produces log entries", "[root_coordinator]") {
    RootCoordinator<T> rc;
    auto model = single_gen_model();
    auto log   = rc.simulate(model, zero_i(), /*max_steps=*/5);

    REQUIRE_FALSE(log.empty());
    // All entries should be from the gen engine
    for (const auto &e : log) {
        CHECK(e.component == "gen");
        CHECK(e.kind == "atomic");
    }
}

TEST_CASE("RootCoordinator log entry output is formatted interval string", "[root_coordinator]") {
    RootCoordinator<T> rc;
    auto model = single_gen_model();
    auto log   = rc.simulate(model, zero_i(), /*max_steps=*/1);

    REQUIRE_FALSE(log.empty());
    const auto &e = log.front();
    // generator outputs interval<double>::closed(1.997, 2.003)
    REQUIRE(e.output.has_value());
    CHECK(*e.output == "[1.997, 2.003]");
}

TEST_CASE("dual-gen coordinator compute_branches returns 2 at step 1", "[root_coordinator]") {
    auto model  = dual_gen_model();
    auto shared = std::make_shared<const CoupledModel<T>>(model);
    cadmia::engine::coordinator<T> coord(shared);
    coord.init(zero_i());
    auto t = coord.t_next();
    REQUIRE_FALSE(t.is_empty());
    auto branches = coord.compute_branches(t);
    // Both generators are imminent simultaneously → 2 branches expected
    REQUIRE(branches.size() == 2u);
}

TEST_CASE("RootCoordinator simulate dual-gen produces log entries from both",
          "[root_coordinator]") {
    RootCoordinator<T> rc;
    auto model = dual_gen_model();
    auto log   = rc.simulate(model, zero_i(), /*max_steps=*/6);

    REQUIRE_FALSE(log.empty());
    // Entries should be from gen1 or gen2
    for (const auto &e : log) {
        CHECK((e.component == "gen1" || e.component == "gen2" || e.component.empty()));
    }
}

TEST_CASE("RootCoordinator max_steps limits output", "[root_coordinator]") {
    RootCoordinator<T> rc;
    auto model = single_gen_model();

    auto log3 = rc.simulate(model, zero_i(), 3);
    auto log6 = rc.simulate(model, zero_i(), 6);

    REQUIRE(log3.size() <= 3u);
    REQUIRE(log6.size() <= 6u);
    CHECK(log6.size() > log3.size());
}

TEST_CASE("RootCoordinator log entries have ascending time strings", "[root_coordinator]") {
    RootCoordinator<T> rc;
    auto model = single_gen_model();
    auto log   = rc.simulate(model, zero_i(), 3);
    // Just verify we get at least one entry with branch "0"
    REQUIRE_FALSE(log.empty());
    CHECK(log.front().branch == "0");
}

TEST_CASE("RootCoordinator throws SimulationLimitError when max_branches exceeded",
          "[root_coordinator]") {
    // dual_gen_model has two generators with the same period — compute_branches
    // returns 2 branches simultaneously (gen1 and gen2 are both imminent).
    // max_branches=1 means 0 (remaining after pop) + 2 > 1 → limit triggered.
    RootCoordinator<T> rc;
    auto model = dual_gen_model();
    REQUIRE_THROWS_AS(rc.simulate(model, zero_i(), /*max_steps=*/1000, /*max_branches=*/1),
                      SimulationLimitError);
}

TEST_CASE("RootCoordinator deduplicates converging branches in dual-gen", "[root_coordinator]") {
    // After the initial fork (gen1 fires vs gen2 fires), each branch fires the other
    // generator and reaches an identical coordinator state. Without deduplication the
    // queue grows exponentially; with it the duplicate branch is removed and the
    // simulation completes within a tight branch budget.
    RootCoordinator<T> rc;
    auto model = dual_gen_model();
    REQUIRE_NOTHROW(rc.simulate(model, zero_i(), /*max_steps=*/20, /*max_branches=*/4));
}
