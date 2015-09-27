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
