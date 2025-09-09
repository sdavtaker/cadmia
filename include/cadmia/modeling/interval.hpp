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
#include <type_traits>

namespace cadmia::modeling {

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
    int  lower_inf_sign{0};
    int  upper_inf_sign{0};

        constexpr interval() noexcept = default;

        // Internal constructor used by factory methods
        constexpr interval(const T &lo, const T &hi, bool lc, bool uc) noexcept
            : lower(lo), upper(hi), lower_closed(lc), upper_closed(uc) {}

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
        static constexpr void add_endpoint(const T &a, int a_inf, const T &b, int b_inf,
                                           T &out_val, int &out_inf) noexcept {
            if (a_inf == 0 && b_inf == 0) {
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

        static constexpr void sub_endpoint(const T &a, int a_inf, const T &b, int b_inf,
                                           T &out_val, int &out_inf) noexcept {
            if (a_inf == 0 && b_inf == 0) {
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

        // Minkowski addition of intervals
        friend constexpr interval operator+(const interval &a, const interval &b) {
            if (a.is_empty() || b.is_empty()) {
                return interval::empty_interval();
            }
            interval r{};
            add_endpoint(a.lower, a.lower_inf_sign, b.lower, b.lower_inf_sign,
                         r.lower, r.lower_inf_sign);
            add_endpoint(a.upper, a.upper_inf_sign, b.upper, b.upper_inf_sign,
                         r.upper, r.upper_inf_sign);
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
            sub_endpoint(l.lower, l.lower_inf_sign, e.upper, e.upper_inf_sign,
                         r.lower, r.lower_inf_sign);
            sub_endpoint(l.upper, l.upper_inf_sign, e.lower, e.lower_inf_sign,
                         r.upper, r.upper_inf_sign);
            if (invalid_order(r.lower, r.lower_inf_sign, r.upper, r.upper_inf_sign)) {
                return interval::empty_interval();
            }
            r.lower_closed = l.lower_closed && e.upper_closed;
            r.upper_closed = l.upper_closed && e.lower_closed;
            return r;
        }
    };

} // namespace cadmia::modeling
