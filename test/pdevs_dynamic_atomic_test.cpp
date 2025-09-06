// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2017-2025, Laouen M. L. Belloli, Damian Vicino
 * Carleton University, Universidad de Buenos Aires
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
#include <cadmia/basic_model/pdevs/accumulator.hpp>
#include <cadmia/modeling/dynamic_atomic.hpp>
#include <cadmia/modeling/dynamic_model_translator.hpp>

#include <type_traits>
#include <memory>

/**
  * This test is for the dynamic atomic class that wraps an atomic model to make it pointer friendly
  */
template<typename TIME>
using int_accumulator = cadmium::basic_models::pdevs::accumulator<int, TIME>;

template<typename TIME>
struct test_custom_acumulator : public cadmium::basic_models::pdevs::accumulator<int, TIME> {
    test_custom_acumulator() = default;
    test_custom_acumulator(int) {}
};

TEST_CASE("create_dynamic_atomic_test", "[pdevs][dynamic][atomic]") {
    int_accumulator<float> model;
    cadmium::dynamic::modeling::atomic<int_accumulator, float> wrapped_model;

    #ifndef RT_ARM_MBED
      static_assert(std::is_same<decltype(model.state), decltype(wrapped_model.state)>::value);
    #endif

    CHECK(model.state == wrapped_model.state);
}

TEST_CASE("create_dynamic_atomic_with_custom_id_and_arguments_test", "[pdevs][dynamic][atomic]") {
    std::shared_ptr<cadmium::dynamic::modeling::atomic_abstract<float>> atomic_model =
        cadmium::dynamic::translate::make_dynamic_atomic_model<test_custom_acumulator, float, int>("id_test", 2);

    // No explicit assertions in original test; construction should not crash
}
