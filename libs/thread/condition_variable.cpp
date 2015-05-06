#include "thread/condition_variable.hpp"
#include "common.hpp"
#include <iostream>

namespace crossbow {
using namespace impl;

condition_variable::condition_variable() {}

condition_variable::~condition_variable() {
    if (!mQueue.empty()) {
        std::cerr << "Trying to destruct a condition_variable where events are waiting\n";
        std::terminate();
    }
}

void condition_variable::wait(std::unique_lock<crossbow::mutex>& lock)
{
    std::unique_lock<crossbow::busy_mutex> _(mMutex);
    if (mLastNotifyWasEmpty) {
        mLastNotifyWasEmpty = false;
        return;
    }
    lock.unlock();
    auto myThread = this_processor().currThread;
    myThread->state_ = thread_impl::state::BLOCKED;
    mQueue.push(std::make_pair(&this_processor(), myThread));
    _.unlock();
    doBlock();
    lock.lock();
}

void condition_variable::notify_one() {
    std::unique_lock<crossbow::busy_mutex> _(mMutex);
    if (mQueue.empty()) {
        mLastNotifyWasEmpty = true;
        return;
    }
    auto n = mQueue.front();
    mQueue.pop();
    n.second->state_ = thread_impl::state::READY;
    while (!n.first->queue.push(n.second));
}

void condition_variable::notify_all() {
    std::unique_lock<crossbow::busy_mutex> _(mMutex);
    if (mQueue.empty()) {
        mLastNotifyWasEmpty = true;
    }
    while (!mQueue.empty()) {
        auto n = mQueue.front();
        mQueue.pop();
        n.second->state_ = thread_impl::state::READY;
        while (!n.first->queue.push(n.second));
    }
}

}

