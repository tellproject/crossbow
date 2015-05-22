#pragma once

#include <crossbow/infinio/EventDispatcher.hpp>
#include <crossbow/infinio/InfinibandLimits.hpp>

#include <atomic>
#include <memory>
#include <system_error>
#include <thread>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {

class CompletionContext;
class DeviceContext;

/**
 * @brief Class handling all Infiniband related management tasks
 *
 * Provides the interface to the RDMA Connection Manager and dispatches any received events to the associated sockets.
 * A background thread is started in order to poll the event channel but all actions on the associated sockets are
 * dispatched through the EventDispatcher.
 */
class InfinibandService {
public:
    InfinibandService(EventDispatcher& dispatcher)
            : InfinibandService(dispatcher, InfinibandLimits()) {
    }

    InfinibandService(EventDispatcher& dispatcher, const InfinibandLimits& limits);

    ~InfinibandService();

    /**
     * @brief Shutsdown the Infiniband service
     */
    void shutdown(std::error_code& ec);

private:
    friend class InfinibandAcceptor;
    friend class InfinibandSocket;

    struct rdma_event_channel* channel() {
        return mChannel;
    }

    /**
     * @brief Gets the completion context associated with the ibv_context
     */
    CompletionContext* context();

    /**
     * @brief Process the event received from the RDMA event channel
     */
    void processEvent(struct rdma_cm_event* event);

    /// Dispatcher to execute all actions on
    EventDispatcher& mDispatcher;

    /// Configurable limits for the Infiniband devices
    InfinibandLimits mLimits;

    /// Thread polling the RDMA event channel for events
    std::thread mPollingThread;

    /// RDMA event channel for handling all connection events
    struct rdma_event_channel* mChannel;

    /// The context associated with the Infiniband device
    /// This might be null because the device only gets initialized after a connection is bound to it
    std::unique_ptr<DeviceContext> mDevice;

    /// True if this service is in the process of shutting down
    std::atomic<bool> mShutdown;
};

} // namespace infinio
} // namespace crossbow
