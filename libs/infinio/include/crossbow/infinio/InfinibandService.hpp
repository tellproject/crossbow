#pragma once

#include <crossbow/infinio/EventProcessor.hpp>
#include <crossbow/infinio/InfinibandLimits.hpp>
#include <crossbow/infinio/InfinibandSocket.hpp>
#include <crossbow/non_copyable.hpp>

#include <atomic>
#include <memory>
#include <system_error>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {

class CompletionContext;
class DeviceContext;

class InfinibandProcessor : crossbow::non_copyable, crossbow::non_movable {
public:
    InfinibandProcessor(std::shared_ptr<DeviceContext> device, const InfinibandLimits& limits);

    ~InfinibandProcessor();

    void execute(std::function<void()> fun) {
        mTaskQueue.execute(std::move(fun));
    }

    void executeLocal(std::function<void()> fun) {
        mProcessor.execute(std::move(fun));
    }

    void executeFiber(std::function<void(Fiber&)> fun) {
        mTaskQueue.execute([this, fun] () {
            mProcessor.executeFiber(std::move(fun));
        });
    }

    void executeLocalFiber(std::function<void(Fiber&)> fun) {
            mProcessor.executeFiber(std::move(fun));
    }

    CompletionContext* context() {
        return mContext.get();
    }

private:
    /// The event loop processor handling all Infiniband events
    EventProcessor mProcessor;

    /// Task queue to execute functions in the poll thread
    TaskQueue mTaskQueue;

    /// Completion channel polling for Infiniband events
    std::unique_ptr<CompletionContext> mContext;
};

/**
 * @brief Class handling all Infiniband related management tasks
 *
 * Provides the interface to the RDMA Connection Manager and dispatches any received events to the associated sockets.
 */
class InfinibandService : crossbow::non_copyable, crossbow::non_movable {
public:
    InfinibandService()
            : InfinibandService(InfinibandLimits()) {
    }

    InfinibandService(const InfinibandLimits& limits);

    ~InfinibandService();

    const InfinibandLimits& limits() const {
        return mLimits;
    }

    /**
     * @brief Starts polling the RDMA Connection Manager for events
     *
     * The function call blocks while the Infiniband service is active.
     */
    void run();

    /**
     * @brief Shutsdown the Infiniband service
     */
    void shutdown();

    std::unique_ptr<InfinibandProcessor> createProcessor();

    InfinibandAcceptor createAcceptor() {
        return InfinibandAcceptor(new InfinibandAcceptorImpl(mChannel));
    }

    InfinibandSocket createSocket(InfinibandProcessor& processor);

    LocalMemoryRegion registerMemoryRegion(void* data, size_t length, int access);

private:
    friend class InfinibandSocketImpl;

    /**
     * @brief Gets the completion context associated with the ibv_context
     */
    CompletionContext* context(uint64_t num);

    /**
     * @brief Process the event received from the RDMA event channel
     */
    void processEvent(struct rdma_cm_event* event);

    /// Configurable limits for the Infiniband devices
    InfinibandLimits mLimits;

    /// RDMA event channel for handling all connection events
    struct rdma_event_channel* mChannel;

    /// The context associated with the Infiniband device
    std::shared_ptr<DeviceContext> mDevice;

    /// True if this service is in the process of shutting down
    std::atomic<bool> mShutdown;
};

} // namespace infinio
} // namespace crossbow
