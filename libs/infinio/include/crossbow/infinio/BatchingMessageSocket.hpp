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

#include <crossbow/byte_buffer.hpp>
#include <crossbow/infinio/ErrorCode.hpp>
#include <crossbow/infinio/InfinibandBuffer.hpp>
#include <crossbow/infinio/InfinibandService.hpp>
#include <crossbow/infinio/InfinibandSocket.hpp>
#include <crossbow/infinio/MessageId.hpp>
#include <crossbow/logger.hpp>
#include <crossbow/non_copyable.hpp>

#include <cstddef>
#include <cstdint>
#include <system_error>

namespace crossbow {
namespace infinio {

/**
 * @brief Socket wrapper for sending and receiving messages
 *
 * The implementation transparently batches multiple messages into one Infiniband buffer to increase throughput at the
 * cost of latency. Messages are flushed every time all events in the poll round have been processed.
 */
template <typename Handler>
class BatchingMessageSocket : protected InfinibandSocketHandler, crossbow::non_copyable, crossbow::non_movable {
public:
    bool isConnected() const {
        return (mState == ConnectionState::CONNECTED);
    }

protected:
    enum class ConnectionState {
        DISCONNECTED,
        SHUTDOWN,
        CONNECTING,
        CONNECTED,
    };

    BatchingMessageSocket(InfinibandSocket socket, size_t maxBatchSize = std::numeric_limits<size_t>::max())
            : mSocket(std::move(socket)),
              mMaxBatchSize(maxBatchSize),
              mBuffer(InfinibandBuffer::INVALID_ID),
              mSendBuffer(static_cast<char*>(nullptr), 0),
              mBatchSize(0),
              mState(ConnectionState::DISCONNECTED),
              mFlush(false) {
        mSocket->setHandler(this);
        if (!mSocket->isOpen()) {
            mSocket->open();
        }
    }

    virtual ~BatchingMessageSocket();

    ConnectionState state() const {
        return mState;
    }

    /**
     * @brief Start the connection to the remote host
     */
    void connect(Endpoint endpoint, const crossbow::string& data);

    /**
     * @brief Accept the connection from the remote host
     */
    void accept(const crossbow::string& data, InfinibandProcessor& processor);

    /**
     * @brief Shutdown the connection to the remote host
     */
    void shutdown();

    /**
     * @brief Allocates buffer space for a message
     *
     * The implementation performs message batching: Messages share a common send buffer which is scheduled for sending
     * each poll interval.
     *
     * @param messageId A unique ID associated with the message to be written
     * @param messageType The type of the message to be written
     * @param messageLength The length of the message to be written
     * @param ec Error in case allocating the buffer failed
     * @return The buffer to write the message content
     */
    template <typename Fun>
    void writeMessage(MessageId messageId, uint32_t messageType, uint32_t messageLength, Fun fun, std::error_code& ec);

    void handleSocketError(const std::error_code& ec);

    InfinibandSocket mSocket;

private:
    static constexpr uint32_t HEADER_SIZE = sizeof(uint64_t) + 2 * sizeof(uint32_t);

    /**
     * @brief Callback function invoked by the Infiniband socket
     *
     * Handles the connection setup phase: In case the connection was successfully established the onSocketConnected
     * callback is invoked.
     */
    virtual void onConnected(const crossbow::string& data, const std::error_code& ec) final override;

    /**
     * @brief Callback function invoked by the Infiniband socket
     *
     * Reads all messages contained in the receive buffer and for every message invokes the onMessage callback on the
     * handler.
     */
    virtual void onReceive(const void* buffer, size_t length, const std::error_code& ec) final override;

    /**
     * @brief Callback function invoked by the Infiniband socket
     */
    virtual void onSend(uint32_t userId, const std::error_code& ec) final override;

    /**
     * @brief Callback function invoked by the Infiniband socket
     *
     * Invoked when the remote host disconnected: Disconnects the this side of connection.
     */
    virtual void onDisconnect() final override;

    /**
     * @brief Callback function invoked by the Infiniband socket
     *
     * Invoked when the remote host completely disconnected: Invokes the onSocketDisconnected callback.
     */
    virtual void onDisconnected() final override;

    /**
     * @brief Sends the current send buffer
     *
     * @param ec Error in case the send failed
     */
    void sendCurrentBuffer(std::error_code& ec);

    /**
     * @brief Schedules the socket's send buffer to be flushed
     *
     * If the flush is not yet pending this enqueues a task on the socket's completion context to send the current send
     * buffer.
     *
     * @param ec Error in case the scheduling failed
     */
    void scheduleFlush();

    /// Maximum number of messages in a batch before sending the current message
    size_t mMaxBatchSize;

    /// The current Infiniband send buffer
    InfinibandBuffer mBuffer;

    /// A buffer writer to write data to the current Infiniband buffer
    crossbow::buffer_writer mSendBuffer;

    /// Number of messages in the current batch
    size_t mBatchSize;

    /// The connection state of the socket
    ConnectionState mState;

    /// Whether a flush task is pending
    bool mFlush;
};

template <typename Handler>
BatchingMessageSocket<Handler>::~BatchingMessageSocket() {
    // TODO Make this less restrictive
    while (mState == ConnectionState::SHUTDOWN) {
        std::this_thread::yield();
    }
    LOG_ASSERT(mState == ConnectionState::DISCONNECTED, "Socket must be disconnected");

    try {
        mSocket->close();
    } catch (std::system_error& e) {
        LOG_ERROR("Error while closing socket [error = %1% %2%]", e.code(), e.what());
    }
}

template <typename Handler>
void BatchingMessageSocket<Handler>::connect(Endpoint endpoint, const crossbow::string& data) {
    if (mState != ConnectionState::DISCONNECTED) {
        throw std::system_error(std::make_error_code(std::errc::already_connected));
    }

    mSocket->connect(endpoint, data);

    mState = ConnectionState::CONNECTING;
}

template <typename Handler>
void BatchingMessageSocket<Handler>::accept(const crossbow::string& data, InfinibandProcessor& processor) {
    if (mState != ConnectionState::DISCONNECTED) {
        throw std::system_error(std::make_error_code(std::errc::already_connected));
    }

    mSocket->accept(data, processor);
    mState = ConnectionState::CONNECTING;
}

template <typename Handler>
void BatchingMessageSocket<Handler>::shutdown() {
    if (mState != ConnectionState::CONNECTED) {
        throw std::system_error(std::make_error_code(std::errc::not_connected));
    }

    mState = ConnectionState::SHUTDOWN;
    try {
        mSocket->disconnect();
    } catch (std::system_error& e) {
        LOG_ERROR("Error disconnecting socket [error = %1% %2%]", e.code(), e.what());
    }
}

template <typename Handler>
template <typename Fun>
void BatchingMessageSocket<Handler>::writeMessage(MessageId messageId, uint32_t messageType, uint32_t messageLength,
        Fun fun, std::error_code& ec) {
    auto length = HEADER_SIZE + messageLength;
    if (!mSendBuffer.canWrite(length)) {
        // Check if the buffer writer points to the beginning of the send buffer - in this case nothing has been written
        // and the message is too big to fit into a buffer.
        if (mBuffer.valid() && (mSendBuffer.data() == reinterpret_cast<char*>(mBuffer.data()))) {
            ec = error::message_too_big;
            return;
        }

        sendCurrentBuffer(ec);
        if (ec) {
            return;
        }

        mBuffer = mSocket->acquireSendBuffer();
        if (!mBuffer.valid()) {
            mSendBuffer = crossbow::buffer_writer(static_cast<char*>(nullptr), 0);
            ec = error::invalid_buffer;
            return;
        }
        mSendBuffer = crossbow::buffer_writer(reinterpret_cast<char*>(mBuffer.data()), mBuffer.length());

        scheduleFlush();

        // If a new buffer is still too small the message must be too big
        if (!mSendBuffer.canWrite(length)) {
            ec = error::message_too_big;
            return;
        }
    }

    auto oldOffset = static_cast<uint32_t>(mSendBuffer.data() - reinterpret_cast<char*>(mBuffer.data()));

    mSendBuffer.write<uint64_t>(messageId.id());
    mSendBuffer.write<uint32_t>(messageType);
    mSendBuffer.write<uint32_t>(messageLength);
    auto message = mSendBuffer.extract(messageLength);
    mSendBuffer.align(sizeof(uint64_t));

    fun(message, ec);
    if (ec) {
        mSendBuffer = crossbow::buffer_writer(reinterpret_cast<char*>(mBuffer.data()) + oldOffset,
                mBuffer.length() - oldOffset);
        return;
    }

    ++mBatchSize;
    if (mBatchSize == mMaxBatchSize) {
        sendCurrentBuffer(ec);
    }
}

template <typename Handler>
void BatchingMessageSocket<Handler>::onConnected(const crossbow::string& data, const std::error_code& ec) {
    LOG_ASSERT(mState == ConnectionState::CONNECTING, "State is not connecting");

    if (ec) {
        mState = ConnectionState::DISCONNECTED;
        handleSocketError(ec);
        return;
    }

    mState = ConnectionState::CONNECTED;

    static_cast<Handler*>(this)->onSocketConnected(data);
}

template <typename Handler>
void BatchingMessageSocket<Handler>::onReceive(const void* buffer, size_t length, const std::error_code& ec) {
    if (ec) {
        handleSocketError(ec);
        return;
    }

    crossbow::buffer_reader receiveBuffer(reinterpret_cast<const char*>(buffer), length);
    while (receiveBuffer.canRead(HEADER_SIZE)) {
        MessageId messageId(receiveBuffer.read<uint64_t>());
        auto messageType = receiveBuffer.read<uint32_t>();
        auto messageLength = receiveBuffer.read<uint32_t>();

        if (!receiveBuffer.canRead(messageLength)) {
            handleSocketError(error::invalid_message);
            return;
        }
        auto message = receiveBuffer.extract(messageLength);

        static_cast<Handler*>(this)->onMessage(messageId, messageType, message);

        receiveBuffer.align(sizeof(uint64_t));
    }
}

template <typename Handler>
void BatchingMessageSocket<Handler>::onSend(uint32_t userId, const std::error_code& ec) {
    if (ec) {
        handleSocketError(ec);
        return;
    }
}

template <typename Handler>
void BatchingMessageSocket<Handler>::onDisconnect() {
    shutdown();
}

template <typename Handler>
void BatchingMessageSocket<Handler>::onDisconnected() {
    mState = ConnectionState::DISCONNECTED;

    static_cast<Handler*>(this)->onSocketDisconnected();
}

template <typename Handler>
void BatchingMessageSocket<Handler>::sendCurrentBuffer(std::error_code& ec) {
    if (!mBuffer.valid()) {
        return;
    }

    auto bytesWritten = static_cast<size_t>(mSendBuffer.data() - reinterpret_cast<char*>(mBuffer.data()));
    mBuffer.shrink(bytesWritten);

    mSocket->send(mBuffer, 0, ec);
    if (ec)  {
        mSocket->releaseSendBuffer(mBuffer);
    }
    mBatchSize = 0;
    mBuffer = InfinibandBuffer(InfinibandBuffer::INVALID_ID);
    mSendBuffer = crossbow::buffer_writer(static_cast<char*>(nullptr), 0);
}

template <typename Handler>
void BatchingMessageSocket<Handler>::scheduleFlush() {
    if (mFlush) {
        return;
    }

    mSocket->processor()->executeLocal([this] () {
        mFlush = false;

        // Check if buffer is valid
        if (!mBuffer.valid()) {
            return;
        }

        // Check if buffer has any data written
        if (mSendBuffer.data() == reinterpret_cast<char*>(mBuffer.data())) {
            LOG_ASSERT(mBatchSize == 0, "Batch size must be 0 for empty messages");
            mSocket->releaseSendBuffer(mBuffer);
            mBuffer = InfinibandBuffer(InfinibandBuffer::INVALID_ID);
            mSendBuffer = crossbow::buffer_writer(static_cast<char*>(nullptr), 0);
            return;
        }

        std::error_code ec;
        sendCurrentBuffer(ec);
        if (ec) {
            handleSocketError(ec);
            return;
        }
    });

    mFlush = true;
}

template <typename Handler>
void BatchingMessageSocket<Handler>::handleSocketError(const std::error_code& ec) {
    LOG_ERROR("Error during socket operation [error = %1% %2%]", ec, ec.message());

    // TODO Try to recover from errors

    shutdown();
}

} // namespace infinio
} // namespace crossbow
