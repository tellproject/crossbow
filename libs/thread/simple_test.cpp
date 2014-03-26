#include "thread/thread.hpp"

#include <thread>

#define ctest_assert(x) if (!(x)) return 1;

int main()
{
    int res = 0;
    crossbow::thread t([&res](){ res = 1; });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    t.join();
    ctest_assert(res == 1);
    return 0;
}
