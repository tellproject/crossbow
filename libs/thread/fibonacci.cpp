#include "thread/thread.hpp"
#include "ctest_assert.h"

#include <iostream>
#include <thread>
#include <chrono>

template<size_t N>
struct fib
{
    constexpr static size_t value = fib<N-1>::value + fib<N-2>::value;
};

template<>
struct fib<2>
{
    constexpr static size_t value = 1;
};

template<>
struct fib<1>
{
    constexpr static size_t value = 1;
};

template<typename Thread>
size_t fibo(size_t n)
{
    if (n <= 2) return 1;
    size_t n1, n2;
    Thread t1([&n1, n]{ n1 = fibo<Thread>(n - 1); });
    Thread t2([&n2, n]{ n2 = fibo<Thread>(n - 2); });
    t1.join();
    t2.join();
    return n1 + n2;
}

template<class Thread, size_t... N>
struct test;

template<class Thread, size_t First, size_t... Rest>
struct test<Thread, First, Rest...>
{
    static void run()
    {
        ctest_assert(fibo<Thread>(First) == fib<First>::value);
        test<Thread, Rest...>::run();
    }
};

template<class Thread>
struct test<Thread>
{
    static void run()
    {
    }
};

constexpr size_t num_measure = 10;

int main()
{
    auto begin = std::chrono::system_clock::now();
    test<std::thread, num_measure>::run();
    auto end = std::chrono::system_clock::now();
    auto stdthread_time = end - begin;
    begin = std::chrono::system_clock::now();
    test<crossbow::thread, num_measure>::run();
    end = std::chrono::system_clock::now();
    auto crossbow_time = end - begin;
    std::cout << "With crossbow, test took " << std::chrono::duration_cast<std::chrono::milliseconds>(crossbow_time).count() << std::endl;
    std::cout << "With std::thread, test took " << std::chrono::duration_cast<std::chrono::milliseconds>(stdthread_time).count() << std::endl;
    test<crossbow::thread, 1, 2, 10, 18>::run();
}
