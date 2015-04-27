#pragma once

#include <mutex>
#include <memory>

#include <boost/asio/io_service.hpp>

namespace crossbow {
namespace infinio {

/**
 * @brief The EventDispatcher maintains an event loop and dispatches functions for later execution on it
 *
 * This implementation is only a wrapper around the Boost Asio IO Service.
 */
class EventDispatcher {
public:
    void run() {
        {
            std::lock_guard<std::mutex> g(mWorkMutex);
            if (!mWork) {
                mWork.reset(new boost::asio::io_service::work(mService));
            }
        }
        mService.run();
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
    boost::asio::io_service mService;

    std::mutex mWorkMutex;
    std::unique_ptr<boost::asio::io_service::work> mWork;
};

} // namespace infinio
} // namespace crossbow
