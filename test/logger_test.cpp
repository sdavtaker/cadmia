// SPDX-License-Identifier: BSD-2-Clause
/**
 * Copyright (c) 2013-2025, Damian Vicino
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
#include <sstream>
#include <cadmium/logger/logger.hpp>
#include <cadmium/logger/common_loggers.hpp>

namespace {
    std::ostringstream main_stream;
    std::ostringstream secondary_stream;

    struct main_sink {
        static std::ostream& sink() { return main_stream; }
    };

    struct secondary_sink {
        static std::ostream& sink() { return secondary_stream; }
    };
}

TEST_CASE("Logger does not output for debug log with info logger", "[logger]") {
    main_stream.str("");

    cadmium::logger::logger<
        cadmium::logger::logger_info,
        cadmium::logger::formatter<float>,
        main_sink
    > logger;

    logger.log<cadmium::logger::logger_debug, cadmium::logger::run_info>("should not appear");

    REQUIRE(main_stream.str().empty());
}

TEST_CASE("Logger outputs info log correctly", "[logger]") {
    main_stream.str("");

    cadmium::logger::logger<
        cadmium::logger::logger_info,
        cadmium::logger::formatter<float>,
        main_sink
    > logger;

    logger.log<cadmium::logger::logger_info, cadmium::logger::run_info>("visible message");

    REQUIRE(main_stream.str() == "visible message\n");
}

TEST_CASE("Multiple loggers direct output to separate sinks", "[logger]") {
    main_stream.str("");
    secondary_stream.str("");

    using info_logger = cadmium::logger::logger<
        cadmium::logger::logger_info,
        cadmium::logger::formatter<float>,
        main_sink
    >;
    using debug_logger = cadmium::logger::logger<
        cadmium::logger::logger_debug,
        cadmium::logger::formatter<float>,
        secondary_sink
    >;

    cadmium::logger::multilogger<info_logger, debug_logger> multi_logger;

    multi_logger.log<cadmium::logger::logger_info, cadmium::logger::run_info>("info output");
    multi_logger.log<cadmium::logger::logger_debug, cadmium::logger::run_info>("debug output");

    REQUIRE(main_stream.str() == "info output\n");
    REQUIRE(secondary_stream.str() == "debug output\n");
}
