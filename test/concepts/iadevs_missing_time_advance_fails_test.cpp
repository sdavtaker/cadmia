// SPDX-License-Identifier: BSD-2-Clause
/**
 * Compile-time fail test: Model missing time_advance fails concept check
 */

#include <cadmia/concepts/iadevs_atomic_model.hpp>
#include <cadmia/modeling/interval.hpp>

// Invalid model: missing time_advance
class invalid_model_no_time_advance {
  public:
    using time_interval_t = cadmia::modeling::interval<float>;
    using state_t         = cadmia::modeling::interval<float>;
    using input_value_t   = cadmia::modeling::interval<float>;
    using output_value_t  = cadmia::modeling::interval<float>;

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

    [[nodiscard]] static output_value_t output(const state_t &s) {
        return s;
    }

    // Missing time_advance!
};

// This static_assert should fail
static_assert(cadmia::concepts::IADEVSAtomicModel<invalid_model_no_time_advance>,
              "Model should fail: missing time_advance");

int main() {
    return 0;
}
