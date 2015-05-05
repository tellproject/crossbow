#include "thread/thread.hpp"
#include "thread/mutex.hpp"

#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <boost/lockfree/queue.hpp>
#include <boost/context/all.hpp>
#include <boost/version.hpp>
#include <unistd.h>
#include <cassert>
#include <cstdlib>
#include <sys/types.h>
#include <sys/sysctl.h>

#include <config.hpp>

namespace crossbow {
namespace {

inline unsigned int rdtsc() {
    unsigned int lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return lo;
}

unsigned get_num_cpus() {
#if 0
#if (defined(linux) || defined(__linux) || defined(__linux__) || defined(__GNU__) || defined(__GLIBC__)) && !defined(_CRAYC)
#elif defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);

    return sysinfo.dwNumberOfProcessors;
#elif defined(macintosh) || defined(__APPLE__) || defined(__APPLE_CC__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    // BSD-likeint mib[4];
    unsigned numCPU;
    int mib[4];
    size_t len = sizeof(numCPU);

    /* set the mib for hw.ncpu */
    mib[0] = CTL_HW;
    mib[1] = HW_AVAILCPU;  // alternatively, try HW_NCPU;

    /* get the number of CPUs from the system */
    sysctl(mib, 2, &numCPU, &len, NULL, 0);

    if (numCPU < 1) {
        mib[1] = HW_NCPU;
        sysctl(mib, 2, &numCPU, &len, NULL, 0);

        if (numCPU < 1) {
            numCPU = 1;
        }
    }
    return numCPU;
#else
    // fallback
    return std::thread::hardware_concurrency();
#endif
#endif
    return std::thread::hardware_concurrency();
}

struct stack_allocator {
    static boost::lockfree::queue<char*, boost::lockfree::fixed_sized<true>, boost::lockfree::capacity<1024> > stacks;
    static void* alloc() {
        char* res;
        if (!stacks.pop(res))
            res = reinterpret_cast<char*>(std::calloc(STACK_SIZE, sizeof(char)));
        if (!res) throw std::bad_alloc();
        return res + STACK_SIZE;
    }

    static void free(void* stack) {
        char* s = reinterpret_cast<char*>(stack);
        s -= STACK_SIZE;
        if (!stacks.push(s))
            std::free(s);
    }
};

decltype(stack_allocator::stacks) stack_allocator::stacks;

void* alloc_stack() {
    return stack_allocator::alloc();
}

void free_stack(void* vp) {
    return stack_allocator::free(vp);
}

std::atomic<bool> initialized(false);
std::mutex init_mutex;

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
};

processor** processors;
volatile unsigned num_processors;
crossbow::busy_mutex map_mutex;
std::unordered_map<std::thread::id, processor*> this_thread_map;

processor &this_processor() {
    std::lock_guard<decltype(map_mutex)> _(map_mutex);
    return *this_thread_map.at(std::this_thread::get_id());
}

inline void default_init() {
    if (!initialized.load()) {
        std::lock_guard<std::mutex> _(init_mutex);
        if (!initialized.load()) {
            auto numCPU = get_num_cpus();
            num_processors = std::max(numCPU, 1u);
            processors = new processor*[num_processors];
            for (unsigned i = 1; i < num_processors; ++i) {
                processors[i] = new processor();
            }
            processors[0] = new processor(true);
            {
                std::lock_guard<decltype(map_mutex)> _(map_mutex);
                this_thread_map.insert(std::make_pair(std::this_thread::get_id(), processors[num_processors - 1]));
            }
            initialized.store(true);
        }
    }
}

void run_function(intptr_t funptr);

void scheduler(processor &p);
void schedule(processor &p);

} // namspace <anonymous>

namespace impl {

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
    ~thread_impl() {
        free_stack(sp);
    }
};

} // namespace impl

impl::thread_impl* thread::create_impl(std::function<void()> fun) {
    default_init();
    auto timpl = new impl::thread_impl {};
    timpl->sp = alloc_stack();
    timpl->fun = std::move(fun);
    timpl->fc = boost::context::make_fcontext(timpl->sp, STACK_SIZE, run_function);
RETRY:
    auto idx = rdtsc() % num_processors;
    if (processors[idx]->queue.push(timpl)) {
    } else {
        goto RETRY;
    }
    return timpl;
}

thread::thread(thread && other) {
    if (impl_)
        std::terminate();
    impl_ = other.impl_;
    other.impl_ = nullptr;
}

thread::~thread() {
    if (impl_) {
        join();
    }
}

bool thread::joinable() const {
    return impl_ != nullptr;
}

void thread::join() {
    processor &proc = this_processor();
RETRY:
    bool do_run = false;
    bool do_delete = false;
    {
        std::lock_guard<busy_mutex> _(impl_->mutex);
        if (impl_->state_ == impl::thread_impl::state::READY) {
            do_run = true;
            impl_->state_ = impl::thread_impl::state::RUNNING;
        } else if (impl_->state_ == impl::thread_impl::state::FINISHED) {
            do_delete = true;
            goto FINISH;
        }
    }
    if (do_run) {
        boost::context::jump_fcontext(&(impl_->ocontext), impl_->fc, (intptr_t) impl_);
        std::lock_guard<busy_mutex> _(impl_->mutex);
        if (impl_->in_queue) {
            impl_->state_ = impl::thread_impl::state::FINISHED;
            impl_->is_detached = true;
        } else {
            do_delete = true;
        }
    } else {
        schedule(proc);
        goto RETRY;
    }
FINISH:
    if (do_delete) delete impl_;
    impl_ = nullptr;
}

void thread::detach() {
    bool do_delete = false;
    if (impl_) {
        std::lock_guard<busy_mutex> _(impl_->mutex);
        do_delete = impl_->state_ == impl::thread_impl::state::FINISHED;
        impl_->is_detached = true;
    }
    if (do_delete) delete impl_;
    impl_ = nullptr;
}

void mutex::lock() {
    bool val = _m.load();
    for (int i = 0; i < 100; ++i) {
        if (val)
            if (_m.compare_exchange_strong(val, false)) return;
    }
    auto &proc = this_processor();
    while (true) {
        schedule(proc);
        for (int i = 0; i < 100; ++i) {
            if (val)
                if (_m.compare_exchange_strong(val, false)) return;
        }
    }
}

namespace this_thread {

void yield() {
    auto &p = this_processor();
    schedule(p);
}

template<class Rep, class Period>
void sleep_for(const std::chrono::duration<Rep, Period> &duration) {
    auto &p = this_processor();
    auto begin = std::chrono::system_clock::now();
    decltype(begin) end;
    do {
        schedule(p);
        end = std::chrono::system_clock::now();
    } while (begin + duration > end);
}

template<class Clock, class Duration>
void sleep_until(const std::chrono::time_point<Clock, Duration> &sleep_time) {
    auto &p = this_processor();
    auto now = Clock::now();
    while (now < sleep_time) {
        schedule(p);
        now = Clock::now();
    }
}

} // namespace this_thread

namespace {

void run_function(intptr_t funptr) {
    crossbow::impl::thread_impl* timpl = (crossbow::impl::thread_impl*)funptr;
    (timpl->fun)();
    // jump back to scheduler
#if BOOST_VERSION >= 105600
    boost::context::jump_fcontext(&timpl->fc, timpl->ocontext, 0);
#else
    boost::context::jump_fcontext(timpl->fc, &(timpl->ocontext), 0);
#endif
    assert(false); // never returns
}

void scheduler(processor &p) {
    {
        std::lock_guard<decltype(map_mutex)> _(map_mutex);
        this_thread_map.insert(std::make_pair(std::this_thread::get_id(), &p));
    }
    while (true) {
        schedule(p);
    }
}

void schedule(processor &p) {
    processor* proc = &p;
    crossbow::impl::thread_impl* timpl;
    size_t count = 10;
    while (!proc->queue.pop(timpl)) {
        // do work stealing
        proc = processors[rdtsc() % num_processors];
        if (--count) return;
    }
    // schedule this thread
    bool do_run = false;
    {
        std::lock_guard<crossbow::busy_mutex> _(timpl->mutex);
        timpl->in_queue = false;
        if (timpl->state_ == crossbow::impl::thread_impl::state::READY) {
            do_run = true;
            timpl->state_ = crossbow::impl::thread_impl::state::RUNNING;
        }
    }
    if (do_run) boost::context::jump_fcontext(&(timpl->ocontext), timpl->fc, (intptr_t) timpl);
    // delete timpl, if it is detached
    bool do_delete = false;
    {
        std::lock_guard<crossbow::busy_mutex> _(timpl->mutex);
        if (timpl->is_detached) do_delete = true;
        timpl->state_ = crossbow::impl::thread_impl::state::FINISHED;
    }
    if (do_delete) delete timpl;
}

} // namespace anonymous
} // namespace crossbow

