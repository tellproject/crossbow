#pragma once

#include <boost/asio/io_service.hpp>

#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace crossbow {
namespace infinio {

/**
 * @brief Implementation of EventDispatcher using Boost Asio as its underlying event loop
 */
class AsioEventDispatcherImpl {
public:
    AsioEventDispatcherImpl(uint64_t numThreads)
            : mNumThreads(numThreads) {
    }

    void run() {
        {
            std::lock_guard<std::mutex> g(mWorkMutex);
            if (!mWork) {
                mWork.reset(new boost::asio::io_service::work(mService));
            }
        }

        auto execDispatcher = [this] () {
            mService.run();
        };

        for (size_t i = 1; i < mNumThreads; ++i) {
            mThreads.emplace_back(execDispatcher);
        }
        execDispatcher();

        for (auto& t : mThreads) {
            t.join();
        }
    }

    void stop() {
        std::lock_guard<std::mutex> g(mWorkMutex);
        mWork.reset();
    }

    template <typename Fun>
    void post(Fun f) {
        mService.post(f);
    }

private:
    uint64_t mNumThreads;

    boost::asio::io_service mService;

    std::mutex mWorkMutex;
    std::vector<std::thread> mThreads;
    std::unique_ptr<boost::asio::io_service::work> mWork;
};

using EventDispatcher = AsioEventDispatcherImpl;

} // namespace infinio
} // namespace crossbow
