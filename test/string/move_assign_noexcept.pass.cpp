//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

// <string>

// basic_string& operator=(basic_string&& c)
//     noexcept(
//          allocator_type::propagate_on_container_move_assignment::value &&
//          is_nothrow_move_assignable<allocator_type>::value);

// This tests a conforming extension

#include <string>
#include <cassert>

#include "test_allocator.h"
#include <crossbow/string.hpp>
using namespace crossbow;

template <class T>
struct some_alloc
{
    typedef T value_type;
    some_alloc(const some_alloc&);
};

int main()
{
#if __has_feature(cxx_noexcept)
    {
        typedef string C;
        static_assert(std::is_nothrow_move_assignable<C>::value, "");
    }
#endif
}
