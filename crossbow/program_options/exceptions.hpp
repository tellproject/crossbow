/*
 * (C) Copyright 2015 ETH Zurich Systems Group (http://www.systems.ethz.ch/) and others.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Contributors:
 *     Markus Pilman <mpilman@inf.ethz.ch>
 *     Simon Loesing <sloesing@inf.ethz.ch>
 *     Thomas Etter <etterth@gmail.com>
 *     Kevin Bocksrocker <kevin.bocksrocker@gmail.com>
 *     Lucas Braun <braunl@inf.ethz.ch>
 */
#pragma once
#include <exception>
#include <string>
#include <crossbow/string.hpp>

namespace crossbow {

namespace program_options {

struct parse_error : public std::runtime_error {
    explicit parse_error(const std::string &what_arg)
        : std::runtime_error("Error while parsing arguments: " + what_arg) {}
};

struct inexpected_value : public parse_error {
    string value;
    explicit inexpected_value(const std::string &value)
        : parse_error("Unexpected value '" + value + '\''), value(value) {}
};

struct argument_not_found : public parse_error {
    string argName;
    explicit argument_not_found(const std::string &arg)
        : parse_error("Unknown argument '" + arg + "'"), argName(arg) {}
};

} // namespace program_options

} // namespace crossbow
