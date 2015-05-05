#pragma once
#include <boost/lockfree/queue.hpp>
#include <boost/context/all.hpp>
#include <thread>
#include "thread/mutex.hpp"

namespace crossbow {
namespace impl {

struct thread_impl;
struct thread_impl {
    enum class state {
        RUNNING,
        READY,
        FINISHED,
        BLOCKED
    };

    void* sp;
    std::function<void()> fun;
    boost::context::fcontext_t ocontext;
#if BOOST_VERSION >= 105600
    boost::context::fcontext_t fc;
#else
    boost::context::fcontext_t* fc;
#endif
    volatile bool is_detached;
    volatile state state_;
    volatile bool in_queue;
    busy_mutex mutex;
    thread_impl()
        : sp(nullptr), fc(nullptr), is_detached(false), state_(state::READY), in_queue(true) {
    }
    thread_impl(const thread_impl &) = delete;
    thread_impl(thread_impl && o)
        : sp(o.sp), fun(std::move(o.fun)), ocontext(std::move(o.ocontext)), fc(o.fc),
          is_detached(o.is_detached) {
        o.sp = nullptr;
        o.fc = nullptr;
        ocontext = boost::context::fcontext_t {};
    }
    ~thread_impl();
};


struct processor;
void scheduler(processor &p);

struct processor {
    boost::lockfree::queue<crossbow::impl::thread_impl*, boost::lockfree::fixed_sized<true>, boost::lockfree::capacity<1024> > queue;
    processor() {
        auto t = std::thread([this]() {
            scheduler(*this);
        });
        t.detach();
    }
    processor(bool main)
    {}
    thread_impl* currThread;
};

extern processor& this_processor();
extern void schedule(processor &p);
} // namespace impl
} // namespace crossbow
