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
