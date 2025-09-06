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

#include <concepts>
#include <stdexcept>

namespace cadmia::modeling {

    // Types usable in Interval must be totally ordered and default-initializable
    // (default value is treated as the additive identity "zero" where needed).
    template <typename T>
    concept IntervalScalar = std::totally_ordered<T> && std::default_initializable<T>;

    // Generic interval type with closure flags.
    template <IntervalScalar T> struct Interval {
        T lower{};
        T upper{};
        bool lower_closed{false};
        bool upper_closed{false};

        constexpr Interval() noexcept = default;

        // Internal constructor used by factory methods
        constexpr Interval(const T &lo, const T &hi, bool lc, bool uc) noexcept
            : lower(lo), upper(hi), lower_closed(lc), upper_closed(uc) {}

        // Represent an empty interval as (v, v). Defaults to (0, 0).
        static Interval empty_interval() {
            return open(T{}, T{});
        }

        // [lo, hi]
        static Interval closed(const T &lo, const T &hi) {
            if (hi < lo)
                throw std::invalid_argument("Interval.closed: hi < lo");
            return Interval{lo, hi, true, true};
        }

        // [lo, hi)
        static Interval right_open(const T &lo, const T &hi) { // right-open
            if (hi < lo)
                throw std::invalid_argument("Interval.right_open: hi < lo");
            return Interval{lo, hi, true, false};
        }

        // (lo, hi]
        static Interval left_open(const T &lo, const T &hi) {
            if (hi < lo)
                throw std::invalid_argument("Interval.left_open: hi < lo");
            return Interval{lo, hi, false, true};
        }

        // (lo, hi)
        static Interval open(const T &lo, const T &hi) {
            if (hi < lo)
                throw std::invalid_argument("Interval.open: hi < lo");
            return Interval{lo, hi, false, false};
        }

        [[nodiscard]] constexpr bool is_empty() const noexcept {
            return !lower_closed && !upper_closed && !(lower < upper) && !(upper < lower);
        }
    };

} // namespace cadmia::modeling
