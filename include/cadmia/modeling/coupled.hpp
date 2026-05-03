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
#include <cadmia/modeling/component.hpp>

#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace cadmia::modeling {

    namespace detail {
        struct PairHash {
            std::size_t operator()(const std::pair<std::string, std::string> &p) const noexcept {
                std::size_t h1 = std::hash<std::string>{}(p.first);
                std::size_t h2 = std::hash<std::string>{}(p.second);
                return h1 ^ (h2 * 2654435761ULL);
            }
        };
    } // namespace detail

    /**
     * CoupledModel<T> — immutable data structure for IA-DEVS coupling topology.
     *
     * Formal tuple C = <X, Y, D, M, I, Z, SELECT> from VWD21.
     * Pure data: no simulation logic. Coordinator consumes this at init time.
     *
     * "self" is reserved as a boundary alias:
     *   source="self" → EIC (external input coupling into a child)
     *   dest="self"   → EOC (external output coupling out of a child)
     *
     * Validated at construction. Shared via shared_ptr across coordinator clones
     * for BFS branching (immutable so sharing is safe).
     */
    template <typename T> class CoupledModel {
      public:
        using time_t         = T;
        using component_map  = std::unordered_map<std::string, ComponentSpec<T>>;
        using influencer_map = std::unordered_map<std::string, std::unordered_set<std::string>>;
        using translation_map =
            std::unordered_map<std::pair<std::string, std::string>, cadmia::engine::translation_fn,
                               detail::PairHash>;
        using select_fn = std::function<std::string(const std::unordered_set<std::string> &)>;

        CoupledModel(component_map components, influencer_map influencers,
                     translation_map translations, select_fn select, T zero_time)
            : components_(std::move(components)), influencers_(std::move(influencers)),
              translations_(std::move(translations)), select_(std::move(select)),
              zero_time_(std::move(zero_time)) {
            validate();
        }

        [[nodiscard]] const component_map &components() const noexcept {
            return components_;
        }
        [[nodiscard]] const influencer_map &influencers() const noexcept {
            return influencers_;
        }
        [[nodiscard]] const translation_map &translations() const noexcept {
            return translations_;
        }
        [[nodiscard]] const select_fn &select() const noexcept {
            return select_;
        }
        [[nodiscard]] const T &zero_time() const noexcept {
            return zero_time_;
        }

        // Lookup helpers used by coordinator
        [[nodiscard]] bool has_translation(const std::string &src, const std::string &dst) const {
            return translations_.contains({src, dst});
        }

        [[nodiscard]] const cadmia::engine::translation_fn &
        translation(const std::string &src, const std::string &dst) const {
            return translations_.at({src, dst});
        }

      private:
        component_map components_;
        influencer_map influencers_;
        translation_map translations_;
        select_fn select_;
        T zero_time_;

        void validate() const {
            const auto &comps = components_;
            std::unordered_set<std::string> comp_names;
            for (const auto &[name, _] : comps)
                comp_names.insert(name);

            // Rule 1: at least one component
            if (comp_names.empty())
                throw std::invalid_argument("CoupledModel: must have at least one component");

            // Rule 7: "self" is reserved
            if (comp_names.contains("self"))
                throw std::invalid_argument(
                    "CoupledModel: 'self' is reserved and cannot be a component name");

            std::unordered_set<std::string> valid = comp_names;
            valid.insert("self");

            // Rule 2: all influencer keys and source refs must exist
            for (const auto &[dest, sources] : influencers_) {
                if (!valid.contains(dest))
                    throw std::invalid_argument("CoupledModel: influencer dest '" + dest +
                                                "' does not exist");
                for (const auto &src : sources)
                    if (!valid.contains(src))
                        throw std::invalid_argument("CoupledModel: influencer src '" + src +
                                                    "' of dest '" + dest + "' does not exist");
            }

            // Rule 3: every component must have an influencer entry (may be empty)
            for (const auto &name : comp_names)
                if (!influencers_.contains(name))
                    throw std::invalid_argument(
                        "CoupledModel: component '" + name +
                        "' has no influencer entry (empty set is valid but must be explicit)");

            // Rule 4: translation endpoints must exist
            for (const auto &[edge, _] : translations_) {
                if (!valid.contains(edge.first))
                    throw std::invalid_argument("CoupledModel: translation src '" + edge.first +
                                                "' does not exist");
                if (!valid.contains(edge.second))
                    throw std::invalid_argument("CoupledModel: translation dest '" + edge.second +
                                                "' does not exist");
            }

            // Rule 5: IC translations must match influencer declarations
            for (const auto &[edge, _] : translations_) {
                const auto &[src, dest] = edge;
                if (src == "self" || dest == "self")
                    continue; // EIC/EOC exempt
                const auto it = influencers_.find(dest);
                if (it == influencers_.end() || !it->second.contains(src))
                    throw std::invalid_argument("CoupledModel: translation '" + src + "'->'" +
                                                dest + "' has no matching influencer entry");
            }

            // Rule 6: every influencer pair must have a translation
            for (const auto &[dest, sources] : influencers_) {
                for (const auto &src : sources) {
                    if (!translations_.contains({src, dest}))
                        throw std::invalid_argument("CoupledModel: influencer '" + src + "'->'" +
                                                    dest +
                                                    "' has no translation function registered");
                }
            }
        }
    };

} // namespace cadmia::modeling
