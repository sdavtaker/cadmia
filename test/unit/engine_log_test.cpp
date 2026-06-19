// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2025, Damian Vicino
 * Carleton University
 * All rights reserved.
 */

#include <cadmia/basic_model/iadevs/generator.hpp>
#include <cadmia/engine/coordinator.hpp>
#include <cadmia/engine/log.hpp>
#include <cadmia/modeling/coupled.hpp>

#include <catch2/catch_test_macros.hpp>
#include <sstream>
#include <string>

using cadmia::engine::coordinator;
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

} // namespace

TEST_CASE("to_ndjson produces valid JSON line for a step entry", "[log]") {
    cadmia::engine::LogEntry<T> e;
    e.step          = 3;
    e.branch        = "0.1.2";
    e.kind          = "atomic";
    e.parent_branch = std::string{"0.1"};
    e.time          = "[0.997, 1.005]";
    e.component     = "gen";
    e.output        = std::string{"<output>"};

    const std::string line = cadmia::log::to_ndjson(e);

    // Must start and end with braces
    REQUIRE(line.front() == '{');
    REQUIRE(line.back() == '}');

    // Required fields present
    CHECK(line.find(R"("level":"info")") != std::string::npos);
    CHECK(line.find(R"("event":"step")") != std::string::npos);
    CHECK(line.find(R"("step":3)") != std::string::npos);
    CHECK(line.find(R"("branch":"0.1.2")") != std::string::npos);
    CHECK(line.find(R"("sim_time":"[0.997, 1.005]")") != std::string::npos);
    CHECK(line.find(R"("kind":"atomic")") != std::string::npos);
    CHECK(line.find(R"("component":"gen")") != std::string::npos);
    CHECK(line.find(R"("parent_branch":"0.1")") != std::string::npos);
    CHECK(line.find(R"("output":"<output>")") != std::string::npos);
    CHECK(line.find(R"("ts":)") != std::string::npos);
}

TEST_CASE("to_ndjson skip entry omits component and output", "[log]") {
    cadmia::engine::LogEntry<T> e;
    e.step      = 0;
    e.branch    = "0";
    e.kind      = "skip";
    e.time      = "[1.0, 1.0]";
    e.component = ""; // skip — no component

    const std::string line = cadmia::log::to_ndjson(e);

    CHECK(line.find(R"("event":"skip")") != std::string::npos);
    CHECK(line.find(R"("component")") == std::string::npos);
    CHECK(line.find(R"("output")") == std::string::npos);
    CHECK(line.find(R"("parent_branch")") == std::string::npos);
}

TEST_CASE("to_ndjson escapes special characters in strings", "[log]") {
    cadmia::engine::LogEntry<T> e;
    e.step      = 0;
    e.branch    = "0";
    e.kind      = "atomic";
    e.time      = "[0, 1]";
    e.component = "gen\"quote";
    e.output    = std::string{"line1\\nline2"};

    const std::string line = cadmia::log::to_ndjson(e);

    CHECK(line.find(R"(gen\"quote)") != std::string::npos);
}

TEST_CASE("write_ndjson writes one line per entry", "[log]") {
    std::vector<cadmia::engine::LogEntry<T>> entries(3);
    for (int i = 0; i < 3; ++i) {
        entries[i].step      = i;
        entries[i].branch    = std::to_string(i);
        entries[i].kind      = "atomic";
        entries[i].time      = "[0, 1]";
        entries[i].component = "gen";
    }

    std::ostringstream oss;
    cadmia::log::write_ndjson(entries, oss);

    const std::string out = oss.str();

    // Three lines, each ending with '\n'
    int newlines = 0;
    for (char c : out)
        if (c == '\n')
            ++newlines;
    CHECK(newlines == 3);

    // Each line is a JSON object
    std::istringstream iss(out);
    std::string line;
    while (std::getline(iss, line)) {
        REQUIRE_FALSE(line.empty());
        CHECK(line.front() == '{');
        CHECK(line.back() == '}');
    }
}

TEST_CASE("to_ndjson dedup entry carries merged_into field", "[log]") {
    cadmia::engine::LogEntry<T> e;
    e.step          = 2;
    e.branch        = "0.1";
    e.kind          = "dedup";
    e.parent_branch = std::string{"0"};
    e.time          = "[0.997, 1.005]";
    e.merged_into   = std::string{"0.0"};

    const std::string line = cadmia::log::to_ndjson(e);

    CHECK(line.find(R"("kind":"dedup")") != std::string::npos);
    CHECK(line.find(R"("merged_into":"0.0")") != std::string::npos);
    CHECK(line.find(R"("component")") == std::string::npos);
    CHECK(line.find(R"("output")") == std::string::npos);
}

TEST_CASE("dedup entries appear in log when dual-gen branches converge", "[log]") {
    CoupledModel<T>::component_map comps;
    comps.emplace("gen1", make_atomic_component<generator>(gen_state(), zero_i()));
    comps.emplace("gen2", make_atomic_component<generator>(gen_state(), zero_i()));
    CoupledModel<T>::influencer_map infl;
    infl["gen1"] = {};
    infl["gen2"] = {};
    auto model   = std::make_shared<const CoupledModel<T>>(
        std::move(comps), std::move(infl), CoupledModel<T>::translation_map{}, first_select, zero);

    cadmia::engine::RootCoordinator<T> rc;
    auto log = rc.simulate(*model, zero_i(), /*max_steps=*/20, /*max_branches=*/4);

    const bool has_dedup =
        std::any_of(log.begin(), log.end(), [](const auto &e) { return e.kind == "dedup"; });
    REQUIRE(has_dedup);

    for (const auto &e : log) {
        if (e.kind == "dedup") {
            CHECK(e.merged_into.has_value());
            CHECK_FALSE(e.merged_into->empty());
        }
    }
}

TEST_CASE("write_ndjson end-to-end with real simulation", "[log]") {
    CoupledModel<T>::component_map comps;
    comps.emplace("gen", make_atomic_component<generator>(gen_state(), zero_i()));
    CoupledModel<T>::influencer_map infl;
    infl["gen"] = {};
    auto model  = std::make_shared<const CoupledModel<T>>(
        std::move(comps), std::move(infl), CoupledModel<T>::translation_map{}, first_select, zero);

    cadmia::engine::RootCoordinator<T> rc;
    auto log = rc.simulate(*model, zero_i(), /*max_steps=*/5, /*max_branches=*/100);

    REQUIRE_FALSE(log.empty());

    std::ostringstream oss;
    cadmia::log::write_ndjson(log, oss);

    // Every line must be a non-empty JSON object
    std::istringstream iss(oss.str());
    std::string line;
    int count = 0;
    while (std::getline(iss, line)) {
        if (line.empty())
            continue;
        CHECK(line.front() == '{');
        CHECK(line.back() == '}');
        CHECK(line.find(R"("ts":)") != std::string::npos);
        CHECK(line.find(R"("sim_time":)") != std::string::npos);
        ++count;
    }
    CHECK(count == static_cast<int>(log.size()));
}
