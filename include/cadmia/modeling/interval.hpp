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

#include <cfenv>
#include <concepts>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace cadmia::modeling {

    // Types usable in interval must be totally ordered and default-initializable.
    template <typename T>
    concept interval_scalar = std::totally_ordered<T> && std::default_initializable<T>;

    // Generic interval type with closure flags.
    //
    // Infinity is carried in-band: store std::numeric_limits<T>::infinity() (or its
    // negation) directly in lower/upper. For floating-point T this is the IEEE 754
    // sentinel. For integer T, std::numeric_limits<T>::has_infinity == false so
    // is_lower_infinite() / is_upper_infinite() always return false.
    //
    // Arithmetic operators use directed FP rounding for floating-point T, preserving
    // the outward-rounding invariant required by IA-DEVS. Inf ± Inf propagation
    // (e.g. +∞ − +∞ = NaN) is the model's responsibility, not the interval's.
    template <interval_scalar T> struct interval {
        using value_t = T;
        T lower{};
        T upper{};
        bool lower_closed{false};
        bool upper_closed{false};

        constexpr interval() noexcept = default;

        constexpr interval(const T &lo, const T &hi, bool lc, bool uc) noexcept
            : lower(lo), upper(hi), lower_closed(lc), upper_closed(uc) {}

      public:
        // Convenience wrappers so callers don't have to write `a < b` / `!(a<b)&&!(b<a)`.
        static constexpr bool endpoint_less(const T &a, const T &b) noexcept {
            return a < b;
        }
        static constexpr bool endpoint_equal(const T &a, const T &b) noexcept {
            return !(a < b) && !(b < a);
        }

        static interval empty_interval() {
            return open(T{}, T{});
        }

        [[nodiscard]] constexpr bool is_subset_of(const interval &other) const noexcept {
            if (is_empty())
                return true;
            if (other.is_empty())
                return false;

            bool lower_ok;
            if (other.lower < lower) {
                lower_ok = true;
            } else if (endpoint_equal(other.lower, lower)) {
                lower_ok = !(lower_closed && !other.lower_closed);
            } else {
                lower_ok = false;
            }
            if (!lower_ok)
                return false;

            bool upper_ok;
            if (upper < other.upper) {
                upper_ok = true;
            } else if (endpoint_equal(upper, other.upper)) {
                upper_ok = !(upper_closed && !other.upper_closed);
            } else {
                upper_ok = false;
            }
            return upper_ok;
        }

        [[nodiscard]] constexpr bool intersects(const interval &other) const noexcept {
            if (is_empty() || other.is_empty())
                return false;

            const bool this_before =
                (upper < other.lower) ||
                (endpoint_equal(upper, other.lower) && !(upper_closed && other.lower_closed));

            const bool other_before =
                (other.upper < lower) ||
                (endpoint_equal(other.upper, lower) && !(other.upper_closed && lower_closed));

            return !(this_before || other_before);
        }

        // [lo, hi]
        static interval closed(const T &lo, const T &hi) {
            if (hi < lo)
                throw std::invalid_argument("interval.closed: hi < lo");
            return interval{lo, hi, true, true};
        }

        // [lo, hi)
        static interval right_open(const T &lo, const T &hi) {
            if (hi < lo)
                throw std::invalid_argument("interval.right_open: hi < lo");
            return interval{lo, hi, true, false};
        }

        // (lo, hi]
        static interval left_open(const T &lo, const T &hi) {
            if (hi < lo)
                throw std::invalid_argument("interval.left_open: hi < lo");
            return interval{lo, hi, false, true};
        }

        // (lo, hi)
        static interval open(const T &lo, const T &hi) {
            if (hi < lo)
                throw std::invalid_argument("interval.open: hi < lo");
            return interval{lo, hi, false, false};
        }

        [[nodiscard]] constexpr bool is_empty() const noexcept {
            if (lower_closed && upper_closed)
                return false;
            if (is_lower_infinite() || is_upper_infinite())
                return false;
            return !(lower < upper);
        }

        [[nodiscard]] constexpr bool is_lower_infinite() const noexcept {
            if constexpr (std::numeric_limits<T>::has_infinity)
                return lower == -std::numeric_limits<T>::infinity();
            return false;
        }

        [[nodiscard]] constexpr bool is_upper_infinite() const noexcept {
            if constexpr (std::numeric_limits<T>::has_infinity)
                return upper == std::numeric_limits<T>::infinity();
            return false;
        }

        // True if this interval is a single finite point [v, v] (both closed).
        [[nodiscard]] constexpr bool is_punctual() const noexcept {
            return lower_closed && upper_closed && !is_lower_infinite() && !is_upper_infinite() &&
                   lower == upper;
        }

        [[nodiscard]] constexpr interval intersection(const interval &other) const noexcept {
            if (!intersects(other))
                return interval::empty_interval();

            T lo;
            bool lo_closed;
            if (lower < other.lower) {
                lo        = other.lower;
                lo_closed = other.lower_closed;
            } else if (other.lower < lower) {
                lo        = lower;
                lo_closed = lower_closed;
            } else {
                lo        = lower;
                lo_closed = lower_closed && other.lower_closed;
            }

            T hi;
            bool hi_closed;
            if (upper < other.upper) {
                hi        = upper;
                hi_closed = upper_closed;
            } else if (other.upper < upper) {
                hi        = other.upper;
                hi_closed = other.upper_closed;
            } else {
                hi        = upper;
                hi_closed = upper_closed && other.upper_closed;
            }

            interval r{};
            r.lower        = lo;
            r.lower_closed = lo_closed;
            r.upper        = hi;
            r.upper_closed = hi_closed;
            return r;
        }

        // Minkowski addition. For floating-point T, directed rounding maintains the
        // outward-rounding invariant. Infinity propagates naturally through T arithmetic.
        friend interval operator+(const interval &a, const interval &b) {
            if (a.is_empty() || b.is_empty())
                return interval::empty_interval();
            interval r{};
            if constexpr (std::floating_point<T>) {
                std::fesetround(FE_DOWNWARD);
                r.lower = a.lower + b.lower;
                std::fesetround(FE_UPWARD);
                r.upper = a.upper + b.upper;
                std::fesetround(FE_TONEAREST);
            } else {
                r.lower = a.lower + b.lower;
                r.upper = a.upper + b.upper;
            }
            if (r.upper < r.lower)
                return interval::empty_interval();
            r.lower_closed = a.lower_closed && b.lower_closed;
            r.upper_closed = a.upper_closed && b.upper_closed;
            return r;
        }

        // Interval subtraction: {l - e | l ∈ L, e ∈ E}.
        // For floating-point T, directed rounding maintains the outward-rounding invariant.
        friend interval operator-(const interval &l, const interval &e) {
            if (l.is_empty() || e.is_empty())
                return interval::empty_interval();
            interval r{};
            if constexpr (std::floating_point<T>) {
                std::fesetround(FE_DOWNWARD);
                r.lower = l.lower - e.upper;
                std::fesetround(FE_UPWARD);
                r.upper = l.upper - e.lower;
                std::fesetround(FE_TONEAREST);
            } else {
                r.lower = l.lower - e.upper;
                r.upper = l.upper - e.lower;
            }
            if (r.upper < r.lower)
                return interval::empty_interval();
            r.lower_closed = l.lower_closed && e.upper_closed;
            r.upper_closed = l.upper_closed && e.lower_closed;
            return r;
        }
    };

} // namespace cadmia::modeling
