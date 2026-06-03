// SPDX-License-Identifier: BSD-2-Clause
/**
 * Compile-time fail test: Model missing type aliases fails concept check
 */

#include <cadmia/concepts/iadevs_atomic_model.hpp>
#include <cadmia/modeling/interval.hpp>

// Invalid model: missing state_t type alias
class invalid_model_no_state_type {
  public:
    using time_interval_t = cadmia::modeling::interval<float>;
    // Missing state_t!
    using input_value_t  = cadmia::modeling::interval<float>;
    using output_value_t = cadmia::modeling::interval<float>;

    using actual_state = cadmia::modeling::interval<float>;

    struct q_interval {
        actual_state state;
        time_interval_t elapsed;
    };
    using q_interval_t = q_interval;

    [[nodiscard]] static actual_state internal_transition(const actual_state &s) {
        return s;
    }

    [[nodiscard]] static actual_state external_transition(const q_interval_t &q,
                                                          const input_value_t &x) {
        return q.state;
    }

    [[nodiscard]] static output_value_t output(const actual_state &s) {
        return s;
    }

    [[nodiscard]] static time_interval_t time_advance(const actual_state &s) {
        return s;
    }
};

// This static_assert should fail
static_assert(cadmia::concepts::IADEVSAtomicModel<invalid_model_no_state_type>,
              "Model should fail: missing state_t");

int main() {
    return 0;
}
