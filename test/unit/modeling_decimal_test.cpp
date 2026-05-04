// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2013-present, Damian Vicino
 * Carleton University, Universite de Nice-Sophia Antipolis
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

#include <cadmia/modeling/decimal.hpp>

using cadmia::modeling::decimal;

TEST_CASE("decimal<3> from_scaled and from_whole", "[decimal]") {
  using d3 = decimal<3>;
  auto a = d3::from_whole(1);         // 1.000
  auto b = d3::from_scaled(2500);     // 2.500
  auto c = d3::from_scaled(1500);     // 1.500

  REQUIRE(b > a);
  REQUIRE(c > a);
  REQUIRE(b > c);
}

TEST_CASE("decimal<3> addition and subtraction", "[decimal]") {
  using d3 = decimal<3>;
  auto one = d3::from_whole(1);       // 1.000
  auto one_point_two = d3::from_scaled(1200); // 1.200
  auto sum = one + one_point_two;     // 2.200
  auto diff = one_point_two - one;    // 0.200

  // Compare via ordering with known scaled constructions
  REQUIRE(sum > d3::from_scaled(2199));
  REQUIRE(sum == d3::from_scaled(2200));
  REQUIRE(sum < d3::from_scaled(2201));

  REQUIRE(diff == d3::from_scaled(200));
}

TEST_CASE("decimal<2> supports independent scale", "[decimal]") {
  using d2 = decimal<2>;
  auto a = d2::from_whole(3);   // 300
  auto b = d2::from_scaled(45); // 0.45
  auto c = a - b;               // 2.55
  REQUIRE(c == d2::from_scaled(255));
}
