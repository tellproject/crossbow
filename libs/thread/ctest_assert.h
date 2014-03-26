#pragma once
#include <stdlib.h>

inline void ctest_assert(bool exp)
{
    if (!exp) abort();
}
