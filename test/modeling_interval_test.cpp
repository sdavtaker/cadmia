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

#include <catch2/catch_test_macros.hpp>
#include <stdexcept>

#include <cadmia/modeling/interval.hpp>
#include <cadmia/modeling/decimal.hpp>

using cadmia::modeling::decimal;

TEST_CASE("Interval closed constructor and flags", "[interval]") {
  const auto iv = cadmia::modeling::Interval<int>::closed(1, 3);
  REQUIRE(iv.lower == 1);
  REQUIRE(iv.upper == 3);
  REQUIRE(iv.lower_closed);
  REQUIRE(iv.upper_closed);
  REQUIRE_FALSE(iv.is_empty());
}

TEST_CASE("Interval right_open and left_open closures", "[interval]") {
  const auto r = cadmia::modeling::Interval<int>::right_open(1, 3); // [1,3)
  REQUIRE(r.lower == 1);
  REQUIRE(r.upper == 3);
  REQUIRE(r.lower_closed);
  REQUIRE_FALSE(r.upper_closed);

  const auto l = cadmia::modeling::Interval<int>::left_open(1, 3); // (1,3]
  REQUIRE(l.lower == 1);
  REQUIRE(l.upper == 3);
  REQUIRE_FALSE(l.lower_closed);
  REQUIRE(l.upper_closed);
}

TEST_CASE("Open interval (v,v) is empty", "[interval]") {
  const auto e1 = cadmia::modeling::Interval<int>::open(0, 0);
  REQUIRE(e1.is_empty());

  const auto e2 = cadmia::modeling::Interval<double>::open(1.0, 1.0);
  REQUIRE(e2.is_empty());
}

TEST_CASE("Interval throws when hi < lo", "[interval]") {
  REQUIRE_THROWS_AS((cadmia::modeling::Interval<int>::closed(5, 4)), std::invalid_argument);
  REQUIRE_THROWS_AS((cadmia::modeling::Interval<int>::right_open(5, 4)), std::invalid_argument);
  REQUIRE_THROWS_AS((cadmia::modeling::Interval<int>::left_open(5, 4)), std::invalid_argument);
  REQUIRE_THROWS_AS((cadmia::modeling::Interval<int>::open(5, 4)), std::invalid_argument);
}

TEST_CASE("Interval works with decimal type", "[interval][decimal]") {
  using d3 = decimal<3>;
  const auto a = d3::from_whole(1);     // 1.000
  const auto b = d3::from_scaled(2500); // 2.500
  const auto iv = cadmia::modeling::Interval<d3>::closed(a, b);
  REQUIRE_FALSE(iv.is_empty());
  REQUIRE(iv.lower == a);
  REQUIRE(iv.upper == b);
}
