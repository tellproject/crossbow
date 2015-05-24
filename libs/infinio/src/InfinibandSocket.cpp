#include <crossbow/infinio/InfinibandSocket.hpp>

#include <crossbow/infinio/InfinibandService.hpp>

#include "AddressHelper.hpp"
#include "DeviceContext.hpp"
#include "Logging.hpp"
#include "WorkRequestId.hpp"

#define SOCKET_LOG(...) INFINIO_LOG("[InfinibandSocket] " __VA_ARGS__)
#define SOCKET_ERROR(...) INFINIO_ERROR("[InfinibandSocket] " __VA_ARGS__)

namespace crossbow {
namespace infinio {

namespace {

/**
 * @brief Timeout value for resolving addresses and routes
 */
constexpr std::chrono::milliseconds gTimeout = std::chrono::milliseconds(10);

} // anonymous namespace

template <typename SocketType>
void InfinibandBaseSocket<SocketType>::open(std::error_code& ec) {
    if (mId != nullptr) {
        ec = error::already_open;
        return;
    }

    SOCKET_LOG("Open socket");

    // Increment the reference count by one and then detach the pointer
    boost::intrusive_ptr<SocketType> ptr(static_cast<SocketType*>(this));

    errno = 0;
    if (rdma_create_id(mChannel, &mId, ptr.detach(), RDMA_PS_TCP) != 0) {
        ec = std::error_code(errno, std::system_category());

        // Open failed, attach the pointer back without incrementing the counter so it gets decremented on destruction
        ptr.reset(static_cast<SocketType*>(this), false);

        return;
    }
    // TODO Log assert mId != nullptr
}

template <typename SocketType>
void InfinibandBaseSocket<SocketType>::close(std::error_code& ec) {
    if (mId == nullptr) {
        ec = error::bad_descriptor;
        return;
    }

    SOCKET_LOG("Close socket");

    // Attach the pointer back without incrementing the counter
    boost::intrusive_ptr<SocketType> ptr(static_cast<SocketType*>(this), false);

    errno = 0;
    if (rdma_destroy_id(mId) != 0) {
        ec = std::error_code(errno, std::system_category());

        // Close failed, detach the pointer again
        ptr.detach();

        return;
    }
    mId = nullptr;
}

template <typename SocketType>
void InfinibandBaseSocket<SocketType>::bind(Endpoint& addr, std::error_code& ec) {
    if (mId == nullptr) {
        ec = error::bad_descriptor;
        return;
    }

    SOCKET_LOG("Bind on address %1%", addr);
    errno = 0;
    if (rdma_bind_addr(mId, addr.handle()) != 0) {
        ec = std::error_code(errno, std::system_category());
        return;
    }
}

ConnectionRequest::~ConnectionRequest() {
    if (mSocket) {
        reject();
    }
}

InfinibandSocket ConnectionRequest::accept(const crossbow::string& data, std::error_code& ec, uint64_t thread) {
    mSocket->accept(mService.context(thread), data, ec);
    if (ec) {
        SOCKET_LOG("%1%: Accepting connection failed [error = %2% %3%]", formatRemoteAddress(mSocket->mId), ec,
                ec.message());

        std::error_code ec2;
        mSocket->close(ec2);
        if (ec2) {
            SOCKET_ERROR("%1%: Destroying failed connection failed [error = %2% %3%]",
                    formatRemoteAddress(mSocket->mId), ec2, ec2.message());
        }
    }
    return std::move(mSocket);
}

void ConnectionRequest::reject(const crossbow::string& data) {
    std::error_code ec;
    mSocket->reject(data, ec);
    if (ec) {
        SOCKET_ERROR("%1%: Rejecting connection failed [error = %2% %3%]", formatRemoteAddress(mSocket->mId), ec,
                ec.message());
    }
    mSocket->close(ec);
    if (ec) {
        SOCKET_ERROR("%1%: Closing rejected connection failed [error = %2% %3%]", formatRemoteAddress(mSocket->mId), ec,
                ec.message());
    }
    mSocket.reset();
}

InfinibandAcceptorHandler::~InfinibandAcceptorHandler() {
}

void InfinibandAcceptorHandler::onConnection(ConnectionRequest request) {
    // Reject by default
    request.reject();
}

void InfinibandAcceptorImpl::listen(int backlog, std::error_code& ec) {
    if (mId == nullptr) {
        ec = error::bad_descriptor;
        return;
    }

    SOCKET_LOG("Listen on socket with backlog %1%", backlog);
    errno = 0;
    if (rdma_listen(mId, backlog) != 0) {
        ec = std::error_code(errno, std::system_category());
        return;
    }
}

InfinibandSocketHandler::~InfinibandSocketHandler() {
}

void InfinibandSocketHandler::onConnected(const string& data, const std::error_code& ec) {
    // Empty default function
}

void InfinibandSocketHandler::onReceive(const void* buffer, size_t length, const std::error_code& ec) {
    // Empty default function
}

void InfinibandSocketHandler::onSend(uint32_t userId, const std::error_code& ec) {
    // Empty default function
}

void InfinibandSocketHandler::onRead(uint32_t userId, const std::error_code& ec) {
    // Empty default function
}

void InfinibandSocketHandler::onWrite(uint32_t userId, const std::error_code& ec) {
    // Empty default function
}

void InfinibandSocketHandler::onDisconnect() {
    // Empty default function
}

void InfinibandSocketHandler::onDisconnected() {
    // Empty default function
}

void InfinibandSocketImpl::execute(std::function<void()> fun, std::error_code& ec) {
    mContext->execute(std::move(fun), ec);
}

void InfinibandSocketImpl::connect(Endpoint& addr, std::error_code& ec) {
    if (mId == nullptr) {
        ec = error::bad_descriptor;
        return;
    }

    SOCKET_LOG("%1%: Connect to address", addr);
    errno = 0;
    if (rdma_resolve_addr(mId, nullptr, addr.handle(), gTimeout.count()) != 0) {
        ec = std::error_code(errno, std::system_category());
        return;
    }
}

void InfinibandSocketImpl::connect(Endpoint& addr, const crossbow::string& data, std::error_code& ec) {
    mData = data;
    connect(addr, ec);
}

void InfinibandSocketImpl::disconnect(std::error_code& ec) {
    if (mId == nullptr) {
        ec = error::bad_descriptor;
        return;
    }

    SOCKET_LOG("%1%: Disconnect from address", formatRemoteAddress(mId));
    errno = 0;
    if (rdma_disconnect(mId) != 0) {
        ec = std::error_code(errno, std::system_category());
        return;
    }
}

void InfinibandSocketImpl::send(InfinibandBuffer& buffer, uint32_t userId, std::error_code& ec) {
    WorkRequestId workId(userId, buffer.id(), WorkType::SEND);

    struct ibv_send_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.opcode = IBV_WR_SEND;
    wr.wr_id = workId.id();
    wr.sg_list = buffer.handle();
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;

    SOCKET_LOG("%1%: Send %2% bytes from buffer %3%", formatRemoteAddress(mId), buffer.length(), buffer.id());
    doSend(&wr, ec);
}

void InfinibandSocketImpl::read(const RemoteMemoryRegion& src, size_t offset, InfinibandBuffer& dst, uint32_t userId,
        std::error_code& ec) {
    if (offset +  dst.length() > src.length()) {
        ec = error::out_of_range;
        return;
    }

    WorkRequestId workId(userId, dst.id(), WorkType::READ);

    struct ibv_send_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.opcode = IBV_WR_RDMA_READ;
    wr.wr_id = workId.id();
    wr.sg_list = dst.handle();
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.rdma.remote_addr = src.address();
    wr.wr.rdma.rkey = src.key();

    SOCKET_LOG("%1%: RDMA read %2% bytes from remote %3% into buffer %4%", formatRemoteAddress(mId), dst.length(),
            reinterpret_cast<void*>(src.address()), dst.id());
    doSend(&wr, ec);
}

void InfinibandSocketImpl::write(InfinibandBuffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
        std::error_code& ec) {
    if (offset +  src.length() > dst.length()) {
        ec = error::out_of_range;
        return;
    }

    WorkRequestId workId(userId, src.id(), WorkType::WRITE);

    struct ibv_send_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.opcode = IBV_WR_RDMA_WRITE;
    wr.wr_id = workId.id();
    wr.sg_list = src.handle();
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;
    wr.wr.rdma.remote_addr = dst.address();
    wr.wr.rdma.rkey = dst.key();

    SOCKET_LOG("%1%: RDMA write %2% bytes to remote %3% from buffer %4%", formatRemoteAddress(mId), src.length(),
            reinterpret_cast<void*>(dst.address()), src.id());
    doSend(&wr, ec);
}

uint32_t InfinibandSocketImpl::bufferLength() const {
    return mContext->bufferLength();
}

InfinibandBuffer InfinibandSocketImpl::acquireSendBuffer() {
    return mContext->acquireSendBuffer();
}

InfinibandBuffer InfinibandSocketImpl::acquireSendBuffer(uint32_t length) {
    return mContext->acquireSendBuffer(length);
}

void InfinibandSocketImpl::releaseSendBuffer(InfinibandBuffer& buffer) {
    mContext->releaseSendBuffer(buffer);
}

void InfinibandSocketImpl::accept(CompletionContext* context, const string& data, std::error_code& ec) {
    SOCKET_LOG("%1%: Accepting connection", formatRemoteAddress(mId));
    if (mContext != nullptr) {
        ec = error::already_initialized;
    }
    mContext = context;

    // Add connection to queue handler
    mContext->addConnection(mId, this, ec);
    if (ec) {
        return;
    }

    struct rdma_conn_param cm_params;
    memset(&cm_params, 0, sizeof(cm_params));
    cm_params.private_data = data.c_str();
    cm_params.private_data_len = data.length();

    errno = 0;
    if (rdma_accept(mId, &cm_params) != 0) {
        auto res = errno;

        mContext->removeConnection(mId, ec);
        if (ec) {
            // There is nothing we can do if removing the incomplete connection failed
            SOCKET_ERROR("%1%: Removing invalid connection failed [error = %2% %3%]", formatRemoteAddress(mId), ec,
                    ec.message());
        }

        ec = std::error_code(res, std::system_category());
        return;
    }
}

void InfinibandSocketImpl::reject(const crossbow::string& data, std::error_code& ec) {
    SOCKET_LOG("%1%: Rejecting connection", formatRemoteAddress(mId));

    errno = 0;
    if (rdma_reject(mId, data.c_str(), data.length()) != 0) {
        ec = std::error_code(errno, std::system_category());
    }
}

void InfinibandSocketImpl::doSend(struct ibv_send_wr* wr, std::error_code& ec) {
    if (mId == nullptr) {
        ec = error::bad_descriptor;
        return;
    }

    struct ibv_send_wr* bad_wr = nullptr;
    if (auto res = ibv_post_send(mId->qp, wr, &bad_wr) != 0) {
        ec = std::error_code(res, std::system_category());
        return;
    }
}

void InfinibandSocketImpl::onAddressResolved() {
    SOCKET_LOG("%1%: Address resolved", formatRemoteAddress(mId));
    errno = 0;
    if (rdma_resolve_route(mId, gTimeout.count()) != 0) {
        mData.clear();
        mHandler->onConnected(mData, std::error_code(errno, std::system_category()));
        return;
    }
}

void InfinibandSocketImpl::onRouteResolved() {
    SOCKET_LOG("%1%: Route resolved", formatRemoteAddress(mId));

    // Add connection to queue handler
    std::error_code ec;
    mContext->addConnection(mId, this, ec);
    if (ec) {
        mData.clear();
        mHandler->onConnected(mData, ec);
        return;
    }

    struct rdma_conn_param cm_params;
    memset(&cm_params, 0, sizeof(cm_params));
    cm_params.private_data = mData.c_str();
    cm_params.private_data_len = mData.length();

    errno = 0;
    if (rdma_connect(mId, &cm_params) != 0) {
        auto res = errno;

        mContext->removeConnection(mId, ec);
        if (ec) {
            // There is nothing we can do if removing the incomplete connection failed
            SOCKET_ERROR("%1%: Remove of invalid connection failed [error = %2% %3%]", formatRemoteAddress(mId), ec,
                    ec.message());
        }

        mData.clear();
        mHandler->onConnected(mData, std::error_code(res, std::system_category()));
    }
}

void InfinibandSocketImpl::onResolutionError(error::network_errors err) {
    mData.clear();
    mHandler->onConnected(mData, err);
}

void InfinibandSocketImpl::onConnectionError(error::network_errors err) {
    std::error_code ec;
    mContext->removeConnection(mId, ec);
    if (ec) {
        // There is nothing we can do if removing the incomplete connection failed
        SOCKET_ERROR("%1%: Removing invalid connection failed [error = %2% %3%]", formatRemoteAddress(mId), ec,
                ec.message());
    }

    mData.clear();
    mHandler->onConnected(mData, err);
}

void InfinibandSocketImpl::onConnectionRejected(const crossbow::string& data) {
    std::error_code ec;
    mContext->removeConnection(mId, ec);
    if (ec) {
        // There is nothing we can do if removing the rejected connection failed
        SOCKET_ERROR("%1%: Removing rejected connection failed [error = %2% %3%]", formatRemoteAddress(mId), ec,
                ec.message());
    }

    mData.clear();
    mHandler->onConnected(data, error::connection_rejected);
}

void InfinibandSocketImpl::onConnectionEstablished(const crossbow::string& data) {
    mData.clear();
    mHandler->onConnected(data, std::error_code());
}

void InfinibandSocketImpl::onTimewaitExit() {
    mContext->drainConnection(this);
}

void InfinibandSocketImpl::onDrained() {
    std::error_code ec;
    mContext->removeConnection(mId, ec);
    if (ec) {
        SOCKET_ERROR("%1%: Removing drained connection failed [error = %2% %3%]", formatRemoteAddress(mId), ec,
                ec.message());
    }
    mHandler->onDisconnected();
}

template class InfinibandBaseSocket<InfinibandAcceptorImpl>;
template class InfinibandBaseSocket<InfinibandSocketImpl>;

} // namespace infinio
} // namespace crossbow
