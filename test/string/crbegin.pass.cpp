//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// const_reverse_iterator crbegin() const;

#include <string>
#include <cassert>

#include "min_allocator.h"
#include "string.h"
using namespace awesome;

template <class S>
void
test(const S& s)
{
    typename S::const_reverse_iterator cb = s.crbegin();
    if (!s.empty())
    {
        assert(*cb == s.back());
    }
    assert(cb == s.rbegin());
}

int main()
{
    {
    typedef string S;
    test(S());
    test(S("123"));
    }
#if __cplusplus >= 201103L
    {
    typedef basic_string<char, std::char_traits<char>, min_allocator<char>> S;
    test(S());
    test(S("123"));
    }
#endif
}