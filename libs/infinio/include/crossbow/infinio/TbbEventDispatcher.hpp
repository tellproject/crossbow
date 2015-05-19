#pragma once

#include <tbb/task.h>
#include <tbb/task_scheduler_init.h>

namespace crossbow {
namespace infinio {

template <typename Fun>
class TbbTask: public tbb::task {
public:
    TbbTask(Fun fun)
            : mFun(std::move(fun)) {
    }

    virtual task* execute() override {
        mFun();
        return nullptr;
    }

private:
    Fun mFun;
};

/**
 * @brief Implementation of EventDispatcher using TBB as its underlying event loop
 */
class TbbEventDispatcherImpl {
public:
    TbbEventDispatcherImpl(uint64_t numThreads)
            : mInit(numThreads),
              mDummy(nullptr) {
    }

    ~TbbEventDispatcherImpl() {
        tbb::task::destroy(*mDummy);
    }

    void run() {
        mDummy = new (tbb::task::allocate_root()) tbb::empty_task();
        mDummy->set_ref_count(2);
        tbb::task::enqueue(*mDummy);
        mDummy->wait_for_all();
    }

    void stop() {
        mDummy->set_ref_count(1);
    }

    template <typename Fun>
    void post(Fun f) {
        auto t = new (tbb::task::allocate_root()) TbbTask<Fun>(std::move(f));
        tbb::task::enqueue(*t);
    }

private:
    tbb::task_scheduler_init mInit;
    tbb::empty_task* mDummy;
};

using EventDispatcher = TbbEventDispatcherImpl;

} // namespace infinio
} // namespace crossbow
