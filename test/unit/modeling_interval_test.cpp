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

#include <cadmia/modeling/decimal.hpp>
#include <cadmia/modeling/interval.hpp>

#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>

using cadmia::modeling::decimal;

// Convenience sentinels for double-based infinity tests
static const double D_POS_INF = std::numeric_limits<double>::infinity();
static const double D_NEG_INF = -std::numeric_limits<double>::infinity();

TEST_CASE("interval closed constructor and flags", "[interval]") {
    const auto iv = cadmia::modeling::interval<int>::closed(1, 3);
    REQUIRE(iv.lower == 1);
    REQUIRE(iv.upper == 3);
    REQUIRE(iv.lower_closed);
    REQUIRE(iv.upper_closed);
    REQUIRE_FALSE(iv.is_empty());
}

TEST_CASE("interval right_open and left_open closures", "[interval]") {
    const auto r = cadmia::modeling::interval<int>::right_open(1, 3); // [1,3)
    REQUIRE(r.lower == 1);
    REQUIRE(r.upper == 3);
    REQUIRE(r.lower_closed);
    REQUIRE_FALSE(r.upper_closed);

    const auto l = cadmia::modeling::interval<int>::left_open(1, 3); // (1,3]
    REQUIRE(l.lower == 1);
    REQUIRE(l.upper == 3);
    REQUIRE_FALSE(l.lower_closed);
    REQUIRE(l.upper_closed);
}

TEST_CASE("Open interval (v,v) is empty", "[interval]") {
    const auto e1 = cadmia::modeling::interval<int>::open(0, 0);
    REQUIRE(e1.is_empty());

    const auto e2 = cadmia::modeling::interval<double>::open(1.0, 1.0);
    REQUIRE(e2.is_empty());
}

TEST_CASE("Half-open interval (v,v] and [v,v) are empty", "[interval]") {
    // (a, a] is mathematically empty
    const auto lo = cadmia::modeling::interval<int>::left_open(0, 0);
    REQUIRE(lo.is_empty());

    const auto lo2 = cadmia::modeling::interval<double>::left_open(1.0, 1.0);
    REQUIRE(lo2.is_empty());

    // [a, a) is also empty
    const auto ro = cadmia::modeling::interval<int>::right_open(0, 0);
    REQUIRE(ro.is_empty());

    const auto ro2 = cadmia::modeling::interval<double>::right_open(2.5, 2.5);
    REQUIRE(ro2.is_empty());

    // [a, a] is NOT empty
    const auto pt = cadmia::modeling::interval<int>::closed(3, 3);
    REQUIRE_FALSE(pt.is_empty());
}

TEST_CASE("interval throws when hi < lo", "[interval]") {
    REQUIRE_THROWS_AS((cadmia::modeling::interval<int>::closed(5, 4)), std::invalid_argument);
    REQUIRE_THROWS_AS((cadmia::modeling::interval<int>::right_open(5, 4)), std::invalid_argument);
    REQUIRE_THROWS_AS((cadmia::modeling::interval<int>::left_open(5, 4)), std::invalid_argument);
    REQUIRE_THROWS_AS((cadmia::modeling::interval<int>::open(5, 4)), std::invalid_argument);
}

TEST_CASE("interval works with decimal type", "[interval][decimal]") {
    using d3      = decimal<3>;
    const auto a  = d3::from_whole(1);     // 1.000
    const auto b  = d3::from_scaled(2500); // 2.500
    const auto iv = cadmia::modeling::interval<d3>::closed(a, b);
    REQUIRE_FALSE(iv.is_empty());
    REQUIRE(iv.lower == a);
    REQUIRE(iv.upper == b);
}

TEST_CASE("interval supports open infinities on either side", "[interval][infinite]") {
    using I = cadmia::modeling::interval<double>;

    // (-inf, 10.0)
    {
        const auto i = I::open(D_NEG_INF, 10.0);
        REQUIRE(i.is_lower_infinite());
        REQUIRE_FALSE(i.is_upper_infinite());
        REQUIRE_FALSE(i.lower_closed);
        REQUIRE_FALSE(i.upper_closed);
    }

    // (10.0, +inf)
    {
        const auto i = I::open(10.0, D_POS_INF);
        REQUIRE_FALSE(i.is_lower_infinite());
        REQUIRE(i.is_upper_infinite());
        REQUIRE_FALSE(i.lower_closed);
        REQUIRE_FALSE(i.upper_closed);
    }

    // (-inf, +inf)
    {
        const auto i = I::open(D_NEG_INF, D_POS_INF);
        REQUIRE(i.is_lower_infinite());
        REQUIRE(i.is_upper_infinite());
        REQUIRE_FALSE(i.lower_closed);
        REQUIRE_FALSE(i.upper_closed);
        REQUIRE_FALSE(i.is_empty());
    }
}

TEST_CASE("interval supports left_open with -inf on left and right_open with +inf on right",
          "[interval][infinite][closures]") {
    using I = cadmia::modeling::interval<double>;

    // (-inf, 5.0]
    {
        const auto i = I::left_open(D_NEG_INF, 5.0);
        REQUIRE(i.is_lower_infinite());
        REQUIRE_FALSE(i.is_upper_infinite());
        REQUIRE_FALSE(i.lower_closed);
        REQUIRE(i.upper_closed);
        REQUIRE_FALSE(i.is_empty());
    }

    // [5.0, +inf)
    {
        const auto i = I::right_open(5.0, D_POS_INF);
        REQUIRE_FALSE(i.is_lower_infinite());
        REQUIRE(i.is_upper_infinite());
        REQUIRE(i.lower_closed);
        REQUIRE_FALSE(i.upper_closed);
        REQUIRE_FALSE(i.is_empty());
    }
}

TEST_CASE("interval infinite order validation throws when reversed",
          "[interval][infinite][order]") {
    using I = cadmia::modeling::interval<double>;

    // (10.0, -inf) is invalid
    REQUIRE_THROWS_AS((I::open(10.0, D_NEG_INF)), std::invalid_argument);
    // (+inf, 10.0) is invalid
    REQUIRE_THROWS_AS((I::open(D_POS_INF, 10.0)), std::invalid_argument);
}

TEST_CASE("interval operator+ (finite ends)", "[interval][ops]") {
    using I      = cadmia::modeling::interval<int>;
    const auto a = I::closed(1, 2);    // [1,2]
    const auto b = I::left_open(3, 5); // (3,5]
    const auto s = a + b;              // expect (1+3, 2+5] = (4,7]
    REQUIRE(s.lower == 4);
    REQUIRE(s.upper == 7);
    REQUIRE_FALSE(s.lower_closed);
    REQUIRE(s.upper_closed);
    REQUIRE_FALSE(s.is_empty());
}

TEST_CASE("interval operator- (finite ends)", "[interval][ops]") {
    using I      = cadmia::modeling::interval<int>;
    const auto l = I::closed(5, 8);     // [5,8]
    const auto e = I::right_open(2, 3); // [2,3)
    const auto d = l - e;               // [5-3, 8-2] with closures: (2,6]
    REQUIRE(d.lower == 2);
    REQUIRE(d.upper == 6);
    REQUIRE_FALSE(d.lower_closed);
    REQUIRE(d.upper_closed);
    REQUIRE_FALSE(d.is_empty());
}

TEST_CASE("interval operator+ with infinities", "[interval][ops][infinite]") {
    using I = cadmia::modeling::interval<double>;

    // (-inf, 10.0) + [2.0, 3.0] = (-inf, 13.0)
    const auto a = I::open(D_NEG_INF, 10.0);
    const auto b = I::closed(2.0, 3.0);
    const auto s = a + b;
    REQUIRE(s.is_lower_infinite());
    REQUIRE_FALSE(s.is_upper_infinite());
    REQUIRE(s.upper == 13.0);
    REQUIRE_FALSE(s.lower_closed);
    REQUIRE_FALSE(s.upper_closed);
}

TEST_CASE("interval operator- with infinities", "[interval][ops][infinite]") {
    using I = cadmia::modeling::interval<double>;

    // [5.0, 10.0) - (-inf, 3.0] = [2.0, +inf)
    const auto l = I::right_open(5.0, 10.0);
    const auto e = I::left_open(D_NEG_INF, 3.0);
    const auto d = l - e;
    REQUIRE_FALSE(d.is_lower_infinite());
    REQUIRE(d.lower == 2.0);
    REQUIRE(d.is_upper_infinite());
    REQUIRE(d.lower_closed);       // true && true
    REQUIRE_FALSE(d.upper_closed); // false && false
}

TEST_CASE("interval operator with empty intervals stays empty", "[interval][ops][empty]") {
    using I          = cadmia::modeling::interval<int>;
    const auto empty = I::open(0, 0);
    const auto one   = I::closed(1, 1);
    REQUIRE((empty + one).is_empty());
    REQUIRE((one - empty).is_empty());
}
