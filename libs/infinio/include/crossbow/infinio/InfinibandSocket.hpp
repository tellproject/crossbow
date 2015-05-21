#pragma once

#include <crossbow/infinio/Endpoint.hpp>
#include <crossbow/infinio/ErrorCode.hpp>
#include <crossbow/infinio/InfinibandBuffer.hpp>
#include <crossbow/infinio/InfinibandService.hpp>

#include <cstdint>
#include <system_error>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {

class DeviceContext;
class InfinibandSocket;

/**
 * @brief Base class containing common functionality shared between InfinibandAcceptor and InfinibandSocket
 */
class InfinibandBaseSocket {
public:
    // TODO Owner of the SocketImplementation pointer is unclear (i.e. never gets deleted)

    void open(std::error_code& ec);

    bool isOpen() const {
        return (mId != nullptr);
    }

    void close(std::error_code& ec);

    void bind(Endpoint& addr, std::error_code& ec);

protected:
    InfinibandBaseSocket(struct rdma_event_channel* channel, void* context)
            : mChannel(channel),
              mId(nullptr),
              mContext(context) {
    }

    InfinibandBaseSocket(struct rdma_cm_id* id, void* context)
            : mChannel(id->channel),
              mId(id),
              mContext(context) {
        // TODO Assert that mId->context was null
        mId->context = mContext;
    }

    /// The RDMA channel the connection is associated with
    struct rdma_event_channel* mChannel;

    /// The RDMA ID of this connection
    struct rdma_cm_id* mId;

    /// The context pointer associated with the ID
    void* mContext;
};

class ConnectionRequest {
public:
    ConnectionRequest(ConnectionRequest&&) = default;
    ConnectionRequest& operator=(ConnectionRequest&&) = default;

    ~ConnectionRequest() {
        if (mSocket) {
            reject();
        }
    }

    InfinibandSocket* accept(std::error_code& ec);

    void reject();

private:
    friend class InfinibandService;

    ConnectionRequest(InfinibandSocket* socket)
            : mSocket(socket) {
    }

    /// The socket with the pending connection request
    std::unique_ptr<InfinibandSocket> mSocket;
};

/**
 * @brief Interface class containing callbacks for various socket events
 */
class InfinibandAcceptorHandler {
public:
    virtual ~InfinibandAcceptorHandler();

    /**
     * @brief Handle a new incoming connection
     *
     * The connection is not yet in a fully connected state so any write operations on the socket will fail until the
     * onConnected callback is invoked from the socket.
     *
     * In case the connection is accepted the class is responsible for taking ownership of the InfinibandSocket object.
     *
     * @param request Connection request from the remote host
     */
    virtual void onConnection(ConnectionRequest request);
};

/**
 * @brief Socket acceptor to listen for new incoming connections
 */
class InfinibandAcceptor: public InfinibandBaseSocket {
public:
    InfinibandAcceptor(InfinibandService& service)
            : InfinibandBaseSocket(service.channel(), this),
              mHandler(nullptr) {
    }

    void listen(int backlog, std::error_code& ec);

    void setHandler(InfinibandAcceptorHandler* handler) {
        mHandler = handler;
    }

private:
    friend class InfinibandService;

    /**
     * @brief The listen socket received a new connection request
     *
     * Invoke the onConnection handler to pass the connection request to the owner.
     */
    void onConnectionRequest(ConnectionRequest request) {
        mHandler->onConnection(std::move(request));
    }

    /// Callback handlers for events occuring on this socket
    InfinibandAcceptorHandler* mHandler;
};

/**
 * @brief Interface class containing callbacks for various socket events
 */
class InfinibandSocketHandler {
public:
    virtual ~InfinibandSocketHandler();

    /**
     * @brief Invoked when the connection to the remote host was established
     *
     * The socket is now able to send and receive messages to and from the remote host.
     *
     * Beware of race conditions in a multithreaded execution environment: The remote end might start sending data
     * before the onConnected function is executed, in this case another thread might call onReceive before onConnected.
     *
     * @param ec Error in case the connection attempt failed
     */
    virtual void onConnected(const std::error_code& ec);

    /**
     * @brief Invoked whenever data was received from the remote host
     *
     * The function must not release the buffer containing the data.
     *
     * @param buffer Pointer to the transmitted data
     * @param length Number of bytes transmitted
     * @param ec Error in case the receive failed
     */
    virtual void onReceive(const void* buffer, size_t length, const std::error_code& ec);

    /**
     * @brief Invoked whenever data was sent to the remote host
     *
     * The function must not release the buffer containing the data.
     *
     * @param userId The user supplied ID of the send call
     * @param ec Error in case the send failed
     */
    virtual void onSend(uint32_t userId, const std::error_code& ec);

    /**
     * @brief Invoked whenever data was read from the remote host
     *
     * @param userId The user supplied ID of the read call
     * @param ec Error in case the read failed
     */
    virtual void onRead(uint32_t userId, const std::error_code& ec);

    /**
     * @brief Invoked whenever data was written to the remote host
     *
     * @param userId The user supplied ID of the write call
     * @param ec Error in case the write failed
     */
    virtual void onWrite(uint32_t userId, const std::error_code& ec);

    /**
     * @brief Invoked whenever the remote host disconnected
     *
     * In order to shutdown the connection the handler should also disconnect from the remote host.
     *
     * Receives may be triggered even after this callback was invoked from remaining packets that were in flight.
     */
    virtual void onDisconnect();

    /**
     * @brief Invoked whenever the connection is disconnected
     *
     * Any remaining in flight packets were processed, it is now safe to clean up the connection.
     */
    virtual void onDisconnected();
};

/**
 * @brief Socket class to communicate with a remote host
 */
class InfinibandSocket: public InfinibandBaseSocket {
public:
    InfinibandSocket(InfinibandService& service)
            : InfinibandBaseSocket(service.channel(), this),
              mDevice(service.device()),
              mHandler(nullptr),
              mWork(1) {
    }

    void setHandler(InfinibandSocketHandler* handler) {
        mHandler = handler;
    }

    void connect(Endpoint& addr, std::error_code& ec);

    void disconnect(std::error_code& ec);

    void send(InfinibandBuffer& buffer, uint32_t userId, std::error_code& ec);

    /**
     * @brief Start a RDMA read from the remote memory region with offset into the local target buffer
     *
     * @param src Remote memory region to read from
     * @param offset Offset into the remote memory region
     * @param dst Local buffer to write the data into
     * @param userId User supplied ID passed to the completion handler
     * @param ec Error in case the read failed
     */
    void read(const RemoteMemoryRegion& src, size_t offset, InfinibandBuffer& dst, uint32_t userId,
            std::error_code& ec);

    /**
     * @brief Start a RDMA write from the local source buffer into the remote memory region with offset
     *
     * @param src Local buffer to read the data from
     * @param dst Remote memory region to write the data into
     * @param offset Offset into the remote memory region
     * @param userId User supplied ID passed to the completion handler
     * @param ec Error in case the write failed
     */
    void write(InfinibandBuffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
            std::error_code& ec);

    uint32_t bufferLength() const;

    InfinibandBuffer acquireSendBuffer();

    InfinibandBuffer acquireSendBuffer(uint32_t length);

    void releaseSendBuffer(InfinibandBuffer& buffer);

private:
    friend class CompletionContext;
    friend class ConnectionRequest;
    friend class InfinibandService;

    InfinibandSocket(InfinibandService& service, struct rdma_cm_id* id)
            : InfinibandBaseSocket(id, this),
              mDevice(service.device()),
              mHandler(nullptr),
              mWork(1) {
    }

    /**
     * @brief Accepts the pending connection request
     */
    void accept(std::error_code& ec);

    /**
     * @brief Rejects the pending connection request
     */
    void reject(std::error_code& ec);

    void doSend(struct ibv_send_wr* wr, std::error_code& ec);

    /**
     * @brief The address to the remote host was successfully resolved
     *
     * Continue by resolving the route to the remote host.
     */
    void onAddressResolved();

    /**
     * @brief Error while resolving address
     *
     * Invoke the onConnected handler to inform the owner that the connection attempt failed.
     */
    void onAddressError() {
        mHandler->onConnected(error::address_resolution);
    }

    /**
     * @brief The route to the remote host was successfully resolved
     *
     * Setup the socket and establish the connection to the remote host.
     */
    void onRouteResolved();

    /**
     * @brief Error while resolving route
     *
     * Invoke the onConnected handler to inform the owner that the connection attempt failed.
     */
    void onRouteError() {
        mHandler->onConnected(error::route_resolution);
    }

    /**
     * @brief An error has occured while establishing a connection
     *
     * Invoke the onConnected handler to inform the owner that the connection attempt failed.
     */
    void onConnectionError(error::network_errors err);

    /**
     * @brief The connection has been established
     *
     * Invoke the onConnected handler to inform the owner that the socket is now connected.
     */
    void onConnectionEstablished() {
        mHandler->onConnected(std::error_code());
    }

    /**
     * @brief The remote connection has been disconnected
     *
     * Executed when the remote host triggered a disconnect. Invoke the onDisconnect handler so that the socket can
     * close on the local host.
     */
    void onDisconnected() {
        mHandler->onDisconnect();
        // TODO Call disconnect here directly? The socket has to call it anyway to succesfully shutdown the connection
    }

    /**
     * @brief The connection has exited the Timewait state
     *
     * After disconnecting the connection any packets that were still in flight have now been processed by the NIC. Tell
     * the connection to drain the completion queue so it can be disconnected.
     */
    void onTimewaitExit();

    /**
     * @brief The connection completed a receive operation
     */
    void onReceive(const void* buffer, uint32_t length, const std::error_code& ec) {
        mHandler->onReceive(buffer, length, ec);
    }

    /**
     * @brief The connection completed a send operation
     */
    void onSend(uint32_t userId, const std::error_code& ec) {
        mHandler->onSend(userId, ec);
    }

    /**
     * @brief The connection completed a read operation
     */
    void onRead(uint32_t userId, const std::error_code& ec) {
        mHandler->onRead(userId, ec);
    }

    /**
     * @brief The connection completed a write operation
     */
    void onWrite(uint32_t userId, const std::error_code& ec) {
        mHandler->onWrite(userId, ec);
    }

    /**
     * @brief The completion queue processed all pending work completions
     */
    void onDrained() {
        // The work count by default is set to 1, the completion queue was drained of all pending work completions, so
        // decrease the work count again so it hits 0 when no handlers are active anymore
        removeWork();
    }

    /**
     * @brief Mark a new handler as active
     *
     * This function should be called before a handler associated with a callback on the socket is dispatched to the
     * EventDispatcher.
     */
    void addWork() {
        ++mWork;
    }

    /**
     * @brief Mark an active handler as completed
     *
     * This function should be called after the callback on the socket was executed.
     *
     * Shuts down the connection in case no more handlers are active.
     */
    void removeWork();

    /// The device context of the underlying NIC
    DeviceContext* mDevice;

    /// Callback handlers for events occuring on this socket
    InfinibandSocketHandler* mHandler;

    /// Number of handlers pending their execution
    std::atomic<uint64_t> mWork;
};

} // namespace infinio
} // namespace crossbow
