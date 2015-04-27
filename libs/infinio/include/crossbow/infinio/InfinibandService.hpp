#pragma once

#include <boost/system/error_code.hpp>

#include <atomic>
#include <memory>
#include <thread>

#include <rdma/rdma_cma.h>

#include <sys/socket.h>

namespace crossbow {
namespace infinio {

class DeviceContext;
class EventDispatcher;
class InfinibandBuffer;
class SocketImplementation;

/**
 * @brief Class handling all Infiniband related management tasks
 *
 * Provides the interface to the RDMA Connection Manager and dispatches any received events to the associated sockets.
 * A background thread is started in order to poll the event channel but all actions on the associated sockets are
 * dispatched through the EventDispatcher.
 */
class InfinibandService {
public:
    InfinibandService(EventDispatcher& dispatcher);

    ~InfinibandService();

    /**
     * @brief Shutsdown the Infiniband service
     */
    void shutdown(boost::system::error_code& ec);


    // TODO The following members should only be accessed by the InfinibandSocket

    void open(SocketImplementation* impl, boost::system::error_code& ec);

    void close(SocketImplementation* impl, boost::system::error_code& ec);

    void bind(SocketImplementation* impl, struct sockaddr* addr, boost::system::error_code& ec);

    void listen(SocketImplementation* impl, int backlog, boost::system::error_code& ec);

    void connect(SocketImplementation* impl, struct sockaddr* addr, boost::system::error_code& ec);

    void disconnect(SocketImplementation* impl, boost::system::error_code& ec);

    void send(SocketImplementation* impl, InfinibandBuffer& buffer, boost::system::error_code& ec);

private:
    /**
     * @brief Gets the device context associated with the ibv_context
     *
     * If the device context for the given ibv_context was not yet initialized it will create the context and
     * initialize it. At this time only one device is supported, receiving events from multiple NICs results in an
     * error.
     */
    DeviceContext* getDevice(struct ibv_context* verbs);

    /**
     * @brief Process the event received from the RDMA event channel
     */
    void processEvent(struct rdma_cm_event* event);

    /**
     * @brief The address to the remote host was successfully resolved
     *
     * Continue by resolving the route to the remote host.
     */
    void onAddressResolved(struct rdma_cm_id* id);

    /**
     * @brief Error while resolving address
     *
     * Invoke the onConnected handler to inform the socket that the connection attempt failed.
     */
    void onAddressError(struct rdma_cm_id* id);

    /**
     * @brief The route to the remote host was successfully resolved
     *
     * Setup the socket and establish the connection to the remote host.
     */
    void onRouteResolved(struct rdma_cm_id* id);

    /**
     * @brief Error while resolving route
     *
     * Invoke the onConnected handler to inform the socket that the connection attempt failed.
     */
    void onRouteError(struct rdma_cm_id* id);

    /**
     * @brief The listen socket received a new connection request
     *
     * Invoke the onConnection handler to pass the new connection to the listen socket. Accepts or rejects the
     * connection depending on the return value of the handler.
     */
    void onConnectionRequest(struct rdma_cm_id* listener, struct rdma_cm_id* id);

    /**
     * @brief An error has occured while establishing a connection
     *
     * Invoke the onConnected handler to inform the socket that the connection attempt failed.
     */
    void onConnectionError(struct rdma_cm_id* id);

    /**
     * @brief The remote host was unreachable
     *
     * Invoke the onConnected handler to inform the socket that the connection attempt failed.
     */
    void onUnreachable(struct rdma_cm_id* id);

    /**
     * @brief The outgoing connection has been rejected by the remote host
     *
     * Invoke the onConnected handler to inform the socket it was rejected.
     */
    void onRejected(struct rdma_cm_id* id);

    /**
     * @brief The connection has been established
     *
     * Invoke the onConnected handler to inform the socket it is now connected.
     */
    void onEstablished(struct rdma_cm_id* id);

    /**
     * @brief The remote connection has been disconnected
     *
     * Executed when the remote host triggered a disconnect. Invoke the onDisconnect handler so that the socket can
     * close on the local host.
     */
    void onDisconnected(struct rdma_cm_id* id);

    /**
     * @brief The connection has exited the Timewait state
     *
     * After disconnecting the connection any packets that were still in flight have now been processed by the NIC. Tell
     * the connection to drain the completion queue so it can be disconnected.
     */
    void onTimewaitExit(struct rdma_cm_id* id);

    /// Dispatcher to execute all actions on
    EventDispatcher& mDispatcher;

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
