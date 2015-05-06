#pragma once

#include "mutex.hpp"
#include <queue>
#include <mutex>

namespace crossbow {
namespace impl {
class processor;
}

class condition_variable {
    crossbow::busy_mutex mMutex;
    std::queue<std::pair<impl::processor*, impl::thread_impl*>> mQueue;
    bool mLastNotifyWasEmpty = false;
public:
    condition_variable();
    ~condition_variable();
    condition_variable(const condition_variable&) = delete;
    void wait(std::unique_lock<crossbow::mutex>& lock);
    template<class Predicate>
    void wait(std::unique_lock<crossbow::mutex>& lock, Predicate pred) {
        while (!pred) {
            wait(lock);
        }
    }
    void notify_one();
    void notify_all();
};

} // namespace crossbow
