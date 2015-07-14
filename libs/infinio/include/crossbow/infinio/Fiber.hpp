#pragma once

#include <crossbow/non_copyable.hpp>

#include <boost/context/fcontext.hpp>
#include <boost/version.hpp>

#include <cstddef>
#include <functional>
#include <new>
#include <utility>

namespace crossbow {
namespace infinio {

class EventProcessor;

class Fiber final : crossbow::non_copyable, crossbow::non_movable {
public:
    static constexpr size_t STACK_SIZE = 0x800000;

    template <typename... Args>
    static Fiber* create(Args&&... args) {
        return new Fiber(std::forward<Args>(args)...);
    }

    void* operator new(size_t size);

    void* operator new[](size_t size) = delete;

    void operator delete(void* ptr);

    void operator delete[](void* ptr) = delete;

    void yield();

    void wait();

    void resume();

private:
    static void entry(intptr_t ptr);

    Fiber(EventProcessor& processor, std::function<void(Fiber&)> fun);

    void start();

    EventProcessor& mProcessor;

    std::function<void(Fiber&)> mFun;

#if BOOST_VERSION >= 105600
    boost::context::fcontext_t mContext;
#else
    boost::context::fcontext_t* mContext;
#endif

    boost::context::fcontext_t mReturnContext;
};

} // namespace infinio
} // namespace crossbow
