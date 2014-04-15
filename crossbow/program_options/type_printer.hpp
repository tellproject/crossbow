#pragma once
#include <iostream>

namespace crossbow {

namespace program_options {

template<class T>
struct type_printer {
    std::ostream &operator()(std::ostream &os) const {
        return os << "Unknown";
    }
};

template<>
struct type_printer<bool> {
    std::ostream &operator()(std::ostream &os) const {
        return os;
    }
};

template<>
struct type_printer<char> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "character";
    }
};

template<>
struct type_printer<unsigned char> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "unsigned char";
    }
};

template<>
struct type_printer<short> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "short";
    }
};

template<>
struct type_printer<unsigned short> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "unsigned short";
    }
};

template<>
struct type_printer<int> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "int";
    }
};

template<>
struct type_printer<unsigned> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "unsigned";
    }
};

template<>
struct type_printer<long> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "long";
    }
};

template<>
struct type_printer<unsigned long> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "unsigned long";
    }
};

template<>
struct type_printer<long long> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "long long";
    }
};

template<>
struct type_printer<unsigned long long> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "unsigned long long";
    }
};

template<>
struct type_printer<double> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "double";
    }
};

template<>
struct type_printer<long double> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "long double";
    }
};

template<>
struct type_printer<std::string> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "string";
    }
};

template<>
struct type_printer<string> {
    std::ostream &operator()(std::ostream &os) const {
        return os << "string";
    }
};

} // namespace program_options

} // namespace crossbow
