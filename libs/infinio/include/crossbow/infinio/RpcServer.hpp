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
#include <crossbow/enum_underlying.hpp>
#include <crossbow/infinio/BatchingMessageSocket.hpp>
#include <crossbow/infinio/InfinibandService.hpp>
#include <crossbow/logger.hpp>

#include <tbb/spin_rw_mutex.h>
#include <tbb/concurrent_unordered_set.h>

#include <cstdint>
#include <system_error>
#include <type_traits>

namespace crossbow {
namespace infinio {

template <typename Manager, typename Socket>
class RpcServerSocket;

/**
 * @brief Connection manager base class to manage RPC server connections
 *
 * A RPC server connection manager has to implement the createConnection(InfinibandSocket socket,
 * const crossbow::string& data) function to handle create a new implementation specific Connection object associated
 * with the socket.
 */
template <typename Manager, typename Socket>
class RpcServerManager : private InfinibandAcceptorHandler {
protected:
    RpcServerManager(InfinibandService& service, uint16_t port);

    void shutdown();

private:
    friend class RpcServerSocket<Manager, Socket>;

    virtual void onConnection(InfinibandSocket socket, const crossbow::string& data) final override;

    void removeConnection(Socket* con);

    InfinibandAcceptor mAcceptor;

    tbb::spin_rw_mutex mSocketsMutex;
    tbb::concurrent_unordered_set<Socket*> mSockets;
};

template <typename Manager, typename Socket>
RpcServerManager<Manager, Socket>::RpcServerManager(InfinibandService& service, uint16_t port)
        : mAcceptor(service.createAcceptor()) {
    mAcceptor->setHandler(this);
    mAcceptor->open();

    Endpoint ep(Endpoint::ipv4(), port);
    mAcceptor->bind(ep);
    mAcceptor->listen(10);
}

template <typename Manager, typename Socket>
void RpcServerManager<Manager, Socket>::shutdown() {
    mAcceptor->close();

    for (auto& con : mSockets) {
        con->shutdown();
    }

    // TODO Wait until all connections are disconnected
}

template <typename Manager, typename Socket>
void RpcServerManager<Manager, Socket>::onConnection(InfinibandSocket socket, const crossbow::string& data) {
    LOG_TRACE("Adding connection");
    try {
        auto con = static_cast<Manager*>(this)->createConnection(std::move(socket), data);

        typename decltype(mSocketsMutex)::scoped_lock _(mSocketsMutex, false);
        __attribute__((unused)) auto res = mSockets.insert(con);
        LOG_ASSERT(res.second, "New connection already in connection set");
    } catch (std::system_error& e) {
        LOG_ERROR("Error accepting connection [error = %1% %2%]", e.code(), e.what());
    }
}

template <typename Manager, typename Socket>
void RpcServerManager<Manager, Socket>::removeConnection(Socket* con) {
    LOG_TRACE("Removing connection");
    {
        typename decltype(mSocketsMutex)::scoped_lock lock(mSocketsMutex, true);
        mSockets.unsafe_erase(con);
    }
    delete con;
}

/**
 * @brief Socket base class implementing server side basic remote procedure calls functionality
 *
 * A RPC request handler has to implement the onResponse(MessageId messageId, uint32_t messageType, buffer_reader&
 * message) function to handle incoming requests.
 */
template <typename Manager, typename Socket>
class RpcServerSocket : public BatchingMessageSocket<RpcServerSocket<Manager, Socket>> {
    using Base = BatchingMessageSocket<RpcServerSocket<Manager, Socket>>;

public:

protected:
    RpcServerSocket(Manager& manager, InfinibandProcessor& processor, InfinibandSocket socket,
            const crossbow::string& data, size_t maxBatchSize = std::numeric_limits<size_t>::max());

    template <typename Fun, typename Message>
    bool writeResponse(MessageId messageId, Message messageType, uint32_t length, Fun fun);

    template <typename Error>
    bool writeErrorResponse(MessageId messageId, Error error);

    Manager& manager() {
        return mManager;
    }

private:
    friend Base;

    template <typename Fun>
    bool writeInternalResponse(MessageId messageId, uint32_t messageType, uint32_t length, Fun fun);

    void onMessage(MessageId messageId, uint32_t messageType, crossbow::buffer_reader& message) {
        static_cast<Socket*>(this)->onRequest(messageId, messageType, message);
    }

    void onSocketConnected(const crossbow::string& data);

    void onSocketDisconnected();

    Manager& mManager;
};

template <typename Manager, typename Socket>
RpcServerSocket<Manager, Socket>::RpcServerSocket(Manager& manager, InfinibandProcessor& processor,
        InfinibandSocket socket, const crossbow::string& data, size_t maxBatchSize)
        : Base(std::move(socket), maxBatchSize),
          mManager(manager) {
    this->accept(data, processor);
}

template <typename Manager, typename Socket>
template <typename Fun, typename Message>
bool RpcServerSocket<Manager, Socket>::writeResponse(MessageId messageId, Message messageType, uint32_t length,
        Fun fun) {
    static_assert(std::is_same<typename std::underlying_type<Message>::type, uint32_t>::value,
            "Given message type is not of the correct type");
    LOG_ASSERT(crossbow::to_underlying(messageType) != std::numeric_limits<uint32_t>::max(), "Invalid message tupe");

    return writeInternalResponse(messageId, crossbow::to_underlying(messageType), length, std::move(fun));
}

template <typename Manager, typename Socket>
template <typename Error>
bool RpcServerSocket<Manager, Socket>::writeErrorResponse(MessageId messageId, Error error) {
    static_assert(std::is_error_code_enum<Error>::value, "Error code must be an error code enum");

    uint32_t messageLength = sizeof(uint64_t);
    return writeInternalResponse(messageId, std::numeric_limits<uint32_t>::max(), messageLength, [error]
            (crossbow::buffer_writer& message, std::error_code& /* ec */) {
        message.write<uint64_t>(static_cast<uint64_t>(error));
    });
}

template <typename Manager, typename Socket>
template <typename Fun>
bool RpcServerSocket<Manager, Socket>::writeInternalResponse(MessageId messageId, uint32_t messageType,
        uint32_t length, Fun fun) {
    std::error_code ec;
    this->writeMessage(messageId, messageType, length, std::move(fun), ec);
    if (ec) {
        // TODO Try to recover from errors
        this->handleSocketError(ec);
        return false;
    }
    return true;
}

template <typename Manager, typename Socket>
void RpcServerSocket<Manager, Socket>::onSocketConnected(const string& data) {
    LOG_TRACE("Connection established");
}

template <typename Manager, typename Socket>
void RpcServerSocket<Manager, Socket>::onSocketDisconnected() {
    auto manager = static_cast<RpcServerManager<Manager, Socket>*>(&mManager);
    auto self = static_cast<Socket*>(this);
    this->mSocket->processor()->executeLocal([self, manager] () {
        manager->removeConnection(self);
    });
}

} // namespace infinio
} // namespace crossbow
