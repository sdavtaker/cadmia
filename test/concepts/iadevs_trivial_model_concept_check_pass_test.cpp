// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2025, Damian Vicino
 * Carleton University
 * All rights reserved.
 */

// Compile test: Verify that a trivial IA-DEVS model satisfies the IADEVSAtomicModel concept

#include <cadmia/concepts/iadevs_atomic_model.hpp>
#include <cadmia/modeling/interval.hpp>

// Trivial IA-DEVS atomic model for concept validation
struct trivial_iadevs_model {
    // Base types (required by concept)
    using state_t  = int;
    using time_t   = double;
    using input_t  = int;
    using output_t = int;

    // Interval aliases for convenience (not required by concept)
    using state_i_t  = cadmia::modeling::interval<state_t>;
    using time_i_t   = cadmia::modeling::interval<time_t>;
    using input_i_t  = cadmia::modeling::interval<input_t>;
    using output_i_t = cadmia::modeling::interval<output_t>;

    // IA-DEVS functions (work with intervals, but concept only checks existence)
    static state_i_t internal_transition(const state_i_t &s) {
        return s; // identity
    }

    static state_i_t external_transition(const state_i_t &s, const time_i_t &, const input_i_t &) {
        return s; // ignore input
    }

    static output_i_t output(const state_i_t &) {
        return output_i_t::closed(0, 0);
    }

    static time_i_t time_advance(const state_i_t &) {
        return time_i_t::closed(1.0, 1.0); // constant time advance
    }
};

// Compile-time assertion: trivial model satisfies the concept
static_assert(cadmia::IADEVSAtomicModel<trivial_iadevs_model>,
              "trivial_iadevs_model must satisfy IADEVSAtomicModel concept");

int main() {
    return 0;
}
