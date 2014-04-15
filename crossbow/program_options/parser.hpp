#pragma once
#include <string>

namespace crossbow {

namespace program_options {

template<class T>
struct parser;

template<>
struct parser<string> {
    string operator()(const char* str) const {
        return str;
    }
};

template<>
struct parser<char> {
    char operator()(const char* str) const {
        if (str[0] == '\0' || str[1] != '\0') {
            throw parse_error("Could not parse char: " + std::string(str));
        }
        return str[0];
    }
};

template<>
struct parser<int> {
    int operator()(const char* str) const {
        try {
            return std::stoi(std::string(str));
        } catch (...) {
            throw parse_error(str + std::string("is not a valid int"));
        }
    }
};

template<>
struct parser<long> {
    long operator()(const char* str) const {
        try {
            return std::stol(std::string(str));
        } catch (...) {
            throw parse_error(str + std::string("is not a valid long"));
        }
    }
};

template<>
struct parser<long long> {
    long long operator()(const char* str) const {
        try {
            return std::stoll(std::string(str));
        } catch (...) {
            throw parse_error(str + std::string("is not a valid long long"));
        }
    }
};

template<>
struct parser<unsigned> {
    unsigned operator()(const char* str) const {
        try {
            return std::stoul(std::string(str));
        } catch (...) {
            throw parse_error(str + std::string("is not a valid unsigned"));
        }
    }
};

template<>
struct parser<unsigned long> {
    unsigned long operator()(const char* str) const {
        try {
            return std::stoul(std::string(str));
        } catch (...) {
            throw parse_error(str + std::string("is not a valid unsigned long"));
        }
    }
};

template<>
struct parser<unsigned long long> {
    unsigned long long operator()(const char* str) const {
        try {
            return std::stoull(std::string(str));
        } catch (...) {
            throw parse_error(str + std::string("is not a valid unsigned long long"));
        }
    }
};

template<>
struct parser<float> {
    float operator()(const char* str) const {
        try {
            return std::stof(std::string(str));
        } catch (...) {
            throw parse_error(str + std::string("is not a valid float"));
        }
    }
};

template<>
struct parser<double> {
    double operator()(const char* str) const {
        try {
            return std::stod(std::string(str));
        } catch (...) {
            throw parse_error(str + std::string("is not a valid double"));
        }
    }
};

template<>
struct parser<long double> {
    long double operator()(const char* str) const {
        try {
            return std::stold(std::string(str));
        } catch (...) {
            throw parse_error(str + std::string("is not a valid long double"));
        }
    }
};

} // namespace program_options

} // namespace crossbow
