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

#include <cadmia/modeling/decimal.hpp>

#include <concepts>
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace cadmia::modeling {
    // Overflow handling policy: default (no overflow to infinity), specialized for int
    template <typename T> struct interval_overflow_policy {
        static constexpr bool add_overflow(const T &, const T &) noexcept {
            return false;
        }
        static constexpr bool add_underflow(const T &, const T &) noexcept {
            return false;
        }
        static constexpr bool sub_overflow(const T &, const T &) noexcept {
            return false;
        }
        static constexpr bool sub_underflow(const T &, const T &) noexcept {
            return false;
        }
    };

    template <> struct interval_overflow_policy<int> {
        static constexpr bool add_overflow(int a, int b) noexcept {
            return (b > 0) && (a > std::numeric_limits<int>::max() - b);
        }
        static constexpr bool add_underflow(int a, int b) noexcept {
            return (b < 0) && (a < std::numeric_limits<int>::min() - b);
        }
        static constexpr bool sub_overflow(int a, int b) noexcept {
            // a - b > INT_MAX  <=>  a > INT_MAX + b
            return (b < 0) && (a > std::numeric_limits<int>::max() + b);
        }
        static constexpr bool sub_underflow(int a, int b) noexcept {
            // a - b < INT_MIN  <=>  a < INT_MIN + b
            return (b > 0) && (a < std::numeric_limits<int>::min() + b);
        }
    };

    // Specialization for fixed-point decimal: detect overflow on underlying raw_type
    template <unsigned int Scale, std::integral Raw>
    struct interval_overflow_policy<decimal<Scale, Raw>> {
        using dec_t = decimal<Scale, Raw>;
        static constexpr bool add_overflow(const dec_t &a, const dec_t &b) noexcept {
            const Raw ar = a.raw_value();
            const Raw br = b.raw_value();
            return (br > 0) && (ar > std::numeric_limits<Raw>::max() - br);
        }
        static constexpr bool add_underflow(const dec_t &a, const dec_t &b) noexcept {
            const Raw ar = a.raw_value();
            const Raw br = b.raw_value();
            return (br < 0) && (ar < std::numeric_limits<Raw>::min() - br);
        }
        static constexpr bool sub_overflow(const dec_t &a, const dec_t &b) noexcept {
            const Raw ar = a.raw_value();
            const Raw br = b.raw_value();
            // a - b > max <=> a > max + b
            return (br < 0) && (ar > std::numeric_limits<Raw>::max() + br);
        }
        static constexpr bool sub_underflow(const dec_t &a, const dec_t &b) noexcept {
            const Raw ar = a.raw_value();
            const Raw br = b.raw_value();
            // a - b < min <=> a < min + b
            return (br > 0) && (ar < std::numeric_limits<Raw>::min() + br);
        }
    };

    // Types usable in interval must be totally ordered and default-initializable
    // (default value is treated as the additive identity "zero" where needed).
    template <typename T>
    concept interval_scalar = std::totally_ordered<T> && std::default_initializable<T>;

    // Tag type and constants to express infinite bounds in factories
    struct infinity_bound {
        int sign; // -1 for -inf, +1 for +inf
    };
    inline constexpr infinity_bound minus_inf{-1};
    inline constexpr infinity_bound plus_inf{+1};

    // Generic interval type with closure flags.
    template <interval_scalar T> struct interval {
        using value_t = T;
        T lower{};
        T upper{};
        bool lower_closed{false};
        bool upper_closed{false};
        // Infinity metadata (0 = finite; -1 = -inf; +1 = +inf)
        int lower_inf_sign{0};
        int upper_inf_sign{0};

        constexpr interval() noexcept = default;

        // Internal constructor used by factory methods
        constexpr interval(const T &lo, const T &hi, bool lc, bool uc) noexcept
            : lower(lo), upper(hi), lower_closed(lc), upper_closed(uc) {}

      public:
        // Public endpoint comparison: returns true if (a, a_inf) < (b, b_inf).
        static constexpr bool endpoint_less(const T &a, int a_inf_sign, const T &b,
                                            int b_inf_sign) noexcept {
            return less_endpoints(a, a_inf_sign, b, b_inf_sign);
        }
        static constexpr bool endpoint_equal(const T &a, int a_inf_sign, const T &b,
                                             int b_inf_sign) noexcept {
            return !less_endpoints(a, a_inf_sign, b, b_inf_sign) &&
                   !less_endpoints(b, b_inf_sign, a, a_inf_sign);
        }

      private:
        static constexpr bool less_endpoints(const T &a, int a_inf_sign, const T &b,
                                             int b_inf_sign) noexcept {
            // Handle infinities first
            if (a_inf_sign != 0 || b_inf_sign != 0) {
                if (a_inf_sign == -1) {
                    // -inf < anything except -inf
                    return b_inf_sign != -1;
                }
                if (a_inf_sign == +1) {
                    // +inf < anything is false
                    return false;
                }
                // a finite here
                if (b_inf_sign == +1) {
                    return true; // finite < +inf
                }
                if (b_inf_sign == -1) {
                    return false; // finite < -inf is false
                }
            }
            // Both finite
            return a < b;
        }

        static constexpr bool invalid_order(const T &lo, int lo_inf, const T &hi,
                                            int hi_inf) noexcept {
            return less_endpoints(hi, hi_inf, lo, lo_inf);
        }

        // Endpoint arithmetic helpers respecting infinities
        static constexpr void add_endpoint(const T &a, int a_inf, const T &b, int b_inf, T &out_val,
                                           int &out_inf) noexcept {
            if (a_inf == 0 && b_inf == 0) {
                if (interval_overflow_policy<T>::add_overflow(a, b)) {
                    out_inf = +1;
                    return;
                }
                if (interval_overflow_policy<T>::add_underflow(a, b)) {
                    out_inf = -1;
                    return;
                }
                out_val = a + b;
                out_inf = 0;
                return;
            }
            // Any +inf makes the sum +inf unless the other is -inf. For our
            // factories, lower cannot be +inf and upper cannot be -inf, so
            // conflicting infinities do not occur on valid intervals.
            if (a_inf == +1 || b_inf == +1) {
                out_inf = +1;
                return;
            }
            if (a_inf == -1 || b_inf == -1) {
                out_inf = -1;
                return;
            }
            // Fallback (should be unreachable)
            out_inf = 0;
        }

        static constexpr void sub_endpoint(const T &a, int a_inf, const T &b, int b_inf, T &out_val,
                                           int &out_inf) noexcept {
            if (a_inf == 0 && b_inf == 0) {
                if (interval_overflow_policy<T>::sub_overflow(a, b)) {
                    out_inf = +1;
                    return;
                }
                if (interval_overflow_policy<T>::sub_underflow(a, b)) {
                    out_inf = -1;
                    return;
                }
                out_val = a - b;
                out_inf = 0;
                return;
            }
            // +inf - finite, finite - (-inf), +inf - (-inf) => +inf
            if (a_inf == +1 || b_inf == -1) {
                out_inf = +1;
                return;
            }
            // -inf - finite, finite - (+inf), -inf - (+inf) => -inf
            if (a_inf == -1 || b_inf == +1) {
                out_inf = -1;
                return;
            }
            // Fallback
            out_inf = 0;
        }

      public:
        // Represent an empty interval as (v, v). Defaults to (0, 0).
        static interval empty_interval() {
            return open(T{}, T{});
        }

        // Subset check: return true if this interval is a subset of 'other'.
        // Handles empty intervals, open/closed endpoints, and infinities.
        [[nodiscard]] constexpr bool is_subset_of(const interval &other) const noexcept {
            // Empty is subset of any interval
            if (this->is_empty())
                return true;
            // Non-empty cannot be subset of empty
            if (other.is_empty())
                return false;

            // Helper to check endpoint equality considering infinities
            auto endpoints_equal = [](const T &a, int a_inf, const T &b, int b_inf) constexpr {
                return !less_endpoints(a, a_inf, b, b_inf) && !less_endpoints(b, b_inf, a, a_inf);
            };

            // Lower bound: require other.lower <= this.lower
            bool lower_ok = false;
            if (less_endpoints(other.lower, other.lower_inf_sign, lower, lower_inf_sign)) {
                // other.lower < this.lower
                lower_ok = true;
            } else if (endpoints_equal(other.lower, other.lower_inf_sign, lower, lower_inf_sign)) {
                // Equal lower endpoints: if this includes the endpoint, other must also include it
                lower_ok = !(lower_closed && !other.lower_closed);
            } else {
                // this.lower < other.lower -> not subset
                lower_ok = false;
            }

            if (!lower_ok)
                return false;

            // Upper bound: require this.upper <= other.upper
            bool upper_ok = false;
            if (less_endpoints(upper, upper_inf_sign, other.upper, other.upper_inf_sign)) {
                // this.upper < other.upper
                upper_ok = true;
            } else if (endpoints_equal(upper, upper_inf_sign, other.upper, other.upper_inf_sign)) {
                // Equal upper endpoints: if this includes the endpoint, other must also include it
                upper_ok = !(upper_closed && !other.upper_closed);
            } else {
                // other.upper < this.upper -> not subset
                upper_ok = false;
            }

            return upper_ok;
        }

        // Intersection test: return true if this interval intersects 'other'.
        [[nodiscard]] constexpr bool intersects(const interval &other) const noexcept {
            if (this->is_empty() || other.is_empty())
                return false;

            // If this.upper < other.lower -> no intersection
            const bool this_before_other =
                less_endpoints(upper, upper_inf_sign, other.lower, other.lower_inf_sign) ||
                // Equal endpoints but open on at least one side
                (!less_endpoints(upper, upper_inf_sign, other.lower, other.lower_inf_sign) &&
                 !less_endpoints(other.lower, other.lower_inf_sign, upper, upper_inf_sign) &&
                 !(upper_closed && other.lower_closed));

            // If other.upper < this.lower -> no intersection
            const bool other_before_this =
                less_endpoints(other.upper, other.upper_inf_sign, lower, lower_inf_sign) ||
                (!less_endpoints(other.upper, other.upper_inf_sign, lower, lower_inf_sign) &&
                 !less_endpoints(lower, lower_inf_sign, other.upper, other.upper_inf_sign) &&
                 !(other.upper_closed && lower_closed));

            return !(this_before_other || other_before_this);
        }

        // [lo, hi]
        static interval closed(const T &lo, const T &hi) {
            if (invalid_order(lo, 0, hi, 0))
                throw std::invalid_argument("interval.closed: hi < lo");
            return interval{lo, hi, true, true};
        }

        // [lo, hi)
        static interval right_open(const T &lo, const T &hi) { // right-open
            if (invalid_order(lo, 0, hi, 0))
                throw std::invalid_argument("interval.right_open: hi < lo");
            return interval{lo, hi, true, false};
        }
        static interval right_open(const T &lo, infinity_bound hi_inf) {
            const T hi{};
            if (invalid_order(lo, 0, hi, hi_inf.sign))
                throw std::invalid_argument("interval.right_open: hi < lo");
            interval r{lo, hi, true, false};
            r.upper_inf_sign = hi_inf.sign;
            return r;
        }

        // (lo, hi]
        static interval left_open(const T &lo, const T &hi) {
            if (invalid_order(lo, 0, hi, 0))
                throw std::invalid_argument("interval.left_open: hi < lo");
            return interval{lo, hi, false, true};
        }

        static interval left_open(infinity_bound lo_inf, const T &hi) {
            const T lo{};
            if (invalid_order(lo, lo_inf.sign, hi, 0))
                throw std::invalid_argument("interval.left_open: hi < lo");
            interval r{lo, hi, false, true};
            r.lower_inf_sign = lo_inf.sign;
            return r;
        }

        // (lo, hi)
        static interval open(const T &lo, const T &hi) {
            if (invalid_order(lo, 0, hi, 0))
                throw std::invalid_argument("interval.open: hi < lo");
            return interval{lo, hi, false, false};
        }

        static interval open(infinity_bound lo_inf, const T &hi) {
            const T lo{};
            if (invalid_order(lo, lo_inf.sign, hi, 0))
                throw std::invalid_argument("interval.open: hi < lo");
            interval r{lo, hi, false, false};
            r.lower_inf_sign = lo_inf.sign;
            return r;
        }
        static interval open(const T &lo, infinity_bound hi_inf) {
            const T hi{};
            if (invalid_order(lo, 0, hi, hi_inf.sign))
                throw std::invalid_argument("interval.open: hi < lo");
            interval r{lo, hi, false, false};
            r.upper_inf_sign = hi_inf.sign;
            return r;
        }

        static interval open(infinity_bound lo_inf, infinity_bound hi_inf) {
            const T lo{};
            const T hi{};
            if (invalid_order(lo, lo_inf.sign, hi, hi_inf.sign))
                throw std::invalid_argument("interval.open: hi < lo");
            interval r{lo, hi, false, false};
            r.lower_inf_sign = lo_inf.sign;
            r.upper_inf_sign = hi_inf.sign;
            return r;
        }

        [[nodiscard]] constexpr bool is_empty() const noexcept {
            return !lower_closed && !upper_closed &&
                   !less_endpoints(lower, lower_inf_sign, upper, upper_inf_sign) &&
                   !less_endpoints(upper, upper_inf_sign, lower, lower_inf_sign);
        }

        [[nodiscard]] constexpr bool is_lower_infinite() const noexcept {
            return lower_inf_sign != 0;
        }
        [[nodiscard]] constexpr bool is_upper_infinite() const noexcept {
            return upper_inf_sign != 0;
        }

        // True if this interval is a single point [v, v] (finite, both closed).
        [[nodiscard]] constexpr bool is_punctual() const noexcept {
            return lower_closed && upper_closed && lower_inf_sign == 0 && upper_inf_sign == 0 &&
                   lower == upper;
        }

        // Returns the intersection of this interval with other.
        // Returns empty_interval() if the two intervals do not intersect.
        [[nodiscard]] constexpr interval intersection(const interval &other) const noexcept {
            if (!intersects(other))
                return interval::empty_interval();

            // Lower = max(this.lower, other.lower)
            T lo{};
            int lo_inf{0};
            bool lo_closed{};
            if (less_endpoints(lower, lower_inf_sign, other.lower, other.lower_inf_sign)) {
                lo        = other.lower;
                lo_inf    = other.lower_inf_sign;
                lo_closed = other.lower_closed;
            } else if (less_endpoints(other.lower, other.lower_inf_sign, lower, lower_inf_sign)) {
                lo        = lower;
                lo_inf    = lower_inf_sign;
                lo_closed = lower_closed;
            } else {
                lo        = lower;
                lo_inf    = lower_inf_sign;
                lo_closed = lower_closed && other.lower_closed; // more restrictive
            }

            // Upper = min(this.upper, other.upper)
            T hi{};
            int hi_inf{0};
            bool hi_closed{};
            if (less_endpoints(upper, upper_inf_sign, other.upper, other.upper_inf_sign)) {
                hi        = upper;
                hi_inf    = upper_inf_sign;
                hi_closed = upper_closed;
            } else if (less_endpoints(other.upper, other.upper_inf_sign, upper, upper_inf_sign)) {
                hi        = other.upper;
                hi_inf    = other.upper_inf_sign;
                hi_closed = other.upper_closed;
            } else {
                hi        = upper;
                hi_inf    = upper_inf_sign;
                hi_closed = upper_closed && other.upper_closed; // more restrictive
            }

            interval r{};
            r.lower          = lo;
            r.lower_inf_sign = lo_inf;
            r.lower_closed   = lo_closed;
            r.upper          = hi;
            r.upper_inf_sign = hi_inf;
            r.upper_closed   = hi_closed;
            return r;
        }

        // Minkowski addition of intervals
        friend constexpr interval operator+(const interval &a, const interval &b) {
            if (a.is_empty() || b.is_empty()) {
                return interval::empty_interval();
            }
            interval r{};
            add_endpoint(a.lower, a.lower_inf_sign, b.lower, b.lower_inf_sign, r.lower,
                         r.lower_inf_sign);
            add_endpoint(a.upper, a.upper_inf_sign, b.upper, b.upper_inf_sign, r.upper,
                         r.upper_inf_sign);
            if (invalid_order(r.lower, r.lower_inf_sign, r.upper, r.upper_inf_sign)) {
                return interval::empty_interval();
            }
            r.lower_closed = a.lower_closed && b.lower_closed;
            r.upper_closed = a.upper_closed && b.upper_closed;
            return r;
        }

        // Interval subtraction: {l - e | l ∈ L, e ∈ E}
        friend constexpr interval operator-(const interval &l, const interval &e) {
            if (l.is_empty() || e.is_empty()) {
                return interval::empty_interval();
            }
            interval r{};
            sub_endpoint(l.lower, l.lower_inf_sign, e.upper, e.upper_inf_sign, r.lower,
                         r.lower_inf_sign);
            sub_endpoint(l.upper, l.upper_inf_sign, e.lower, e.lower_inf_sign, r.upper,
                         r.upper_inf_sign);
            if (invalid_order(r.lower, r.lower_inf_sign, r.upper, r.upper_inf_sign)) {
                return interval::empty_interval();
            }
            r.lower_closed = l.lower_closed && e.upper_closed;
            r.upper_closed = l.upper_closed && e.lower_closed;
            return r;
        }
    };

} // namespace cadmia::modeling
