#pragma once

#include <crossbow/infinio/ByteBuffer.hpp>
#include <crossbow/infinio/ErrorCode.hpp>
#include <crossbow/infinio/EventProcessor.hpp>
#include <crossbow/infinio/InfinibandBuffer.hpp>
#include <crossbow/infinio/InfinibandService.hpp>
#include <crossbow/infinio/InfinibandSocket.hpp>
#include <crossbow/logger.hpp>

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
class BatchingMessageSocket : protected InfinibandSocketHandler {
protected:
    BatchingMessageSocket(InfinibandSocket socket)
            : mSocket(std::move(socket)),
              mBuffer(InfinibandBuffer::INVALID_ID),
              mSendBuffer(static_cast<char*>(nullptr), 0),
              mOldOffset(std::numeric_limits<uint32_t>::max()),
              mFlush(false) {
        mSocket->setHandler(this);
    }

    ~BatchingMessageSocket() = default;

    /**
     * @brief Allocates buffer space for a message
     *
     * The implementation performs message batching: Messages share a common send buffer which is scheduled for sending
     * each poll interval.
     *
     * @param transactionId The transaction ID associated with the message to be written
     * @param messageType The type of the message to be written
     * @param messageLength The length of the message to be written
     * @param ec Error in case allocating the buffer failed
     * @return The buffer to write the message content
     */
    BufferWriter writeMessage(uint64_t transactionId, uint32_t messageType, uint32_t messageLength,
            std::error_code& ec);

    /**
     * @brief Deallocates the buffer space for the last allocated message
     *
     * This can only be called once and only after writeMessage has been called.
     */
    void revertMessage();

    InfinibandSocket mSocket;

private:
    static constexpr size_t HEADER_SIZE = sizeof(uint64_t) + 2 * sizeof(uint32_t);

    /**
     * @brief Callback function invoked by the Infinibadn socket.
     *
     * Reads all messages contained in the receive buffer and for every message invokes the onMessage callback on the
     * handler.
     */
    virtual void onReceive(const void* buffer, size_t length, const std::error_code& ec) final override;

    /**
     * @brief Callback function invoked by the Infiniband socket.
     *
     * Checks if the send was successfull and if it was not invokes the onSocketError callback on the handler.
     */
    virtual void onSend(uint32_t userId, const std::error_code& ec) final override;

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

    /// The current Infiniband send buffer
    InfinibandBuffer mBuffer;

    /// A buffer writer to write data to the current Infiniband buffer
    BufferWriter mSendBuffer;

    uint32_t mOldOffset;

    /// Whether a flush task is pending
    bool mFlush;
};

template <typename Handler>
BufferWriter BatchingMessageSocket<Handler>::writeMessage(uint64_t transactionId, uint32_t messageType,
        uint32_t messageLength, std::error_code& ec) {
    auto length = HEADER_SIZE + messageLength;
    if (!mSendBuffer.canWrite(length)) {
        sendCurrentBuffer(ec);
        if (ec) {
            return BufferWriter(nullptr, 0x0u);
        }

        mBuffer = mSocket->acquireSendBuffer();
        if (!mBuffer.valid()) {
            ec = error::invalid_buffer;
            return BufferWriter(nullptr, 0x0u);
        }
        mSendBuffer = BufferWriter(reinterpret_cast<char*>(mBuffer.data()), mBuffer.length());

        scheduleFlush();
    }

    mOldOffset = static_cast<uint32_t>(mSendBuffer.data() - reinterpret_cast<char*>(mBuffer.data()));

    mSendBuffer.write<uint64_t>(transactionId);
    mSendBuffer.write<uint32_t>(messageType);
    mSendBuffer.write<uint32_t>(messageLength);

    auto message = mSendBuffer.extract(messageLength);

    mSendBuffer.align(sizeof(uint64_t));

    return message;
}

template <typename Handler>
void BatchingMessageSocket<Handler>::revertMessage() {
    LOG_ASSERT(mOldOffset != std::numeric_limits<uint32_t>::max(), "Invalid offset");
    LOG_ASSERT(mBuffer.length() < mOldOffset, "Offset larger than buffer length");

    mSendBuffer = BufferWriter(reinterpret_cast<char*>(mBuffer.data()) + mOldOffset, mBuffer.length() - mOldOffset);
    mOldOffset = std::numeric_limits<uint32_t>::max();
}

template <typename Handler>
void BatchingMessageSocket<Handler>::onReceive(const void* buffer, size_t length, const std::error_code& ec) {
    if (ec) {
        static_cast<Handler*>(this)->onSocketError(ec);
        return;
    }

    BufferReader receiveBuffer(reinterpret_cast<const char*>(buffer), length);
    while (receiveBuffer.canRead(HEADER_SIZE)) {
        auto transactionId = receiveBuffer.read<uint64_t>();
        auto messageType = receiveBuffer.read<uint32_t>();
        auto messageLength = receiveBuffer.read<uint32_t>();

        if (!receiveBuffer.canRead(messageLength)) {
            static_cast<Handler*>(this)->onSocketError(error::invalid_message);
            return;
        }
        auto message = receiveBuffer.extract(messageLength);

        static_cast<Handler*>(this)->onMessage(transactionId, messageType, message);

        receiveBuffer.align(sizeof(uint64_t));
    }
}

template <typename Handler>
void BatchingMessageSocket<Handler>::onSend(uint32_t userId, const std::error_code& ec) {
    if (ec) {
        static_cast<Handler*>(this)->onSocketError(ec);
        return;
    }
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
    mBuffer = InfinibandBuffer(InfinibandBuffer::INVALID_ID);
    mSendBuffer = BufferWriter(static_cast<char*>(nullptr), 0);
}

template <typename Handler>
void BatchingMessageSocket<Handler>::scheduleFlush() {
    if (mFlush) {
        return;
    }

    mSocket->processor()->executeLocal([this] () {
        // Check if buffer is valid
        if (!mBuffer.valid()) {
            return;
        }

        // Check if buffer has any data written
        if (mSendBuffer.data() == reinterpret_cast<char*>(mBuffer.data())) {
            mSocket->releaseSendBuffer(mBuffer);
            mBuffer = InfinibandBuffer(InfinibandBuffer::INVALID_ID);
            mSendBuffer = BufferWriter(static_cast<char*>(nullptr), 0);
            return;
        }

        std::error_code ec;
        sendCurrentBuffer(ec);
        if (ec) {
            static_cast<Handler*>(this)->onSocketError(ec);
            return;
        }

        mFlush = false;
    });

    mFlush = true;
}

} // namespace infinio
} // namespace crossbow
