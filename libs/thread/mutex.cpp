#include "thread/thread.hpp"
#include "thread/mutex.hpp"
#include "common.hpp"

namespace crossbow {

using namespace impl;

mutex::mutex()
    : mLock(0)
{
}

void mutex::lock() {
    while (true) {
        int n = 0;
        if (mLock.compare_exchange_strong(n, 1)) return;
        if (!mLock.compare_exchange_strong(n, n+1)) continue;
        auto& p = this_processor();
        p.currThread->state_ = thread_impl::state::BLOCKED;
        while (!queue.push(p.currThread));
        mLock.fetch_sub(1);
        schedule(p);
    }
}

bool mutex::try_lock()
{
    int n = 0;
    return mLock.compare_exchange_strong(n, 1);
}

void mutex::unlock()
{
    while (true) {
        int n = 1;
        if (mLock.compare_exchange_strong(n, 0)) {
            thread_impl* t;
            if (queue.pop(t)) {
                t->state_ = thread_impl::state::READY;
                auto& p = this_processor();
                while (!p.queue.push(t));
            }
        }
    }
}

} // namespace crossbow
