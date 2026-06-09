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

#include <cadmia/engine/root_coordinator.hpp>

#include <chrono>
#include <format>
#include <ostream>
#include <string>
#include <string_view>
#include <vector>

namespace cadmia::log {

    namespace detail {

        inline std::string escape_json(std::string_view s) {
            std::string out;
            out.reserve(s.size());
            for (char c : s) {
                switch (c) {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    out += c;
                    break;
                }
            }
            return out;
        }

        inline std::string now_iso8601() {
            auto now = std::chrono::system_clock::now();
            return std::format("{0:%Y-%m-%dT%H:%M:%S}Z",
                               std::chrono::floor<std::chrono::milliseconds>(now));
        }

    } // namespace detail

    /**
     * Serialise one LogEntry<T> to a single NDJSON line (no trailing newline).
     *
     * Fields emitted:
     *   ts           — ISO-8601 wall-clock timestamp
     *   level        — always "info" for simulation steps
     *   event        — "step" or "skip"
     *   step         — BFS step counter
     *   branch       — branch identifier (e.g. "0.1.2")
     *   parent_branch — parent branch id, omitted for root
     *   sim_time     — time interval string (e.g. "[0.997, 1.005]")
     *   kind         — "atomic", "coupled", or "skip"
     *   component    — engine name; omitted for skip branches
     *   output       — output string; omitted when no output
     *
     * The format is compatible with cdboost's log::emit() field names so that
     * tooling (cadmia-explorer, jq pipelines) can process logs from all three
     * simulators with the same schema.
     */
    template <typename T>
    [[nodiscard]] std::string to_ndjson(const cadmia::engine::LogEntry<T> &e) {
        const bool is_skip = e.component.empty();
        // Build incrementally: open brace, mandatory fields, optional fields, close brace.
        std::string line = std::format(
            R"({{"ts":"{}","level":"info","event":"{}","step":{},"branch":"{}","sim_time":"{}","kind":"{}")",
            detail::now_iso8601(), is_skip ? "skip" : "step", e.step, detail::escape_json(e.branch),
            detail::escape_json(e.time), detail::escape_json(e.kind));

        if (e.parent_branch.has_value())
            line += std::format(R"(,"parent_branch":"{}")", detail::escape_json(*e.parent_branch));

        if (!is_skip)
            line += std::format(R"(,"component":"{}")", detail::escape_json(e.component));

        if (e.output.has_value())
            line += std::format(R"(,"output":"{}")", detail::escape_json(*e.output));

        line += '}';
        return line;
    }

    /**
     * Write every entry in `entries` as NDJSON to `out`, one line per entry.
     * Callers can pass std::cout, an ofstream, or any std::ostream.
     */
    template <typename T>
    void write_ndjson(const std::vector<cadmia::engine::LogEntry<T>> &entries, std::ostream &out) {
        for (const auto &e : entries)
            out << to_ndjson(e) << '\n';
    }

} // namespace cadmia::log
