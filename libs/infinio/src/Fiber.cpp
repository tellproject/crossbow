#include <crossbow/infinio/Fiber.hpp>

#include <crossbow/infinio/EventProcessor.hpp>
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

Fiber::Fiber(EventProcessor& processor, std::function<void(Fiber&)> fun)
        : mProcessor(processor),
          mFun(std::move(fun)),
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
    mProcessor.execute([this] () {
        resume();
    });
    wait();
}

void Fiber::wait() {
#if BOOST_VERSION >= 105600
    LOG_ASSERT(mReturnContext, "Not waiting from active fiber");
    auto res = boost::context::jump_fcontext(&mContext, mReturnContext, reinterpret_cast<intptr_t>(this));
#else
    auto res = boost::context::jump_fcontext(mContext, &mReturnContext, reinterpret_cast<intptr_t>(this));
#endif
    LOG_ASSERT(res == reinterpret_cast<intptr_t>(this), "Not returning from resume()");
}

void Fiber::resume() {
#if BOOST_VERSION >= 105600
    LOG_ASSERT(!mReturnContext, "Resuming an already active fiber");
    auto res = boost::context::jump_fcontext(&mReturnContext, mContext, reinterpret_cast<intptr_t>(this));
    mReturnContext = nullptr;
#else
    auto res = boost::context::jump_fcontext(&mReturnContext, mContext, reinterpret_cast<intptr_t>(this));
#endif
    LOG_ASSERT(res == reinterpret_cast<intptr_t>(this), "Not returning from wait()");
}

void Fiber::start() {
    try {
        LOG_TRACE("Invoking fiber function");
        mFun(*this);
        LOG_TRACE("Exiting fiber function");
    } catch (std::exception& e) {
        LOG_ERROR("Exception triggered in fiber function [error = %1%]", e.what());
    } catch (...) {
        LOG_ERROR("Exception triggered in fiber function");
    }

    // Enqueue deletion of Fiber object
    auto self = this;
    mProcessor.execute([self] () {
        delete self;
    });

    // Return to previous context
    wait();
}

} // namespace infinio
} // namespace crossbow
