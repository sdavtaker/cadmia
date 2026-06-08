// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2025, Damian Vicino
 * Carleton University
 * All rights reserved.
 */

#include <cadmia/basic_model/iadevs/generator.hpp>
#include <cadmia/basic_model/iadevs/processor.hpp>
#include <cadmia/engine/coordinator.hpp>

#include <catch2/catch_test_macros.hpp>

using cadmia::engine::coordinator;
using cadmia::engine::identity_translation;
using cadmia::iadevs::generator;
using cadmia::iadevs::processor;
using cadmia::modeling::CoupledModel;
using cadmia::modeling::make_atomic_component;

using T        = generator::time_t;
using time_i_t = cadmia::modeling::interval<T>;

namespace {
    const T zero{};

    auto gen_state() {
        return cadmia::modeling::interval<generator::state_t>::closed(T{}, T{});
    }
    auto proc_state() {
        processor::state_t s{};
        return cadmia::modeling::interval<processor::state_t>::closed(s, s);
    }
    auto zero_i() {
        return time_i_t::closed(T{}, T{});
    }

    std::string first_select(const std::unordered_set<std::string> &names) {
        return *std::min_element(names.begin(), names.end());
    }

    // Build a standalone generator coordinator (single-component, no couplings)
    std::shared_ptr<const CoupledModel<T>> single_gen_model() {
        CoupledModel<T>::component_map comps;
        comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_i()));
        CoupledModel<T>::influencer_map infl;
        infl["gen"] = {};
        return std::make_shared<const CoupledModel<T>>(std::move(comps), std::move(infl),
                                                       CoupledModel<T>::translation_map{},
                                                       first_select, zero);
    }

    // Build a generator→processor coupled model
    std::shared_ptr<const CoupledModel<T>> gen_proc_model() {
        CoupledModel<T>::component_map comps;
        comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_i()));
        comps.emplace("proc", make_atomic_component<processor>(proc_state(), zero_i()));
        CoupledModel<T>::influencer_map infl;
        infl["gen"]  = {};
        infl["proc"] = {"gen"};
        CoupledModel<T>::translation_map trans;
        trans[{"gen", "proc"}] = identity_translation();
        return std::make_shared<const CoupledModel<T>>(std::move(comps), std::move(infl),
                                                       std::move(trans), first_select, zero);
    }
} // namespace

// ─── init ─────────────────────────────────────────────────────────────────────

TEST_CASE("Coordinator init populates engines", "[coordinator]") {
    auto model = single_gen_model();
    coordinator<T> coord(model);
    coord.init(zero_i());

    REQUIRE(coord.engines().contains("gen"));
    REQUIRE_FALSE(coord.t_next().is_empty()); // generator is active
}

TEST_CASE("Coordinator init gen-proc — both engines created", "[coordinator]") {
    auto model = gen_proc_model();
    coordinator<T> coord(model);
    coord.init(zero_i());

    REQUIRE(coord.engines().size() == 2);
    REQUIRE(coord.engines().contains("gen"));
    REQUIRE(coord.engines().contains("proc"));
}

TEST_CASE("Coordinator t_next equals min of child t_nexts after init", "[coordinator]") {
    auto model = gen_proc_model();
    coordinator<T> coord(model);
    coord.init(zero_i());

    // gen has t_next ≈ [0.997, 1.005]; proc is passive (empty)
    // coordinator t_next should equal gen's t_next
    const auto &gen_tn = coord.engines().at("gen")->t_next();
    REQUIRE_FALSE(gen_tn.is_empty());
    // coordinator t_next.lower == gen t_next.lower
    CHECK(coord.t_next().lower == gen_tn.lower);
}

// ─── compute_branches ─────────────────────────────────────────────────────────

TEST_CASE("Coordinator compute_branches returns empty when t_next is passive", "[coordinator]") {
    // Use a proc-only model (starts passive)
    CoupledModel<T>::component_map comps;
    comps.emplace("proc", make_atomic_component<processor>(proc_state(), zero_i()));
    CoupledModel<T>::influencer_map infl;
    infl["proc"] = {};
    auto model   = std::make_shared<const CoupledModel<T>>(
        std::move(comps), std::move(infl), CoupledModel<T>::translation_map{}, first_select, zero);
    coordinator<T> coord(model);
    coord.init(zero_i());

    REQUIRE(coord.t_next().is_empty()); // passive
    auto branches = coord.compute_branches(time_i_t::closed(T{}, T{}));
    REQUIRE(branches.empty());
}

TEST_CASE("Coordinator compute_branches returns branch for imminent gen", "[coordinator]") {
    auto model = single_gen_model();
    coordinator<T> coord(model);
    coord.init(zero_i());

    // Query at t = coord.t_next (should fire)
    auto t        = coord.t_next();
    auto branches = coord.compute_branches(t);
    REQUIRE_FALSE(branches.empty());
    // At least one non-skip branch
    bool has_action = false;
    for (const auto &b : branches)
        if (!b.engine_name.empty()) {
            has_action = true;
            break;
        }
    REQUIRE(has_action);
}

// ─── execute_branch ────────────────────────────────────────────────────────────

TEST_CASE("Coordinator execute_branch advances gen and updates t_next", "[coordinator]") {
    auto model = single_gen_model();
    coordinator<T> coord(model);
    coord.init(zero_i());

    auto t_before = coord.t_next();
    auto branches = coord.compute_branches(t_before);
    REQUIRE_FALSE(branches.empty());

    // Find the non-skip branch
    for (auto &b : branches) {
        if (!b.engine_name.empty()) {
            coord.execute_branch(b);
            break;
        }
    }

    // t_next should have advanced (gen fires and schedules next)
    REQUIRE_FALSE(coord.t_next().is_empty());
    // New t_next lower bound should be > old lower bound
    CHECK(t_before.lower < coord.t_next().lower);
}

// ─── clone ────────────────────────────────────────────────────────────────────

TEST_CASE("Coordinator clone produces independent copy", "[coordinator]") {
    auto model = single_gen_model();
    coordinator<T> coord(model);
    coord.init(zero_i());

    auto cloned_engine = coord.clone();
    auto *cloned       = dynamic_cast<coordinator<T> *>(cloned_engine.get());
    REQUIRE(cloned != nullptr);

    auto t_original = coord.t_next();

    // Advance original
    auto branches = coord.compute_branches(coord.t_next());
    for (auto &b : branches)
        if (!b.engine_name.empty()) {
            coord.execute_branch(b);
            break;
        }

    // Clone should be unaffected
    CHECK(cloned->t_next().lower == t_original.lower);
}

// ─── advance_t_next_past_limit ────────────────────────────────────────────────

TEST_CASE("Coordinator advance_t_next_past_limit opens lower bound at punctual limit",
          "[coordinator]") {
    auto model = single_gen_model();
    coordinator<T> coord(model);
    coord.init(zero_i());

    auto tn = coord.t_next();
    REQUIRE(tn.lower_closed);

    // Build a punctual limit at tn.lower
    auto punct_limit = time_i_t::closed(tn.lower, tn.lower);
    coord.advance_t_next_past_limit(punct_limit);

    // Lower bound should now be open
    CHECK_FALSE(coord.t_next().lower_closed);
}

// ─── punctual limit: exactly one branch ───────────────────────────────────────

TEST_CASE(
    "compute_branches returns exactly one branch for punctual limit — SELECT fires, others wait",
    "[coordinator]") {
    // Two generators, both with t_next = [997, 1005] (non-punctual).
    // Query at a punctual limit [1000, 1000] — both intersect it.
    // SELECT picks gen_a (lexicographically first).
    // Correct behaviour: exactly ONE branch (gen_a fires). gen_b is NOT branched or deferred;
    // it remains scheduled and will fire when compute_branches is called again.
    const T v1000 = 1.0;

    auto gen_a_s0 = time_i_t::closed(zero, zero);
    auto gen_b_s0 = time_i_t::closed(zero, zero);
    auto ti       = time_i_t::closed(zero, zero);

    CoupledModel<T>::component_map comps;
    comps.emplace("gen_a", make_atomic_component<generator>(gen_a_s0, ti));
    comps.emplace("gen_b", make_atomic_component<generator>(gen_b_s0, ti));
    CoupledModel<T>::influencer_map infl;
    infl["gen_a"] = {};
    infl["gen_b"] = {};
    auto model    = std::make_shared<const CoupledModel<T>>(
        std::move(comps), std::move(infl), CoupledModel<T>::translation_map{}, first_select, zero);

    coordinator<T> coord(model);
    coord.init(ti);

    auto punctual_t = time_i_t::closed(v1000, v1000);
    auto branches   = coord.compute_branches(punctual_t);

    REQUIRE(branches.size() == 1);
    REQUIRE(branches[0].engine_name == "gen_a");
    REQUIRE(branches[0].defer_engine.empty());

    // After firing gen_a, gen_b must still be active (not lost).
    coord.execute_branch(branches[0]);
    REQUIRE_FALSE(coord.engines().at("gen_b")->t_next().is_empty());
}
