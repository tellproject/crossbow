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
#pragma once

#include <crossbow/infinio/Endpoint.hpp>
#include <crossbow/infinio/ErrorCode.hpp>
#include <crossbow/infinio/InfinibandBuffer.hpp>
#include <crossbow/string.hpp>

#include <boost/smart_ptr/intrusive_ptr.hpp>

#include <atomic>
#include <cassert>
#include <cstdint>
#include <functional>
#include <memory>
#include <system_error>

#include <rdma/rdma_cma.h>

namespace crossbow {
namespace infinio {

class InfinibandProcessor;

class InfinibandSocketImpl;
using InfinibandSocket = boost::intrusive_ptr<InfinibandSocketImpl>;

template <typename SocketType>
class InfinibandBaseSocket;

template <typename SocketType>
inline void intrusive_ptr_add_ref(const InfinibandBaseSocket<SocketType>* socket) noexcept;

template <typename SocketType>
inline void intrusive_ptr_release(const InfinibandBaseSocket<SocketType>* socket) noexcept;

/**
 * @brief Base class containing common functionality shared between InfinibandAcceptor and InfinibandSocket
 */
template <typename SocketType>
class InfinibandBaseSocket {
public:
    void open();

    bool isOpen() const {
        return (mId != nullptr);
    }

    void close();

    void bind(Endpoint& addr);

protected:
    InfinibandBaseSocket(struct rdma_event_channel* channel)
            : mChannel(channel),
              mId(nullptr),
              mReferenceCount(0x0u) {
    }

    InfinibandBaseSocket(struct rdma_cm_id* id)
            : mChannel(id->channel),
              mId(id),
              mReferenceCount(0x0u) {
        // The ID is already open, increment the reference count by one and then detach the pointer
        intrusive_ptr_add_ref(this);
        mId->context = static_cast<SocketType*>(this);
    }

    /// The RDMA channel the connection is associated with
    struct rdma_event_channel* mChannel;

    /// The RDMA ID of this connection
    struct rdma_cm_id* mId;

private:
    friend void intrusive_ptr_add_ref<SocketType>(const InfinibandBaseSocket<SocketType>* socket) noexcept;
    friend void intrusive_ptr_release<SocketType>(const InfinibandBaseSocket<SocketType>* socket) noexcept;

    mutable std::atomic<uint64_t> mReferenceCount;
};

template <typename SocketType>
inline void intrusive_ptr_add_ref(const InfinibandBaseSocket<SocketType>* socket) noexcept {
    ++(socket->mReferenceCount);
}

template <typename SocketType>
inline void intrusive_ptr_release(const InfinibandBaseSocket<SocketType>* socket) noexcept {
    auto count = --(socket->mReferenceCount);
    if (count == 0) {
        delete static_cast<const SocketType*>(socket);
    }
}

/**
 * @brief Interface class containing callbacks for various socket events
 */
class InfinibandAcceptorHandler {
public:
    virtual ~InfinibandAcceptorHandler();

    /**
     * @brief Handle a new incoming connection
     *
     * The class is responsible for taking ownership of the InfinibandSocket object and either has to accept or reject
     * the connection request. Otherwise the connection will stay open.
     *
     * After accepting the connection, it is not yet in a fully connected state so any write operations on the socket
     * will fail until the onConnected callback is invoked on the socket.
     *
     * The reported length of the private data string may be larger than the actual data sent (as dictated by the
     * underlying transport). Any additional bytes are zeroed out.
     *
     * @param socket The socket to the remote host
     * @param data The private data send with the connection request
     */
    virtual void onConnection(InfinibandSocket socket, const crossbow::string& data);
};

/**
 * @brief Socket acceptor to listen for new incoming connections
 */
class InfinibandAcceptorImpl: public InfinibandBaseSocket<InfinibandAcceptorImpl> {
public:
    void listen(int backlog);

    void setHandler(InfinibandAcceptorHandler* handler) {
        mHandler = handler;
    }

private:
    friend class InfinibandService;

    InfinibandAcceptorImpl(struct rdma_event_channel* channel)
            : InfinibandBaseSocket(channel),
              mHandler(nullptr) {
    }

    /**
     * @brief The listen socket received a new connection request
     *
     * Invoke the onConnection handler to pass the connection request to the owner.
     */
    void onConnectionRequest(InfinibandSocket socket, const crossbow::string& data) {
        mHandler->onConnection(std::move(socket), data);
    }

    /// Callback handlers for events occuring on this socket
    InfinibandAcceptorHandler* mHandler;
};

extern template class InfinibandBaseSocket<InfinibandAcceptorImpl>;

using InfinibandAcceptor = boost::intrusive_ptr<InfinibandAcceptorImpl>;

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
     * Beware of race conditions: The remote end might start sending data before the onConnected function is executed,
     * in this case onReceive might be called before onConnected.
     *
     * @param data Private data sent by the remote host
     * @param ec Error in case the connection attempt failed
     */
    virtual void onConnected(const crossbow::string& data, const std::error_code& ec);

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
    virtual void onRead(uint32_t userId, uint16_t bufferId, const std::error_code& ec);

    /**
     * @brief Invoked whenever data was written to the remote host
     *
     * @param userId The user supplied ID of the write call
     * @param ec Error in case the write failed
     */
    virtual void onWrite(uint32_t userId, uint16_t bufferId, const std::error_code& ec);

    /**
     * @brief Invoked whenever immediate data was received from the remote host
     *
     * @param data The immediate data send by the remote host
     */
    virtual void onImmediate(uint32_t data);

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
class InfinibandSocketImpl: public InfinibandBaseSocket<InfinibandSocketImpl> {
public:
    InfinibandProcessor* processor() {
        return mProcessor;
    }

    void setHandler(InfinibandSocketHandler* handler) {
        mHandler = handler;
    }

    void connect(Endpoint& addr);

    void connect(Endpoint& addr, const crossbow::string& data);

    void disconnect();

    /**
     * @brief Accepts the pending connection request
     *
     * @param data The private data to send to the remote host
     * @param processor The polling thread this socket should process on
     * @param ec Error code in case the accept failed
     */
    void accept(const crossbow::string& data, InfinibandProcessor& processor);

    /**
     * @brief Rejects the pending connection request
     *
     * @param data The private data to send to the remote host
     * @param ec Error code in case the reject failed
     */
    void reject(const crossbow::string& data);

    /**
     * @brief The address of the remote host
     */
    Endpoint remoteAddress() const;

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

    void read(const RemoteMemoryRegion& src, size_t offset, ScatterGatherBuffer& dst, uint32_t userId,
            std::error_code& ec);

    /**
     * @brief Start a unsignaled RDMA read from the remote memory region with offset into the local target buffer
     *
     * The onRead event handler will not be invoked in case the read succeeds.
     *
     * @param src Remote memory region to read from
     * @param offset Offset into the remote memory region
     * @param dst Local buffer to write the data into
     * @param userId User supplied ID passed to the completion handler
     * @param ec Error in case the read failed
     */
    void readUnsignaled(const RemoteMemoryRegion& src, size_t offset, InfinibandBuffer& dst, uint32_t userId,
            std::error_code& ec);

    void readUnsignaled(const RemoteMemoryRegion& src, size_t offset, ScatterGatherBuffer& dst, uint32_t userId,
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

    void write(ScatterGatherBuffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
            std::error_code& ec);

    /**
     * @brief Start a immediate data RDMA write from the local source buffer into the remote memory region with offset
     *
     * @param src Local buffer to read the data from
     * @param dst Remote memory region to write the data into
     * @param offset Offset into the remote memory region
     * @param userId User supplied ID passed to the completion handler
     * @param immediate The immediate data in host byte order to send with the RDMA write
     * @param ec Error in case the write failed
     */
    void write(InfinibandBuffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId, uint32_t immediate,
            std::error_code& ec);

    void write(ScatterGatherBuffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
            uint32_t immediate, std::error_code& ec);

    /**
     * @brief Start a unsignaled RDMA write from the local source buffer into the remote memory region with offset
     *
     * The onWrite event handler will not be invoked in case the write succeeds.
     *
     * @param src Local buffer to read the data from
     * @param dst Remote memory region to write the data into
     * @param offset Offset into the remote memory region
     * @param userId User supplied ID passed to the completion handler
     * @param ec Error in case the write failed
     */
    void writeUnsignaled(InfinibandBuffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
            std::error_code& ec);

    void writeUnsignaled(ScatterGatherBuffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
            std::error_code& ec);

    /**
     * @brief Start a unsignaled immediate data RDMA write from the local source buffer into the remote memory region
     *        with offset
     *
     * The onWrite event handler will not be invoked in case the write succeeds.
     *
     * @param src Local buffer to read the data from
     * @param dst Remote memory region to write the data into
     * @param offset Offset into the remote memory region
     * @param userId User supplied ID passed to the completion handler
     * @param immediate The immediate data in host byte order to send with the RDMA write
     * @param ec Error in case the write failed
     */
    void writeUnsignaled(InfinibandBuffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
            uint32_t immediate, std::error_code& ec);

    void writeUnsignaled(ScatterGatherBuffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
            uint32_t immediate, std::error_code& ec);

    uint32_t bufferLength() const;

    InfinibandBuffer acquireSendBuffer();

    InfinibandBuffer acquireSendBuffer(uint32_t length);

    void releaseSendBuffer(InfinibandBuffer& buffer);

private:
    friend class CompletionContext;
    friend class ConnectionRequest;
    friend class InfinibandService;

    InfinibandSocketImpl(InfinibandProcessor& processor, struct rdma_event_channel* channel)
            : InfinibandBaseSocket(channel),
              mProcessor(&processor),
              mHandler(nullptr) {
    }

    InfinibandSocketImpl(struct rdma_cm_id* id)
            : InfinibandBaseSocket(id),
              mProcessor(nullptr),
              mHandler(nullptr) {
    }

    void doSend(struct ibv_send_wr* wr, std::error_code& ec);

    template <typename Buffer>
    void doRead(const RemoteMemoryRegion& src, size_t offset, Buffer& dst, uint32_t userId, int flags,
            std::error_code& ec);

    template <typename Buffer>
    void doWrite(Buffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId, int flags,
        std::error_code& ec);

    template <typename Buffer>
    void doWrite(Buffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId, uint32_t immediate,
        int flags, std::error_code& ec);

    /**
     * @brief The address to the remote host was successfully resolved
     *
     * Continue by resolving the route to the remote host.
     */
    void onAddressResolved();

    /**
     * @brief The route to the remote host was successfully resolved
     *
     * Setup the socket and establish the connection to the remote host.
     */
    void onRouteResolved();

    /**
     * @brief An error has occured while resolving the address or route of a connection
     *
     * Invoke the onConnected handler to inform the owner that the connection attempt failed.
     */
    void onResolutionError(error::network_errors err);

    /**
     * @brief An error has occured while establishing a connection
     *
     * Invoke the onConnected handler to inform the owner that the connection attempt failed.
     */
    void onConnectionError(error::network_errors err);

    /**
     * @brief The connection has been rejected
     *
     * Invoke the onConnected handler to inform the owner that the connection attempt failed.
     */
    void onConnectionRejected(const crossbow::string& data);

    /**
     * @brief The connection has been established
     *
     * Invoke the onConnected handler to inform the owner that the socket is now connected.
     */
    void onConnectionEstablished(const crossbow::string& data);

    /**
     * @brief The remote connection has been disconnected
     *
     * Executed when the remote host triggered a disconnect. Invoke the onDisconnect handler so that the socket can
     * close on the local host.
     */
    void onDisconnected();

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
    void onRead(uint32_t userId, uint16_t bufferId, const std::error_code& ec) {
        mHandler->onRead(userId, bufferId, ec);
    }

    /**
     * @brief The connection completed a write operation
     */
    void onWrite(uint32_t userId, uint16_t bufferId, const std::error_code& ec) {
        mHandler->onWrite(userId, bufferId, ec);
    }

    /**
     * @brief The connection completed a receive with immediate operation
     */
    void onImmediate(uint32_t data) {
        mHandler->onImmediate(data);
    }

    /**
     * @brief The completion queue processed all pending work completions and is now disconnected
     */
    void onDrained();

    /// The Infiniband processor managing this connection
    InfinibandProcessor* mProcessor;

    /// Callback handlers for events occuring on this socket
    InfinibandSocketHandler* mHandler;

    /// The private data to send while connecting
    crossbow::string mData;
};

extern template class InfinibandBaseSocket<InfinibandSocketImpl>;

} // namespace infinio
} // namespace crossbow
