/*
 * (C) Copyright 2015 ETH Zurich Systems Group (http://www.systems.ethz.ch/) and others.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Contributors:
 *     Markus Pilman <mpilman@inf.ethz.ch>
 *     Simon Loesing <sloesing@inf.ethz.ch>
 *     Thomas Etter <etterth@gmail.com>
 *     Kevin Bocksrocker <kevin.bocksrocker@gmail.com>
 *     Lucas Braun <braunl@inf.ethz.ch>
 */
#include <crossbow/infinio/Fiber.hpp>

#include <crossbow/infinio/InfinibandService.hpp>
#include <crossbow/logger.hpp>

#include <cstdlib>

namespace crossbow {
namespace infinio {

namespace {

void* stackFromFiber(Fiber* fiber) {
    return reinterpret_cast<char*>(fiber) + sizeof(Fiber) + Fiber::STACK_SIZE;
}

} // anonymous namespace

void Fiber::entry(intptr_t ptr) {
    auto fiber = reinterpret_cast<Fiber*>(ptr);
    fiber->start();
    LOG_ASSERT(false, "Fiber start function returned");
}

Fiber::Fiber(InfinibandProcessor& processor)
        : mProcessor(processor),
          mContext(boost::context::make_fcontext(stackFromFiber(this), STACK_SIZE, &Fiber::entry)),
#if BOOST_VERSION >= 105600
          mReturnContext(nullptr) {
#else
          mReturnContext() {
#endif
}

void* Fiber::operator new(size_t size) {
    LOG_ASSERT(size == sizeof(Fiber), "Requested size does not match Fiber size");
    return ::malloc(size + Fiber::STACK_SIZE);
}

void Fiber::operator delete(void* ptr) {
    ::free(ptr);
}

void Fiber::yield() {
    // Enqueue resume of Fiber object
    mProcessor.executeLocal([this] () {
        resume();
    });
    wait();
}

void Fiber::wait() {
#if BOOST_VERSION >= 105600
    LOG_ASSERT(mReturnContext, "Not waiting from active fiber");
    __attribute__((unused)) auto res = boost::context::jump_fcontext(&mContext, mReturnContext,
            reinterpret_cast<intptr_t>(this));
#else
    __attribute__((unused)) auto res = boost::context::jump_fcontext(mContext, &mReturnContext,
            reinterpret_cast<intptr_t>(this));
#endif
    LOG_ASSERT(res == reinterpret_cast<intptr_t>(this), "Not returning from resume()");
}

void Fiber::resume() {
#if BOOST_VERSION >= 105600
    LOG_ASSERT(!mReturnContext, "Resuming an already active fiber");
    __attribute__((unused)) auto res = boost::context::jump_fcontext(&mReturnContext, mContext,
            reinterpret_cast<intptr_t>(this));
    mReturnContext = nullptr;
#else
    __attribute__((unused)) auto res = boost::context::jump_fcontext(&mReturnContext, mContext,
            reinterpret_cast<intptr_t>(this));
#endif
    LOG_ASSERT(res == reinterpret_cast<intptr_t>(this), "Not returning from wait()");
}

void Fiber::unblock() {
    mProcessor.execute([this] () {
        resume();
    });
}

void Fiber::execute(std::function<void (Fiber&)> fun) {
    LOG_ASSERT(!mFun, "Fiber function is still valid");

    mFun = std::move(fun);
    resume();
}

void Fiber::start() {
    while (true) {
        try {
            LOG_TRACE("Invoking fiber function");
            mFun(*this);
            LOG_TRACE("Exiting fiber function");
        } catch (std::exception& e) {
            LOG_ERROR("Exception triggered in fiber function [error = %1%]", e.what());
        } catch (...) {
            LOG_ERROR("Exception triggered in fiber function");
        }
        mFun = std::function<void(Fiber&)>();

        mProcessor.recycleFiber(this);

        // Return to previous context
        wait();
    }
}

ConditionVariable::~ConditionVariable() {
    notify_all();
}

void ConditionVariable::wait(Fiber& fiber) {
    mWaiting.push(&fiber);
    fiber.wait();
}

void ConditionVariable::notify_one() {
    if (mWaiting.empty()) {
        return;
    }

    auto fiber = mWaiting.front();
    mWaiting.pop();
    fiber->resume();
}

void ConditionVariable::notify_all() {
    if (mWaiting.empty()) {
        return;
    }

    decltype(mWaiting) waiting;
    waiting.swap(mWaiting);
    do {
        waiting.front()->resume();
        waiting.pop();
    } while (!waiting.empty());
}

} // namespace infinio
} // namespace crossbow
