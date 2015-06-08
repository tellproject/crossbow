#pragma once

#include <crossbow/infinio/InfinibandLimits.hpp>
#include <crossbow/infinio/InfinibandSocket.hpp>

#include <atomic>
#include <memory>
#include <system_error>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {

class CompletionContext;
class DeviceContext;

/**
 * @brief Class handling all Infiniband related management tasks
 *
 * Provides the interface to the RDMA Connection Manager and dispatches any received events to the associated sockets.
 */
class InfinibandService {
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

    InfinibandAcceptor createAcceptor() {
        return InfinibandAcceptor(new InfinibandAcceptorImpl(mChannel));
    }

    InfinibandSocket createSocket(uint64_t thread = 0);

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
    std::unique_ptr<DeviceContext> mDevice;

    /// True if this service is in the process of shutting down
    std::atomic<bool> mShutdown;
};

} // namespace infinio
} // namespace crossbow
