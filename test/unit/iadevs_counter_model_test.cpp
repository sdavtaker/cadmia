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

// Basic tests for IA-DEVS counter model
#include <catch2/catch_test_macros.hpp>

#include <cadmia/basic_model/iadevs/counter.hpp>
#include <cadmia/concepts/iadevs_atomic_model.hpp>

using cadmia::iadevs::counter;

// Compile-time verification that counter satisfies the IADEVSAtomicModel concept
static_assert(cadmia::IADEVSAtomicModel<counter>, 
              "counter must satisfy IADEVSAtomicModel concept");

namespace {
    // Helper to create Add event interval
    static inline counter::input_i_t add_event() {
        return counter::input_i_t::closed(counter::input_event_t::add, 
                                          counter::input_event_t::add);
    }

    // Helper to create Reset event interval
    static inline counter::input_i_t reset_event() {
        return counter::input_i_t::closed(counter::input_event_t::reset, 
                                          counter::input_event_t::reset);
    }

    // Helper to create a state with given count in passive phase
    static inline counter::state_i_t passive_state(int count) {
        counter::state_t s;
        s.count = count;
        s.phase = counter::phase_t::passive;
        return counter::state_i_t::closed(s, s);
    }

    // Helper to create a state with given count in output phase
    static inline counter::state_i_t output_state(int count) {
        counter::state_t s;
        s.count = count;
        s.phase = counter::phase_t::output;
        return counter::state_i_t::closed(s, s);
    }

    // Helper to create elapsed time interval
    static inline counter::time_i_t elapsed(int low_ms, int high_ms) {
        return counter::time_i_t::closed(counter::dec3::from_scaled(low_ms), 
                                         counter::dec3::from_scaled(high_ms));
    }
}

SCENARIO("IA-DEVS counter initial state and passivity", "[iadevs][counter]") {
    GIVEN("an initial counter state (count=0, passive)") {
        const auto s0 = passive_state(0);

        THEN("the state interval is non-empty and closed") {
            REQUIRE_FALSE(s0.is_empty());
            REQUIRE(s0.lower.count == 0);
            REQUIRE(s0.upper.count == 0);
            REQUIRE(s0.lower.phase == counter::phase_t::passive);
            REQUIRE(s0.upper.phase == counter::phase_t::passive);
            REQUIRE(s0.lower_closed);
            REQUIRE(s0.upper_closed);
        }

        THEN("time_advance returns empty interval (passive/infinite)") {
            const auto ta = counter::time_advance(s0);
            REQUIRE(ta.is_empty());
        }
    }
}

SCENARIO("IA-DEVS counter Add event processing", "[iadevs][counter]") {
    GIVEN("a passive counter with count=0") {
        const auto state = passive_state(0);
        const auto elapsed_time = elapsed(0, 0); // arbitrary

        WHEN("an Add event is received") {
            const auto new_state = counter::external_transition(state, elapsed_time, add_event());

            THEN("count increments to 1 and remains passive") {
                REQUIRE_FALSE(new_state.is_empty());
                REQUIRE(new_state.lower.count == 1);
                REQUIRE(new_state.upper.count == 1);
                REQUIRE(new_state.lower.phase == counter::phase_t::passive);
                REQUIRE(new_state.upper.phase == counter::phase_t::passive);
            }

            THEN("time_advance is still empty (passive)") {
                const auto ta = counter::time_advance(new_state);
                REQUIRE(ta.is_empty());
            }
        }
    }

    GIVEN("a passive counter with count=5") {
        const auto state = passive_state(5);
        const auto elapsed_time = elapsed(0, 0);

        WHEN("an Add event is received") {
            const auto new_state = counter::external_transition(state, elapsed_time, add_event());

            THEN("count increments to 6 and remains passive") {
                REQUIRE(new_state.lower.count == 6);
                REQUIRE(new_state.upper.count == 6);
                REQUIRE(new_state.lower.phase == counter::phase_t::passive);
                REQUIRE(new_state.upper.phase == counter::phase_t::passive);
            }
        }
    }

    GIVEN("a passive counter with interval count [3, 5]") {
    counter::state_t lower_state{counter::phase_t::passive, 3};
    counter::state_t upper_state{counter::phase_t::passive, 5};
        const auto state = counter::state_i_t::closed(lower_state, upper_state);
        const auto elapsed_time = elapsed(0, 0);

        WHEN("an Add event is received") {
            const auto new_state = counter::external_transition(state, elapsed_time, add_event());

            THEN("count interval becomes [4, 6]") {
                REQUIRE(new_state.lower.count == 4);
                REQUIRE(new_state.upper.count == 6);
                REQUIRE(new_state.lower.phase == counter::phase_t::passive);
                REQUIRE(new_state.upper.phase == counter::phase_t::passive);
            }
        }
    }
}

SCENARIO("IA-DEVS counter uncertain input [Add, Reset] processing", "[iadevs][counter]") {
    GIVEN("a passive counter with interval count [3, 5]") {
        counter::state_t lower_state{counter::phase_t::passive, 3};
        counter::state_t upper_state{counter::phase_t::passive, 5};
        const auto state = counter::state_i_t::closed(lower_state, upper_state);
        const auto elapsed_time = elapsed(0, 0);

        // Mixed input interval [Add, Reset]
        const auto mixed_input = counter::input_i_t::closed(counter::input_event_t::add,
                                                           counter::input_event_t::reset);

        WHEN("an uncertain [Add, Reset] input is received") {
            const auto new_state = counter::external_transition(state, elapsed_time, mixed_input);

            THEN("lower stays passive without increment, upper becomes output with increment") {
                REQUIRE_FALSE(new_state.is_empty());
                REQUIRE(new_state.lower.phase == counter::phase_t::passive);
                REQUIRE(new_state.upper.phase == counter::phase_t::output);
                REQUIRE(new_state.lower.count == 3);  // lower unchanged
                REQUIRE(new_state.upper.count == 6);  // upper incremented: 5+1=6
            }

            THEN("time_advance for mixed phase becomes [0, +inf) right-open") {
                const auto ta = counter::time_advance(new_state);
                REQUIRE_FALSE(ta.is_empty());
                REQUIRE(ta.lower == counter::dec3{});
                REQUIRE(ta.upper_inf_sign == +1); // +inf
                REQUIRE(ta.lower_closed);
                REQUIRE_FALSE(ta.upper_closed);
            }
        }
    }
}

SCENARIO("IA-DEVS counter Reset event processing", "[iadevs][counter]") {
    GIVEN("a passive counter with count=3") {
        const auto state = passive_state(3);
        const auto elapsed_time = elapsed(0, 0);

        WHEN("a Reset event is received") {
            const auto new_state = counter::external_transition(state, elapsed_time, reset_event());

            THEN("count remains 3 but phase changes to output") {
                REQUIRE(new_state.lower.count == 3);
                REQUIRE(new_state.upper.count == 3);
                REQUIRE(new_state.lower.phase == counter::phase_t::output);
                REQUIRE(new_state.upper.phase == counter::phase_t::output);
            }

            THEN("time_advance returns [0,0] (immediate)") {
                const auto ta = counter::time_advance(new_state);
                REQUIRE_FALSE(ta.is_empty());
                REQUIRE(ta.lower == counter::dec3{});
                REQUIRE(ta.upper == counter::dec3{});
                REQUIRE(ta.lower_closed);
                REQUIRE(ta.upper_closed);
            }

            AND_WHEN("output is called") {
                const auto y = counter::output(new_state);

                THEN("output is [3, 3]") {
                    REQUIRE_FALSE(y.is_empty());
                    REQUIRE(y.lower == 3);
                    REQUIRE(y.upper == 3);
                }
            }
        }
    }

    GIVEN("a passive counter with count=0") {
        const auto state = passive_state(0);
        const auto elapsed_time = elapsed(0, 0);

        WHEN("a Reset event is received (output zero)") {
            const auto new_state = counter::external_transition(state, elapsed_time, reset_event());

            THEN("count is 0 and phase is output") {
                REQUIRE(new_state.lower.count == 0);
                REQUIRE(new_state.upper.count == 0);
                REQUIRE(new_state.lower.phase == counter::phase_t::output);
            }

            AND_WHEN("output is called") {
                const auto y = counter::output(new_state);

                THEN("output is [0, 0]") {
                    REQUIRE(y.lower == 0);
                    REQUIRE(y.upper == 0);
                }
            }
        }
    }

    GIVEN("a passive counter with interval count [7, 10]") {
    counter::state_t lower_state{counter::phase_t::passive, 7};
    counter::state_t upper_state{counter::phase_t::passive, 10};
        const auto state = counter::state_i_t::closed(lower_state, upper_state);
        const auto elapsed_time = elapsed(0, 0);

        WHEN("a Reset event is received") {
            const auto new_state = counter::external_transition(state, elapsed_time, reset_event());

            THEN("count interval [7, 10] is preserved, phase is output") {
                REQUIRE(new_state.lower.count == 7);
                REQUIRE(new_state.upper.count == 10);
                REQUIRE(new_state.lower.phase == counter::phase_t::output);
                REQUIRE(new_state.upper.phase == counter::phase_t::output);
            }

            AND_WHEN("output is called") {
                const auto y = counter::output(new_state);

                THEN("output interval is [7, 10]") {
                    REQUIRE(y.lower == 7);
                    REQUIRE(y.upper == 10);
                }
            }
        }
    }
}

SCENARIO("IA-DEVS counter internal transition (reset after output)", "[iadevs][counter]") {
    GIVEN("a counter in output phase with count=5") {
        const auto state = output_state(5);

        WHEN("internal_transition is applied") {
            const auto new_state = counter::internal_transition(state);

            THEN("count resets to 0 and phase becomes passive") {
                REQUIRE(new_state.lower.count == 0);
                REQUIRE(new_state.upper.count == 0);
                REQUIRE(new_state.lower.phase == counter::phase_t::passive);
                REQUIRE(new_state.upper.phase == counter::phase_t::passive);
            }
        }
    }

    GIVEN("a counter in output phase with interval count [2, 8]") {
    counter::state_t lower_state{counter::phase_t::output, 2};
    counter::state_t upper_state{counter::phase_t::output, 8};
        const auto state = counter::state_i_t::closed(lower_state, upper_state);

        WHEN("internal_transition is applied") {
            const auto new_state = counter::internal_transition(state);

            THEN("count resets to [0,0] and phase becomes passive") {
                REQUIRE(new_state.lower.count == 0);
                REQUIRE(new_state.upper.count == 0);
                REQUIRE(new_state.lower.phase == counter::phase_t::passive);
                REQUIRE(new_state.upper.phase == counter::phase_t::passive);
            }
        }
    }

    GIVEN("a counter in passive phase (invalid for internal transition)") {
        const auto state = passive_state(3);

        THEN("internal_transition throws std::invalid_argument") {
            REQUIRE_THROWS_AS(counter::internal_transition(state), std::invalid_argument);
        }
    }
}

SCENARIO("IA-DEVS counter time_advance semantics", "[iadevs][counter]") {
    GIVEN("a counter in passive phase") {
        const auto state = passive_state(10);

        WHEN("time_advance is computed") {
            const auto ta = counter::time_advance(state);

            THEN("TA is empty (represents infinite/passive)") {
                REQUIRE(ta.is_empty());
            }
        }
    }

    GIVEN("a counter in output phase") {
        const auto state = output_state(5);

        WHEN("time_advance is computed") {
            const auto ta = counter::time_advance(state);

            THEN("TA is [0, 0] (immediate)") {
                REQUIRE_FALSE(ta.is_empty());
                REQUIRE(ta.lower == counter::dec3{});
                REQUIRE(ta.upper == counter::dec3{});
            }
        }
    }

    GIVEN("an empty state interval") {
        const auto state = counter::state_i_t::empty_interval();

        WHEN("time_advance is computed") {
            const auto ta = counter::time_advance(state);

            THEN("TA is also empty") {
                REQUIRE(ta.is_empty());
            }
        }
    }
}

SCENARIO("IA-DEVS counter output function validation", "[iadevs][counter]") {
    GIVEN("a counter in output phase with count=42") {
        const auto state = output_state(42);

        WHEN("output is called") {
            const auto y = counter::output(state);

            THEN("output is [42, 42]") {
                REQUIRE(y.lower == 42);
                REQUIRE(y.upper == 42);
            }
        }
    }

    GIVEN("a counter in passive phase") {
        const auto state = passive_state(7);

        THEN("output throws std::invalid_argument") {
            REQUIRE_THROWS_AS(counter::output(state), std::invalid_argument);
        }
    }
}

SCENARIO("IA-DEVS counter complete simulation sequence", "[iadevs][counter]") {
    GIVEN("initial state (0, passive)") {
        auto state = passive_state(0);
        const auto e = elapsed(0, 0);

        WHEN("sequence: Add, Add, Add, Reset, Internal") {
            // Step 1: Add -> (1, passive)
            state = counter::external_transition(state, e, add_event());
            REQUIRE(state.lower.count == 1);
            REQUIRE(state.lower.phase == counter::phase_t::passive);

            // Step 2: Add -> (2, passive)
            state = counter::external_transition(state, e, add_event());
            REQUIRE(state.lower.count == 2);
            REQUIRE(state.lower.phase == counter::phase_t::passive);

            // Step 3: Add -> (3, passive)
            state = counter::external_transition(state, e, add_event());
            REQUIRE(state.lower.count == 3);
            REQUIRE(state.lower.phase == counter::phase_t::passive);

            // Step 4: Reset -> (3, output)
            state = counter::external_transition(state, e, reset_event());
            REQUIRE(state.lower.count == 3);
            REQUIRE(state.lower.phase == counter::phase_t::output);

            // Verify TA is [0, 0]
            auto ta = counter::time_advance(state);
            REQUIRE_FALSE(ta.is_empty());
            REQUIRE(ta.lower == counter::dec3{});

            // Verify output is 3
            auto y = counter::output(state);
            REQUIRE(y.lower == 3);

            // Step 5: Internal -> (0, passive)
            state = counter::internal_transition(state);
            REQUIRE(state.lower.count == 0);
            REQUIRE(state.lower.phase == counter::phase_t::passive);

            THEN("the counter has reset to initial state") {
                REQUIRE(state.lower.count == 0);
                REQUIRE(state.upper.count == 0);
                REQUIRE(state.lower.phase == counter::phase_t::passive);
            }
        }
    }
}

SCENARIO("IA-DEVS counter error handling", "[iadevs][counter]") {
    GIVEN("empty state interval") {
        const auto state = counter::state_i_t::empty_interval();
        const auto e = elapsed(0, 0);

        THEN("external_transition throws") {
            REQUIRE_THROWS_AS(counter::external_transition(state, e, add_event()), 
                              std::invalid_argument);
        }

        THEN("internal_transition throws") {
            REQUIRE_THROWS_AS(counter::internal_transition(state), std::invalid_argument);
        }

        THEN("output throws") {
            REQUIRE_THROWS_AS(counter::output(state), std::invalid_argument);
        }
    }

    GIVEN("empty input interval") {
        const auto state = passive_state(5);
        const auto e = elapsed(0, 0);
        const auto empty_input = counter::input_i_t::empty_interval();

        THEN("external_transition throws") {
            REQUIRE_THROWS_AS(counter::external_transition(state, e, empty_input), 
                              std::invalid_argument);
        }
    }
}
