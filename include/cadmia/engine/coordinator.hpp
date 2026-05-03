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

#include <cadmia/engine/engine.hpp>
#include <cadmia/modeling/coupled.hpp>

#include <algorithm>
#include <any>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace cadmia::engine {

    /**
     * One possible action for the current simulation step.
     * engine_name == "" means a skip branch (no engine fires in this interval).
     */
    template <typename T> struct BranchAction {
        std::string engine_name;
        cadmia::modeling::interval<T> limit;
    };

    /**
     * IA-DEVS Coordinator (Algorithms 2-3 from VWD21).
     *
     * Wraps a CoupledModel and manages a heterogeneous map of child engines.
     * Inherits IADEVSEngine<T> so it can itself be a child of a parent Coordinator.
     *
     * The immutable CoupledModel is shared via shared_ptr across coordinator clones,
     * so BFS deep-copying is cheap.
     */
    template <typename T> class coordinator : public IADEVSEngine<T> {
      public:
        using time_i_t   = cadmia::modeling::interval<T>;
        using model_t    = cadmia::modeling::CoupledModel<T>;
        using engine_map = std::unordered_map<std::string, std::unique_ptr<IADEVSEngine<T>>>;

        explicit coordinator(std::shared_ptr<const model_t> model) : model_(std::move(model)) {}

        // ── Init (Algorithm 2 init) ────────────────────────────────────────────

        void init(const time_i_t &t) {
            engines_.clear();
            influenced_by_.clear();

            for (const auto &[name, spec] : model_->components()) {
                if (spec.is_atomic()) {
                    engines_[name] = spec.factory()(t);
                } else {
                    auto sub = std::make_unique<coordinator<T>>(
                        std::make_shared<const model_t>(spec.coupled_model()));
                    sub->init(t);
                    engines_[name] = std::move(sub);
                }
            }

            // Build influenced_by: src → {dests} for IC only
            for (const auto &[dest, sources] : model_->influencers()) {
                if (dest == "self")
                    continue;
                for (const auto &src : sources) {
                    if (src == "self")
                        continue;
                    influenced_by_[src].insert(dest);
                }
            }

            bound_ts();
        }

        // ── IADEVSEngine interface ─────────────────────────────────────────────

        [[nodiscard]] time_i_t t_next() const noexcept override {
            return t_next_;
        }
        [[nodiscard]] time_i_t t_last() const noexcept override {
            return t_last_;
        }

        [[nodiscard]] std::optional<std::any> engine_star(const time_i_t &t) override {
            auto actions = compute_branches(t);
            if (actions.empty())
                return std::nullopt;
            for (auto &action : actions) {
                if (!action.engine_name.empty()) {
                    auto [_, eoc_y] = execute_branch(action);
                    return eoc_y;
                }
            }
            auto [_, eoc_y] = execute_branch(actions[0]);
            return eoc_y;
        }

        void engine_x(std::any x_val, const time_i_t &t) override {
            x_function(std::move(x_val), t);
        }

        void advance_t_next_past_limit(const time_i_t &limit) override {
            for (auto &[_, eng] : engines_)
                eng->advance_t_next_past_limit(limit);
            bound_ts();
        }

        [[nodiscard]] std::unique_ptr<IADEVSEngine<T>> clone() const override {
            auto c = std::make_unique<coordinator<T>>(model_); // shares immutable model
            for (const auto &[name, eng] : engines_)
                c->engines_[name] = eng->clone();
            c->influenced_by_ = influenced_by_;
            c->t_last_        = t_last_;
            c->t_next_        = t_next_;
            return c;
        }

        // ── Coordinator-specific public interface ──────────────────────────────

        /**
         * Compute possible branches without side effects (Algorithm 3 logic).
         * Returns BranchAction list. RootCoordinator clones state and calls
         * execute_branch on each.
         */
        [[nodiscard]] std::vector<BranchAction<T>> compute_branches(const time_i_t &t) const {
            if (t_next_.is_empty())
                return {};

            // 1. Find imminent engines (t_next intersects t)
            std::vector<std::string> imminents;
            for (const auto &[name, eng] : engines_) {
                if (!eng->t_next().is_empty() && eng->t_next().intersects(t))
                    imminents.push_back(name);
            }
            if (imminents.empty())
                return {};

            // 2. Sort by (lower, upper) — earliest fires first
            std::sort(imminents.begin(), imminents.end(),
                      [this](const std::string &a, const std::string &b) {
                          const auto &ta = engines_.at(a)->t_next();
                          const auto &tb = engines_.at(b)->t_next();
                          if (time_i_t::endpoint_less(ta.lower, ta.lower_inf_sign, tb.lower,
                                                      tb.lower_inf_sign))
                              return true;
                          if (time_i_t::endpoint_equal(ta.lower, ta.lower_inf_sign, tb.lower,
                                                       tb.lower_inf_sign))
                              return time_i_t::endpoint_less(ta.upper, ta.upper_inf_sign, tb.upper,
                                                             tb.upper_inf_sign);
                          return false;
                      });

            // 3. Compute limit: intersection of first imminent's t_next with t
            const auto &first_tn = engines_.at(imminents[0])->t_next();
            time_i_t limit       = first_tn.intersection(t);

            // Restrict upper if later imminent starts strictly after first's lower
            if (imminents.size() > 1) {
                for (size_t i = 1; i < imminents.size(); ++i) {
                    const auto &eng_tn = engines_.at(imminents[i])->t_next();
                    if (time_i_t::endpoint_less(first_tn.lower, first_tn.lower_inf_sign,
                                                eng_tn.lower, eng_tn.lower_inf_sign)) {
                        limit = restrict_upper(limit, eng_tn.lower, eng_tn.lower_inf_sign,
                                               eng_tn.lower_closed);
                        break;
                    }
                    if (time_i_t::endpoint_equal(first_tn.lower, first_tn.lower_inf_sign,
                                                 eng_tn.lower, eng_tn.lower_inf_sign) &&
                        !eng_tn.lower_closed && first_tn.lower_closed) {
                        limit = restrict_upper(limit, eng_tn.lower, eng_tn.lower_inf_sign,
                                               eng_tn.lower_closed);
                        break;
                    }
                }
            }

            // 4. Punctual limit: SELECT among candidates
            if (limit.is_punctual()) {
                std::unordered_set<std::string> candidates;
                for (const auto &name : imminents) {
                    if (engines_.at(name)->t_next().intersects(limit))
                        candidates.insert(name);
                }
                const std::string chosen = model_->select()(candidates);

                std::vector<BranchAction<T>> branches;
                const auto &chosen_tn = engines_.at(chosen)->t_next();
                if (!endpoint_eq(limit, chosen_tn))
                    branches.push_back({.engine_name = "", .limit = limit}); // fork skip
                branches.push_back({.engine_name = chosen, .limit = limit});
                return branches;
            }

            // Non-punctual: one branch per imminent that overlaps the limit
            std::vector<BranchAction<T>> branches;
            bool any_skip = false;
            for (const auto &name : imminents) {
                const auto &eng_tn = engines_.at(name)->t_next();
                if (eng_tn.intersects(limit)) {
                    branches.push_back({.engine_name = name, .limit = limit});
                    if (!endpoint_eq(limit, eng_tn))
                        any_skip = true;
                }
            }
            if (any_skip)
                branches.push_back({.engine_name = "", .limit = limit});
            return branches;
        }

        /**
         * Execute one branch action: route engine output, update BoundTs.
         *
         * Returns {component_output, eoc_output}.
         * component_output: raw output of the firing engine (for logging).
         * eoc_output: EOC-translated output to parent, or nullopt.
         */
        std::pair<std::optional<std::any>, std::optional<std::any>>
        execute_branch(const BranchAction<T> &action) {
            if (action.engine_name.empty()) {
                subtract_limit(action.limit);
                bound_ts();
                return {std::nullopt, std::nullopt};
            }
            auto result = route(action.engine_name, action.limit);
            bound_ts();
            return result;
        }

        /**
         * Route external input to EIC targets (Algorithm 2 x-function).
         */
        void x_function(std::any x_val, const time_i_t &t) {
            for (const auto &[dest, sources] : model_->influencers()) {
                if (sources.contains("self") && dest != "self") {
                    const auto &z = model_->translation("self", dest);
                    engines_.at(dest)->engine_x(z(x_val), t);
                }
            }
            bound_ts();
        }

        [[nodiscard]] const engine_map &engines() const noexcept {
            return engines_;
        }

      private:
        std::shared_ptr<const model_t> model_;
        engine_map engines_;
        std::unordered_map<std::string, std::unordered_set<std::string>> influenced_by_;
        time_i_t t_last_{};
        time_i_t t_next_{cadmia::modeling::interval<T>::empty_interval()};

        // ── BoundTs (Algorithm 2) ──────────────────────────────────────────────

        void bound_ts() {
            if (engines_.empty()) {
                t_last_ = time_i_t{};
                t_next_ = time_i_t::empty_interval();
                return;
            }

            std::vector<IADEVSEngine<T> *> all, active;
            for (const auto &[_, eng] : engines_) {
                all.push_back(eng.get());
                if (!eng->t_next().is_empty())
                    active.push_back(eng.get());
            }

            t_last_ = compute_t_last(all);
            t_next_ = active.empty() ? time_i_t::empty_interval() : compute_t_next(active);
        }

        static time_i_t compute_t_last(const std::vector<IADEVSEngine<T> *> &engines) {
            auto tl        = engines[0]->t_last();
            T max_lo       = tl.lower;
            int max_lo_inf = tl.lower_inf_sign;
            bool max_lo_cl = tl.lower_closed;
            T max_hi       = tl.upper;
            int max_hi_inf = tl.upper_inf_sign;
            bool max_hi_cl = tl.upper_closed;

            for (size_t i = 1; i < engines.size(); ++i) {
                auto ti = engines[i]->t_last();

                // Max lower
                if (time_i_t::endpoint_less(max_lo, max_lo_inf, ti.lower, ti.lower_inf_sign)) {
                    max_lo     = ti.lower;
                    max_lo_inf = ti.lower_inf_sign;
                    max_lo_cl  = ti.lower_closed;
                } else if (time_i_t::endpoint_equal(max_lo, max_lo_inf, ti.lower,
                                                    ti.lower_inf_sign)) {
                    if (!ti.lower_closed)
                        max_lo_cl = false; // open wins (more restrictive for max)
                }

                // Max upper
                if (time_i_t::endpoint_less(max_hi, max_hi_inf, ti.upper, ti.upper_inf_sign)) {
                    max_hi     = ti.upper;
                    max_hi_inf = ti.upper_inf_sign;
                    max_hi_cl  = ti.upper_closed;
                } else if (time_i_t::endpoint_equal(max_hi, max_hi_inf, ti.upper,
                                                    ti.upper_inf_sign)) {
                    if (ti.upper_closed)
                        max_hi_cl = true; // closed wins (wider max)
                }
            }

            time_i_t r{};
            r.lower          = max_lo;
            r.lower_inf_sign = max_lo_inf;
            r.lower_closed   = max_lo_cl;
            r.upper          = max_hi;
            r.upper_inf_sign = max_hi_inf;
            r.upper_closed   = max_hi_cl;
            return r;
        }

        static time_i_t compute_t_next(const std::vector<IADEVSEngine<T> *> &active) {
            auto tn0       = active[0]->t_next();
            T min_lo       = tn0.lower;
            int min_lo_inf = tn0.lower_inf_sign;
            bool min_lo_cl = tn0.lower_closed;
            T min_hi       = tn0.upper;
            int min_hi_inf = tn0.upper_inf_sign;
            bool min_hi_cl = tn0.upper_closed;

            for (size_t i = 1; i < active.size(); ++i) {
                auto tni = active[i]->t_next();

                // Min lower
                if (time_i_t::endpoint_less(tni.lower, tni.lower_inf_sign, min_lo, min_lo_inf)) {
                    min_lo     = tni.lower;
                    min_lo_inf = tni.lower_inf_sign;
                    min_lo_cl  = tni.lower_closed;
                } else if (time_i_t::endpoint_equal(tni.lower, tni.lower_inf_sign, min_lo,
                                                    min_lo_inf)) {
                    if (tni.lower_closed)
                        min_lo_cl = true; // closed wins (wider min)
                }

                // Min upper
                if (time_i_t::endpoint_less(tni.upper, tni.upper_inf_sign, min_hi, min_hi_inf)) {
                    min_hi     = tni.upper;
                    min_hi_inf = tni.upper_inf_sign;
                    min_hi_cl  = tni.upper_closed;
                } else if (time_i_t::endpoint_equal(tni.upper, tni.upper_inf_sign, min_hi,
                                                    min_hi_inf)) {
                    if (!tni.upper_closed)
                        min_hi_cl = false; // open wins (narrower min)
                }
            }

            time_i_t r{};
            r.lower          = min_lo;
            r.lower_inf_sign = min_lo_inf;
            r.lower_closed   = min_lo_cl;
            r.upper          = min_hi;
            r.upper_inf_sign = min_hi_inf;
            r.upper_closed   = min_hi_cl;
            return r;
        }

        // ── subtract_limit (Algorithm 3 skip branch) ──────────────────────────

        void subtract_limit(const time_i_t &limit) {
            for (auto &[_, eng] : engines_)
                eng->advance_t_next_past_limit(limit);
        }

        // ── route (Algorithm 3 route) ──────────────────────────────────────────

        std::pair<std::optional<std::any>, std::optional<std::any>>
        route(const std::string &engine_name, const time_i_t &t) {
            auto &eng    = engines_.at(engine_name);
            auto maybe_y = eng->engine_star(t);

            if (!maybe_y)
                return {std::nullopt, std::nullopt};
            const auto &y = *maybe_y;

            // IC: route to influenced children
            const auto &dests_it = influenced_by_.find(engine_name);
            if (dests_it != influenced_by_.end()) {
                for (const auto &dest : dests_it->second) {
                    const auto &z = model_->translation(engine_name, dest);
                    engines_.at(dest)->engine_x(z(y), t);
                }
            }

            // EOC: propagate to parent if engine has an outgoing "self" coupling
            const auto &eoc_sources = model_->influencers().find("self");
            if (eoc_sources != model_->influencers().end() &&
                eoc_sources->second.contains(engine_name)) {
                const auto &z_eoc = model_->translation(engine_name, "self");
                return {y, z_eoc(y)};
            }

            return {y, std::nullopt};
        }

        // ── Helpers ────────────────────────────────────────────────────────────

        // Restrict interval upper bound to not exceed (value, value_inf, value_closed).
        static time_i_t restrict_upper(const time_i_t &iv, const T &value, int value_inf,
                                       bool value_closed) {
            const bool iv_upper_exceeds =
                iv.upper_inf_sign == +1 ||
                time_i_t::endpoint_less(value, value_inf, iv.upper, iv.upper_inf_sign);
            const bool iv_upper_eq =
                time_i_t::endpoint_equal(iv.upper, iv.upper_inf_sign, value, value_inf);

            if (iv_upper_exceeds || (iv_upper_eq && iv.upper_closed && value_closed)) {
                time_i_t r{};
                r.lower          = iv.lower;
                r.lower_inf_sign = iv.lower_inf_sign;
                r.lower_closed   = iv.lower_closed;
                r.upper          = value;
                r.upper_inf_sign = value_inf;
                r.upper_closed   = !value_closed;
                return r;
            }
            return iv;
        }

        // True if two intervals represent exactly the same set of points.
        static bool endpoint_eq(const time_i_t &a, const time_i_t &b) noexcept {
            return time_i_t::endpoint_equal(a.lower, a.lower_inf_sign, b.lower, b.lower_inf_sign) &&
                   time_i_t::endpoint_equal(a.upper, a.upper_inf_sign, b.upper, b.upper_inf_sign) &&
                   a.lower_closed == b.lower_closed && a.upper_closed == b.upper_closed;
        }
    };

} // namespace cadmia::engine
