// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2025, Damian Vicino
 * Carleton University
 * All rights reserved.
 */

#include <cadmia/basic_model/iadevs/generator.hpp>
#include <cadmia/basic_model/iadevs/processor.hpp>
#include <cadmia/modeling/component.hpp>
#include <cadmia/modeling/coupled.hpp>

#include <catch2/catch_test_macros.hpp>

using cadmia::engine::identity_translation;
using cadmia::engine::make_translation;
using cadmia::iadevs::generator;
using cadmia::iadevs::processor;
using cadmia::modeling::ComponentSpec;
using cadmia::modeling::CoupledModel;
using cadmia::modeling::make_atomic_component;

using T        = generator::time_t;
using time_i_t = cadmia::modeling::interval<T>;

namespace {
    // Zero time used in CoupledModel construction
    const T zero{};

    // Minimal state intervals for init
    auto gen_state() {
        return cadmia::modeling::interval<generator::state_t>::closed(T{}, T{});
    }
    auto proc_state() {
        processor::state_t s{};
        return cadmia::modeling::interval<processor::state_t>::closed(s, s);
    }
    auto zero_time_i() {
        return time_i_t::closed(T{}, T{});
    }

    // Simple select: return first name alphabetically
    std::string first_select(const std::unordered_set<std::string> &names) {
        return *std::min_element(names.begin(), names.end());
    }
} // namespace

// ─── ComponentSpec ────────────────────────────────────────────────────────────

TEST_CASE("ComponentSpec atomic is_atomic and factory callable", "[component]") {
    auto spec = make_atomic_component<generator>(gen_state(), zero_time_i());
    REQUIRE(spec.is_atomic());
    REQUIRE_FALSE(spec.is_coupled());

    auto engine = spec.factory()(zero_time_i());
    REQUIRE(engine != nullptr);
}

TEST_CASE("ComponentSpec atomic factory initializes engine t_next", "[component]") {
    auto spec   = make_atomic_component<generator>(gen_state(), zero_time_i());
    auto engine = spec.factory()(zero_time_i());
    // Generator has non-empty t_next after init (period [0.997,1.005])
    REQUIRE_FALSE(engine->t_next().is_empty());
}

TEST_CASE("ComponentSpec coupled is_coupled", "[component]") {
    // Build a trivial CoupledModel with one generator
    CoupledModel<T>::component_map comps;
    comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_time_i()));

    CoupledModel<T>::influencer_map infl;
    infl["gen"] = {};

    CoupledModel<T>::translation_map trans;

    auto model = std::make_shared<CoupledModel<T>>(std::move(comps), std::move(infl),
                                                   std::move(trans), first_select, zero);

    auto spec = ComponentSpec<T>::coupled(model);
    REQUIRE(spec.is_coupled());
    REQUIRE_FALSE(spec.is_atomic());
}

TEST_CASE("ComponentSpec atomic null factory throws", "[component]") {
    REQUIRE_THROWS_AS(ComponentSpec<T>::atomic(nullptr), std::invalid_argument);
}

TEST_CASE("ComponentSpec coupled null model throws", "[component]") {
    REQUIRE_THROWS_AS(ComponentSpec<T>::coupled(nullptr), std::invalid_argument);
}

// ─── CoupledModel validation ──────────────────────────────────────────────────

TEST_CASE("CoupledModel valid single component (no couplings)", "[coupled]") {
    CoupledModel<T>::component_map comps;
    comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_time_i()));

    CoupledModel<T>::influencer_map infl;
    infl["gen"] = {};

    REQUIRE_NOTHROW(CoupledModel<T>(std::move(comps), std::move(infl), {}, first_select, zero));
}

TEST_CASE("CoupledModel valid generator-processor IC", "[coupled]") {
    CoupledModel<T>::component_map comps;
    comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_time_i()));
    comps.emplace("proc", make_atomic_component<processor>(proc_state(), zero_time_i()));

    CoupledModel<T>::influencer_map infl;
    infl["gen"]  = {};
    infl["proc"] = {"gen"};

    CoupledModel<T>::translation_map trans;
    trans[{"gen", "proc"}] = identity_translation();

    REQUIRE_NOTHROW(
        CoupledModel<T>(std::move(comps), std::move(infl), std::move(trans), first_select, zero));
}

TEST_CASE("CoupledModel rejects empty component map", "[coupled]") {
    REQUIRE_THROWS_AS(CoupledModel<T>({}, {}, {}, first_select, zero), std::invalid_argument);
}

TEST_CASE("CoupledModel rejects 'self' as component name", "[coupled]") {
    CoupledModel<T>::component_map comps;
    comps.emplace("self", make_atomic_component<generator>(gen_state(), zero_time_i()));
    CoupledModel<T>::influencer_map infl;
    infl["self"] = {};
    REQUIRE_THROWS_AS(CoupledModel<T>(std::move(comps), std::move(infl), {}, first_select, zero),
                      std::invalid_argument);
}

TEST_CASE("CoupledModel rejects component with missing influencer entry", "[coupled]") {
    CoupledModel<T>::component_map comps;
    comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_time_i()));
    // no influencer entry for "gen"
    REQUIRE_THROWS_AS(CoupledModel<T>(std::move(comps), {}, {}, first_select, zero),
                      std::invalid_argument);
}

TEST_CASE("CoupledModel rejects unknown influencer dest", "[coupled]") {
    CoupledModel<T>::component_map comps;
    comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_time_i()));
    CoupledModel<T>::influencer_map infl;
    infl["gen"]     = {};
    infl["missing"] = {}; // dest doesn't exist
    REQUIRE_THROWS_AS(CoupledModel<T>(std::move(comps), std::move(infl), {}, first_select, zero),
                      std::invalid_argument);
}

TEST_CASE("CoupledModel rejects translation with unknown src", "[coupled]") {
    CoupledModel<T>::component_map comps;
    comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_time_i()));
    CoupledModel<T>::influencer_map infl;
    infl["gen"] = {};
    CoupledModel<T>::translation_map trans;
    trans[{"ghost", "gen"}] = identity_translation();
    REQUIRE_THROWS_AS(
        CoupledModel<T>(std::move(comps), std::move(infl), std::move(trans), first_select, zero),
        std::invalid_argument);
}

TEST_CASE("CoupledModel rejects IC translation missing influencer entry", "[coupled]") {
    CoupledModel<T>::component_map comps;
    comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_time_i()));
    comps.emplace("proc", make_atomic_component<processor>(proc_state(), zero_time_i()));
    CoupledModel<T>::influencer_map infl;
    infl["gen"]  = {};
    infl["proc"] = {}; // gen NOT listed as influencer of proc
    CoupledModel<T>::translation_map trans;
    trans[{"gen", "proc"}] = identity_translation(); // translation exists but no influencer
    REQUIRE_THROWS_AS(
        CoupledModel<T>(std::move(comps), std::move(infl), std::move(trans), first_select, zero),
        std::invalid_argument);
}

TEST_CASE("CoupledModel rejects influencer pair with no translation", "[coupled]") {
    CoupledModel<T>::component_map comps;
    comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_time_i()));
    comps.emplace("proc", make_atomic_component<processor>(proc_state(), zero_time_i()));
    CoupledModel<T>::influencer_map infl;
    infl["gen"]  = {};
    infl["proc"] = {"gen"}; // influencer declared
    // but no translation registered
    REQUIRE_THROWS_AS(CoupledModel<T>(std::move(comps), std::move(infl), {}, first_select, zero),
                      std::invalid_argument);
}

TEST_CASE("CoupledModel accessors return references", "[coupled]") {
    CoupledModel<T>::component_map comps;
    comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_time_i()));
    CoupledModel<T>::influencer_map infl;
    infl["gen"] = {};
    CoupledModel<T> m(std::move(comps), std::move(infl), {}, first_select, zero);

    REQUIRE(m.components().contains("gen"));
    REQUIRE(m.influencers().at("gen").empty());
    REQUIRE(m.zero_time() == zero);
}

TEST_CASE("CoupledModel EOC translation (src->self) is valid", "[coupled]") {
    CoupledModel<T>::component_map comps;
    comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_time_i()));
    CoupledModel<T>::influencer_map infl;
    infl["gen"] = {};
    CoupledModel<T>::translation_map trans;
    trans[{"gen", "self"}] = identity_translation(); // EOC exempt from IC consistency check
    REQUIRE_NOTHROW(
        CoupledModel<T>(std::move(comps), std::move(infl), std::move(trans), first_select, zero));
}
