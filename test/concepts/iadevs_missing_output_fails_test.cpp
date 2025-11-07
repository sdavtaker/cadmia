// SPDX-License-Identifier: BSD-2-Clause
/**
 * Compile-time fail test: Model missing output function fails concept check
 */

#include <cadmia/concepts/iadevs_atomic_model.hpp>
#include <cadmia/modeling/decimal.hpp>
#include <cadmia/modeling/interval.hpp>

// Invalid model: missing output
class invalid_model_no_output {
  public:
    using dec3            = cadmia::modeling::decimal<3>;
    using time_interval_t = cadmia::modeling::interval<dec3>;
    using state_t         = cadmia::modeling::interval<dec3>;
    using input_value_t   = cadmia::modeling::interval<dec3>;
    using output_value_t  = cadmia::modeling::interval<dec3>;

    struct q_interval {
        state_t state;
        time_interval_t elapsed;
    };
    using q_interval_t = q_interval;

    [[nodiscard]] static state_t internal_transition(const state_t &s) {
        return s;
    }

    [[nodiscard]] static state_t external_transition(const q_interval_t &q,
                                                     const input_value_t &x) {
        return q.state;
    }

    // Missing output!

    [[nodiscard]] static time_interval_t time_advance(const state_t &s) {
        return s;
    }
};

// This static_assert should fail
static_assert(cadmia::concepts::IADEVSAtomicModel<invalid_model_no_output>,
              "Model should fail: missing output");

int main() {
    return 0;
}
