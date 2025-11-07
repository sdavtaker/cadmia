// SPDX-License-Identifier: BSD-2-Clause
/**
 * Compile-time fail test: Model missing internal_transition fails concept check
 */

#include <cadmia/concepts/iadevs_atomic_model.hpp>
#include <cadmia/modeling/decimal.hpp>
#include <cadmia/modeling/interval.hpp>

// Invalid model: missing internal_transition
class invalid_model_no_internal {
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

    // Missing internal_transition!

    [[nodiscard]] static state_t external_transition(const q_interval_t &q,
                                                     const input_value_t &x) {
        return q.state;
    }

    [[nodiscard]] static output_value_t output(const state_t &s) {
        return s;
    }

    [[nodiscard]] static time_interval_t time_advance(const state_t &s) {
        return s;
    }
};

// This static_assert should fail
static_assert(cadmia::concepts::IADEVSAtomicModel<invalid_model_no_internal>,
              "Model should fail: missing internal_transition");

int main() {
    return 0;
}
