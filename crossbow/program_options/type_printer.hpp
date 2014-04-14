#pragma once
#include <iostream>

namespace crossbow {

namespace program_options
{

template<class T>
struct type_printer
{
    unsigned length() const {
        return 7;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "Unknown";
    }
};

template<>
struct type_printer<bool>
{
    unsigned length() const {
        return 0;
    }
    
    std::ostream& operator() (std::ostream& os) const {
        return os;
    }
};

template<>
struct type_printer<char>
{
    unsigned length() const {
        return 9;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "character";
    }
};

template<>
struct type_printer<unsigned char>
{
    unsigned length() const {
        return 13;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "unsigned char";
    }
};

template<>
struct type_printer<short>
{
    unsigned length() const {
        return 5;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "short";
    }
};

template<>
struct type_printer<unsigned short>
{
    unsigned length() const {
        return 14;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "unsigned short";
    }
};

template<>
struct type_printer<int>
{
    unsigned length() const {
        return 4;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "int";
    }
};

template<>
struct type_printer<unsigned>
{
    unsigned length() const {
        return 8;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "unsigned";
    }
};

template<>
struct type_printer<long>
{
    unsigned length() const {
        return 4;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "long";
    }
};

template<>
struct type_printer<unsigned long>
{
    unsigned length() const {
        return 13;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "unsigned long";
    }
};

template<>
struct type_printer<long long>
{
    unsigned length() const {
        return 9;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "long long";
    }
};

template<>
struct type_printer<unsigned long long>
{
    unsigned length() const {
        return 18;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "unsigned long long";
    }
};

template<>
struct type_printer<double>
{
    unsigned length() const {
        return 6;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "double";
    }
};

template<>
struct type_printer<long double>
{
    unsigned length() const {
        return 11;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "long double";
    }
};

template<>
struct type_printer<std::string>
{
    unsigned length() const {
        return 6;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "string";
    }
};

template<>
struct type_printer<string>
{
    unsigned length() const {
        return 6;
    }

    std::ostream& operator() (std::ostream& os) const {
        return os << "string";
    }
};

} // namespace program_options

} // namespace crossbow
