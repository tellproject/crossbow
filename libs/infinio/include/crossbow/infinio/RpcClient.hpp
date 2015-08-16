#pragma once

#include <crossbow/byte_buffer.hpp>
#include <crossbow/enum_underlying.hpp>
#include <crossbow/infinio/BatchingMessageSocket.hpp>
#include <crossbow/infinio/Fiber.hpp>
#include <crossbow/logger.hpp>
#include <crossbow/string.hpp>

#include <sparsehash/dense_hash_map>

#include <cstdint>
#include <limits>
#include <queue>
#include <system_error>
#include <tuple>
#include <type_traits>

namespace crossbow {
namespace infinio {
namespace detail {

extern const std::error_code gEmptyErrorCode;

} // namespace detail

class Fiber;

/**
 * @brief Base class representing a pending RPC response
 */
class RpcResponse {
public:
    /**
     * @brief Suspend the fiber until notified by another context or an response was received
     *
     * Blocks until notify() or complete() are called.
     *
     * @return Whether an response was received
     */
    bool wait();

    /**
     * @brief Whether an response was received
     */
    bool done() const {
        return !mPending;
    }

    Fiber& fiber() {
        return mFiber;
    }

protected:
    RpcResponse(Fiber& fiber)
            : mFiber(fiber),
              mPending(true),
              mWaiting(false) {
    }

    virtual ~RpcResponse();

    /**
     * @brief Notifies the response fiber of any event
     *
     * Will resume the fiber if it is waiting.
     */
    void notify();

    /**
     * @brief Notifies the response fiber of its completion
     *
     * Will resume the fiber if it is waiting.
     */
    void complete();

private:
    friend class RpcClientSocket;

    /**
     * @brief Called when a response with the given message type was received
     *
     * @param messageType The type of the received message
     * @param message The response message
     */
    virtual void onResponse(uint32_t messageType, crossbow::buffer_reader& message) = 0;

    /**
     * @brief Called when the RPC was aborted due to an error
     */
    virtual void onAbort(std::error_code ec) = 0;

    Fiber& mFiber;

    bool mPending;
    bool mWaiting;
};

enum class RpcResponseState {
    UNSET,
    SET,
    ERROR,
};

/**
 * @brief Class holding a future RPC response
 */
template <typename Handler, typename Result>
class RpcResponseResult : public RpcResponse {
public:
    using ResultType = Result;

    RpcResponseResult(Fiber& fiber)
            : RpcResponse(fiber),
              mState(RpcResponseState::UNSET),
              mRetrieved(false) {
    }

    virtual ~RpcResponseResult();

    /**
     * @brief Suspend the fiber until the response was received
     *
     * @return Whether the response is valid (contains an error otherwise)
     */
    bool waitForResult();

    /**
     * @brief Retrieve the response of the request
     *
     * Blocks until the response has been received.
     *
     * The result can only be retrieved once. An exception is thrown if the result is invalid or was already retrieved.
     *
     * @exception std::system_error The result is invalid
     * @exception std::future_error The result was already retrieved
     * @return The result of the RPC request
     */
    ResultType get();

    /**
     * @brief The error code if the response completed with an error
     *
     * Blocks until the response has been received.
     */
    const std::error_code& error();

protected:
    template <typename... Args>
    void setResult(Args&&... args) {
        setInternalResult<ResultType, RpcResponseState::SET, Args...>(std::forward<Args>(args)...);
    }

    template <typename... Args>
    void setError(Args&&... args) {
        setInternalResult<std::error_code, RpcResponseState::ERROR, Args...>(std::forward<Args>(args)...);
    }

private:
    virtual void onResponse(uint32_t messageType, crossbow::buffer_reader& message) final override;

    virtual void onAbort(std::error_code ec) final override;

    template <typename T, RpcResponseState State, typename... Args>
    void setInternalResult(Args&&... args);

    /// The state of the result container
    RpcResponseState mState;

    /// Whether the result was retrieved
    bool mRetrieved;

#if defined(__GNUC__) && __GNUC__ < 5
    static constexpr size_t maxResultSize = (sizeof(ResultType) > sizeof(std::error_code) ? sizeof(ResultType)
            : sizeof(std::error_code));
    static constexpr size_t maxResultAlign = (alignof(ResultType) > alignof(std::error_code) ? alignof(ResultType)
            : alignof(std::error_code));

    /// Union containing either nothing, the result or an error code
    typename std::aligned_storage<maxResultSize, maxResultAlign>::type mResult;
#else
    /// Union containing either nothing, the result or an error code
    typename std::aligned_union<0, ResultType, std::error_code>::type mResult;
#endif
};

template <typename Handler, typename Result>
RpcResponseResult<Handler, Result>::~RpcResponseResult() {
    switch (mState) {
    case RpcResponseState::SET: {
        reinterpret_cast<ResultType*>(&mResult)->~ResultType();
    } break;
    case RpcResponseState::ERROR: {
        reinterpret_cast<std::error_code*>(&mResult)->~error_code();
    } break;
    default:
        break;
    }
}

template <typename Handler, typename Result>
bool RpcResponseResult<Handler, Result>::waitForResult() {
    while (!wait()) {
    }
    LOG_ASSERT(mState != RpcResponseState::UNSET, "State is still unset after completion");

    return (mState == RpcResponseState::SET);
}

template <typename Handler, typename Result>
typename RpcResponseResult<Handler, Result>::ResultType RpcResponseResult<Handler, Result>::get() {
    if (!waitForResult()) {
        throw std::system_error(*reinterpret_cast<const std::error_code*>(&mResult));
    }

    if (mRetrieved) {
        throw std::future_error(std::future_errc::future_already_retrieved);
    }

    mRetrieved = true;
    return std::move(*reinterpret_cast<ResultType*>(&mResult));
}

template <typename Handler, typename Result>
const std::error_code& RpcResponseResult<Handler, Result>::error() {
    if (waitForResult()) {
        return detail::gEmptyErrorCode;
    }

    return *reinterpret_cast<const std::error_code*>(&mResult);
}

template <typename Handler, typename Result>
void RpcResponseResult<Handler, Result>::onResponse(uint32_t messageType, crossbow::buffer_reader& message) {
    static_assert(std::is_same<typename std::underlying_type<decltype(Handler::MessageType)>::type,
                  uint32_t>::value, "Given message type is not of the correct type");

    LOG_ASSERT(!done(), "Response is already done");
    LOG_ASSERT(mState == RpcResponseState::UNSET, "Result is already set");

    if (messageType == std::numeric_limits<uint32_t>::max()) {
        setError(message.read<uint64_t>(), Handler::errorCategory());
        return;
    }

    if (messageType != crossbow::to_underlying(Handler::MessageType)) {
        setError(error::wrong_type);
        return;
    }

    static_cast<Handler*>(this)->processResponse(message);
    if (mState == RpcResponseState::UNSET) {
        LOG_ASSERT(!done(), "Response is already done");
        setError(std::future_errc::broken_promise);
    }
}

template <typename Handler, typename Result>
void RpcResponseResult<Handler, Result>::onAbort(std::error_code ec) {
    LOG_ASSERT(!done(), "Response is already done");
    LOG_ASSERT(mState == RpcResponseState::UNSET, "Result is already set");

    setError(ec);
}

template <typename Handler, typename Result>
template <typename T, RpcResponseState State, typename... Args>
void RpcResponseResult<Handler, Result>::setInternalResult(Args&&... args) {
    static_assert(State != RpcResponseState::UNSET, "Result must not be unset");
    if (done()) {
        throw std::future_error(std::future_errc::promise_already_satisfied);
    }

    new (&mResult) T(std::forward<Args>(args)...);

    mState = State;
    complete();
}

/**
 * @brief Socket base class implementing client side basic remote procedure calls functionality
 */
class RpcClientSocket : protected BatchingMessageSocket<RpcClientSocket> {
    using Base = BatchingMessageSocket<RpcClientSocket>;

public:
    RpcClientSocket(InfinibandSocket socket, size_t maxPendingResponses = std::numeric_limits<size_t>::max());

protected:
    /**
     * @brief Send the synchronous request to the remote host
     *
     * Blocks the request if the connection is not yet ready - either when not yet connected or the maximum number of
     * concurrent pending responses has been reached.
     *
     * The server is expected to process all requests in the order they arrived.
     *
     * @param response The handler object to invoke when receiving the response
     * @param messageType The message type of the request to send
     * @param length The length of the message to send
     * @param fun Function writing the request to the given buffer
     */
    template <typename Fun, typename Message>
    bool sendRequest(std::shared_ptr<RpcResponse> response, Message messageType, uint32_t length, Fun fun);

    /**
     * @brief Send the asynchronous request to the remote host
     *
     * Blocks the request if the connection is not yet ready - either when not yet connected or the maximum number of
     * concurrent pending responses has been reached.
     *
     * @param response The handler object to invoke when receiving the response
     * @param messageType The message type of the request to send
     * @param length The length of the message to send
     * @param fun Function writing the request to the given buffer
     */
    template <typename Fun, typename Message>
    bool sendAsyncRequest(std::shared_ptr<RpcResponse> response, Message messageType, uint32_t length, Fun fun);

private:
    friend Base;

    template <typename Fun, typename Message>
    bool sendInternalRequest(std::shared_ptr<RpcResponse> response, Message messageType, bool async, uint32_t length,
            Fun fun);

    bool waitUntilReady(Fiber& fiber);

    void onSocketConnected(const crossbow::string& data);

    void onSocketDisconnected();

    void onMessage(MessageId messageId, uint32_t messageType, crossbow::buffer_reader& message);

    void onSyncResponse(uint32_t userId, uint32_t messageType, crossbow::buffer_reader& message);

    void onAsyncResponse(uint32_t userId, uint32_t messageType, crossbow::buffer_reader& message);

    /// Current user ID incremented for each request sent
    uint32_t mUserId;

    /// Number of requests pending a response
    size_t mPendingResponses;

    /// Maximum number of concurrent pending responses
    size_t mMaxPendingResponses;

    /// Queue containing pending synchronous responses in the order they were sent
    std::queue<std::tuple<uint32_t, std::shared_ptr<RpcResponse>>> mSyncResponses;

    /// Map containing asynchronous response not yet processed by the remote host
    google::dense_hash_map<uint32_t, std::shared_ptr<RpcResponse>> mAsyncResponses;

    /// Requests waiting for the connection to become ready
    ConditionVariable mWaitingRequests;
};

template <typename Fun, typename Message>
bool RpcClientSocket::sendInternalRequest(std::shared_ptr<RpcResponse> response, Message messageType, bool async,
        uint32_t length, Fun fun) {
    static_assert(std::is_same<typename std::underlying_type<Message>::type, uint32_t>::value,
            "Given message type is not of the correct type");
    LOG_ASSERT(crossbow::to_underlying(messageType) != std::numeric_limits<uint32_t>::max(),
            "Invalid message tupe");

    if (!waitUntilReady(response->fiber())) {
        response->onAbort(std::make_error_code(std::errc::connection_aborted));
        return false;
    }

    ++mUserId;

    MessageId messageId(mUserId, async);
    std::error_code ec;
    this->writeMessage(messageId, crossbow::to_underlying(messageType), length, std::move(fun), ec);
    if (ec) {
        response->onAbort(std::move(ec));
        return false;
    }

    return true;
}

template <typename Fun, typename Message>
bool RpcClientSocket::sendRequest(std::shared_ptr<RpcResponse> response, Message messageType, uint32_t length,
        Fun fun) {
    auto succeeded = sendInternalRequest(response, messageType, false, length, std::move(fun));
    if (succeeded) {
        ++mPendingResponses;
        mSyncResponses.emplace(mUserId, std::move(response));
    }
    return succeeded;
}

template <typename Fun, typename Message>
bool RpcClientSocket::sendAsyncRequest(std::shared_ptr<RpcResponse> response, Message messageType, uint32_t length,
        Fun fun) {
    auto succeeded = sendInternalRequest(response, messageType, true, length, std::move(fun));
    if (succeeded) {
        ++mPendingResponses;
        mAsyncResponses.insert(std::make_pair(mUserId, std::move(response)));
    }
    return succeeded;
}

} // namespace infinio
} // namespace crossbow
