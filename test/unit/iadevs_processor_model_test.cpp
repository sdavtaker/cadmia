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

// Basic tests for IA-DEVS processor model
#include <cadmia/basic_model/iadevs/processor.hpp>
#include <cadmia/concepts/iadevs_atomic_model.hpp>

#include <catch2/catch_test_macros.hpp>

using cadmia::iadevs::processor;

// Compile-time verification that processor satisfies the IADEVSAtomicModel concept
static_assert(cadmia::IADEVSAtomicModel<processor>,
              "processor must satisfy IADEVSAtomicModel concept");

namespace {
    // Helpers to make expectations readable: convert millisecond-scaled integer to double seconds
    static inline double ts(int raw_scaled) {
        return raw_scaled * 0.001;
    }
    // Build a job interval [id, id]
    static inline processor::input_i_t job(int id) {
        return processor::input_i_t::closed(id, id);
    }
} // namespace

SCENARIO("IA-DEVS processor state construction and passivity", "[iadevs][processor]") {
    GIVEN("an idle processor (empty queue)") {
        processor::state_i_t s = processor::state_i_t::empty_interval();

        THEN("time_advance returns an empty interval (passive)") {
            const auto ta = processor::time_advance(s);
            REQUIRE(ta.is_empty());
        }
    }

    GIVEN("a processor with one job in the queue") {
        processor::state_t sl{}, sr{};
        sl.tocj = ts(0);
        sr.tocj = ts(0);
        sl.qj.clear();
        sr.qj.clear();
        sl.qj.push_back(1);
        sr.qj.push_back(1);
        processor::state_i_t s = processor::state_i_t::closed(sl, sr);

        THEN("time_advance returns [0.250, 0.250] (processing time)") {
            const auto ta = processor::time_advance(s);
            REQUIRE_FALSE(ta.is_empty());
            REQUIRE(ta.lower == ts(250));
            REQUIRE(ta.upper == ts(250));
            REQUIRE(ta.lower_closed);
            REQUIRE(ta.upper_closed);
        }

        THEN("output returns the first job [1,1]") {
            const auto y = processor::output(s);
            REQUIRE_FALSE(y.is_empty());
            REQUIRE(y.lower == 1);
            REQUIRE(y.upper == 1);
        }
    }

    GIVEN("a processor with two jobs in the queue") {
        processor::state_t sl{}, sr{};
        sl.tocj                = ts(0);
        sr.tocj                = ts(0);
        sl.qj                  = {1, 2};
        sr.qj                  = {3, 4};
        processor::state_i_t s = processor::state_i_t::closed(sl, sr);

        THEN("time_advance returns [0.250, 0.250] (processing time)") {
            const auto ta = processor::time_advance(s);
            REQUIRE_FALSE(ta.is_empty());
            REQUIRE(ta.lower == ts(250));
            REQUIRE(ta.upper == ts(250));
            REQUIRE(ta.lower_closed);
            REQUIRE(ta.upper_closed);
        }

        THEN("output returns the first job [1,1]") {
            const auto y = processor::output(s);
            REQUIRE_FALSE(y.is_empty());
            REQUIRE(y.lower == 1);
            REQUIRE(y.upper == 3);
            REQUIRE(y.lower_closed);
            REQUIRE(y.upper_closed);
        }
    }

    GIVEN("a processor with lower QJ empty and one item in the upper QJ") {
        processor::state_t sl{}, sr{};
        sl.tocj = ts(0);
        sr.tocj = ts(0);
        sl.qj.clear();
        sr.qj.clear();
        sr.qj.push_back(1);
        processor::state_i_t s = processor::state_i_t::closed(sl, sr);

        THEN("time_advance returns [0.250 - tocj_upper, +inf) with lower closed and upper infinite "
             "open") {
            const auto ta = processor::time_advance(s);
            REQUIRE_FALSE(ta.is_empty());
            REQUIRE(ta.lower == ts(250));
            REQUIRE(ta.is_upper_infinite());
            REQUIRE(ta.lower_closed);
            REQUIRE_FALSE(ta.upper_closed);
        }
    }
}

SCENARIO("IA-DEVS processor internal transition", "[iadevs][processor]") {
    GIVEN("a processor with two jobs [1,1] and [2,2] and tocj=[0.100, 0.150]") {
        processor::state_t sl{}, sr{};
        sl.tocj                = ts(100);
        sr.tocj                = ts(150);
        sl.qj                  = {1, 2};
        sr.qj                  = {1, 2};
        processor::state_i_t s = processor::state_i_t::closed(sl, sr);

        WHEN("internal_transition is applied") {
            const auto s_new = processor::internal_transition(s);

            THEN("the first job is dequeued and tocj is reset to [0,0]") {
                REQUIRE(s_new.lower.qj.size() == 1);
                REQUIRE(s_new.upper.qj.size() == 1);
                REQUIRE(s_new.lower.qj.front() == 2);
                REQUIRE(s_new.upper.qj.front() == 2);
                REQUIRE(s_new.lower.tocj == ts(0));
                REQUIRE(s_new.upper.tocj == ts(0));
            }
        }
    }

    GIVEN("a processor with an empty queue") {
        processor::state_t sl{}, sr{};
        sl.tocj                = ts(0);
        sr.tocj                = ts(0);
        processor::state_i_t s = processor::state_i_t::closed(sl, sr);

        THEN("internal_transition returns empty interval when queue size <= 1") {
            REQUIRE(processor::internal_transition(s).is_empty());
        }
    }
}

SCENARIO("IA-DEVS processor external transition when idle", "[iadevs][processor]") {
    GIVEN("an idle processor and a new job [3,3]") {
        processor::state_t sl{}, sr{};
        sl.tocj = ts(0);
        sr.tocj = ts(0);
        sl.qj.clear();
        sr.qj.clear();
        processor::state_i_t s = processor::state_i_t::closed(sl, sr);

        processor::time_i_t elapsed =
            processor::time_i_t::closed(ts(50), ts(100)); // arbitrary elapsed

        WHEN("external_transition is applied with job [3,3]") {
            const auto s_new = processor::external_transition(s, elapsed, job(3));

            THEN("the job is enqueued and tocj is updated by elapsed") {
                REQUIRE(s_new.lower.qj.size() == 1);
                REQUIRE(s_new.upper.qj.size() == 1);
                REQUIRE(s_new.lower.qj.front() == 3);
                REQUIRE(s_new.upper.qj.front() == 3);
                REQUIRE(s_new.lower.tocj == ts(50));
                REQUIRE(s_new.upper.tocj == ts(100));
            }
        }
    }
}

SCENARIO("IA-DEVS processor external transition when busy", "[iadevs][processor]") {
    GIVEN("a busy processor with one job and tocj=[0.050, 0.100]") {
        processor::state_t sl{}, sr{};
        sl.tocj                = ts(50);
        sr.tocj                = ts(100);
        sl.qj                  = {1};
        sr.qj                  = {1};
        processor::state_i_t s = processor::state_i_t::closed(sl, sr);

        processor::time_i_t elapsed = processor::time_i_t::closed(ts(80), ts(120)); // elapsed

        WHEN("external_transition is applied with job [4,4]") {
            const auto s_new = processor::external_transition(s, elapsed, job(4));

            THEN("the job is enqueued and tocj is updated (clamped to [0, 0.250])") {
                REQUIRE(s_new.lower.qj.size() == 2);
                REQUIRE(s_new.upper.qj.size() == 2);
                REQUIRE(s_new.lower.qj.front() == 1);
                REQUIRE(s_new.upper.qj.front() == 1);
                REQUIRE(s_new.lower.qj.back() == 4);
                REQUIRE(s_new.upper.qj.back() == 4);
                // tocj = [0.050, 0.100] + [0.080, 0.120] = [0.130, 0.220]
                REQUIRE(s_new.lower.tocj == ts(130));
                REQUIRE(s_new.upper.tocj == ts(220));
            }
        }
    }

    GIVEN("a busy processor with elapsed exceeding processing time") {
        processor::state_t sl{}, sr{};
        sl.tocj                = ts(100);
        sr.tocj                = ts(150);
        sl.qj                  = {2};
        sr.qj                  = {2};
        processor::state_i_t s = processor::state_i_t::closed(sl, sr);

        processor::time_i_t elapsed =
            processor::time_i_t::closed(ts(200), ts(300)); // exceeds [0, 0.250]

        THEN("external_transition throws because elapsed is out of bounds") {
            REQUIRE_THROWS_AS(processor::external_transition(s, elapsed, job(5)),
                              std::invalid_argument);
        }
    }
}

SCENARIO("IA-DEVS processor time_advance with partial progress", "[iadevs][processor]") {
    GIVEN("a processor with one job and tocj=[0.100, 0.150]") {
        processor::state_t sl{}, sr{};
        sl.tocj                = ts(100);
        sr.tocj                = ts(150);
        sl.qj                  = {1};
        sr.qj                  = {1};
        processor::state_i_t s = processor::state_i_t::closed(sl, sr);

        WHEN("time_advance is computed") {
            const auto ta = processor::time_advance(s);

            THEN("TA = [0.250, 0.250] - [0.100, 0.150] = [0.100, 0.150]") {
                REQUIRE_FALSE(ta.is_empty());
                REQUIRE(ta.lower == ts(100)); // 250 - 150
                REQUIRE(ta.upper == ts(150)); // 250 - 100
            }
        }
    }
}
