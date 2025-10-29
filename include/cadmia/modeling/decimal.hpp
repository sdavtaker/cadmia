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

#pragma once

#include <compare>
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace cadmia::modeling {

    // Simple fixed-point decimal backed by an integer Raw with compile-time scale.
    // Stored raw value represents: value_scaled = value * 10^Scale.
    // Therefore, the real value is raw / 10^Scale (resolution = 10^-Scale).
    template <unsigned int Scale, std::integral Raw = int> struct decimal {
        using raw_type                      = Raw;
        static constexpr unsigned int scale = Scale;

        constexpr decimal() = default;

        // Factory from scaled units (raw integer representing value * 10^Scale)
        static constexpr decimal from_scaled(raw_type raw) {
            return decimal(raw);
        }

        // Factory from whole units (assumes input is in units of 10^0)
        static constexpr decimal from_whole(raw_type whole) {
            raw_type acc = whole;
            for (unsigned int i = 0; i < scale; ++i) {
                acc = static_cast<raw_type>(acc * static_cast<raw_type>(10));
            }
            return decimal(acc);
        }

        // Accessor for the underlying scaled raw value (value * 10^Scale)
        constexpr raw_type raw_value() const noexcept { return raw_; }
        // Basic arithmetic
        friend constexpr decimal operator+(decimal a, decimal b) {
            return decimal::from_scaled(static_cast<raw_type>(a.raw_ + b.raw_));
        }
        friend constexpr decimal operator-(decimal a, decimal b) {
            return decimal::from_scaled(static_cast<raw_type>(a.raw_ - b.raw_));
        }

        // Comparison (total order)
        friend constexpr auto operator<=>(decimal a, decimal b) = default;
        friend constexpr bool operator==(decimal a, decimal b)  = default;

      private:
        raw_type raw_{0};
        explicit constexpr decimal(raw_type raw) : raw_(raw) {}
    };

} // namespace cadmia::modeling
