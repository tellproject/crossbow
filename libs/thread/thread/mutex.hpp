#pragma once
#include <atomic>
#include <boost/lockfree/queue.hpp>

namespace crossbow {

namespace impl {
class thread_impl;
} // namespace impl

class busy_mutex {
    friend class mutex;
    std::atomic<bool> _m;
public:
    busy_mutex()
        : _m(true)
    {}

    busy_mutex(const busy_mutex &) = delete;

    busy_mutex &operator= (const busy_mutex &) = delete;
public:
    inline void lock() {
        bool val = true;
        while (true) {
            if (_m.compare_exchange_strong(val, false))
                return;
        }
    }

    inline bool try_lock() {
        bool val = true;
        return _m.compare_exchange_strong(val, false);
    }

    inline void unlock() {
        _m.store(true);
    }
};

class mutex {
    boost::lockfree::queue<impl::thread_impl*, boost::lockfree::fixed_sized<false>> queue;
    std::atomic<int> mLock;
public:
    mutex();
    void lock();
    bool try_lock();
    void unlock();
};

}
