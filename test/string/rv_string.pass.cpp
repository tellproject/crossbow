//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// basic_string<charT,traits,Allocator>&
//   assign(basic_string<charT,traits>&& str);

#include <string>
#include <cassert>

#include "min_allocator.h"
#include <crossbow/string.hpp>
using namespace crossbow;

template <class S>
void
test(S s, S str, S expected)
{
    s.assign(std::move(str));
    assert(s.__invariants());
    assert(s == expected);
}

int main()
{
    {
    typedef string S;
    test(S(), S(), S());
    test(S(), S("12345"), S("12345"));
    test(S(), S("1234567890"), S("1234567890"));
    test(S(), S("12345678901234567890"), S("12345678901234567890"));

    test(S("12345"), S(), S());
    test(S("12345"), S("12345"), S("12345"));
    test(S("12345"), S("1234567890"), S("1234567890"));
    test(S("12345"), S("12345678901234567890"), S("12345678901234567890"));

    test(S("1234567890"), S(), S());
    test(S("1234567890"), S("12345"), S("12345"));
    test(S("1234567890"), S("1234567890"), S("1234567890"));
    test(S("1234567890"), S("12345678901234567890"), S("12345678901234567890"));

    test(S("12345678901234567890"), S(), S());
    test(S("12345678901234567890"), S("12345"), S("12345"));
    test(S("12345678901234567890"), S("1234567890"), S("1234567890"));
    test(S("12345678901234567890"), S("12345678901234567890"),
         S("12345678901234567890"));
    }
#if __cplusplus >= 201103L
    {
    typedef basic_string<char, std::char_traits<char>, min_allocator<char>> S;
    test(S(), S(), S());
    test(S(), S("12345"), S("12345"));
    test(S(), S("1234567890"), S("1234567890"));
    test(S(), S("12345678901234567890"), S("12345678901234567890"));

    test(S("12345"), S(), S());
    test(S("12345"), S("12345"), S("12345"));
    test(S("12345"), S("1234567890"), S("1234567890"));
    test(S("12345"), S("12345678901234567890"), S("12345678901234567890"));

    test(S("1234567890"), S(), S());
    test(S("1234567890"), S("12345"), S("12345"));
    test(S("1234567890"), S("1234567890"), S("1234567890"));
    test(S("1234567890"), S("12345678901234567890"), S("12345678901234567890"));

    test(S("12345678901234567890"), S(), S());
    test(S("12345678901234567890"), S("12345"), S("12345"));
    test(S("12345678901234567890"), S("1234567890"), S("1234567890"));
    test(S("12345678901234567890"), S("12345678901234567890"),
         S("12345678901234567890"));
    }
#endif
}
