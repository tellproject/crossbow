#include <crossbow/infinio/InfinibandSocket.hpp>

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

void InfinibandBaseSocket::open(std::error_code& ec) {
    if (mId != nullptr) {
        ec = error::already_open;
        return;
    }

    SOCKET_LOG("Open socket");
    errno = 0;
    if (rdma_create_id(mChannel, &mId, mContext, RDMA_PS_TCP) != 0) {
        ec = std::error_code(errno, std::system_category());
        return;
    }
    // TODO Log assert mId != nullptr
}

void InfinibandBaseSocket::close(std::error_code& ec) {
    if (mId == nullptr) {
        ec = error::bad_descriptor;
        return;
    }

    SOCKET_LOG("Close socket");
    errno = 0;
    if (rdma_destroy_id(mId) != 0) {
        ec = std::error_code(errno, std::system_category());
        return;
    }
    mId = nullptr;
}

void InfinibandBaseSocket::bind(Endpoint& addr, std::error_code& ec) {
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

InfinibandSocket* ConnectionRequest::accept(std::error_code& ec) {
    mSocket->accept(ec);
    if (ec) {
        SOCKET_LOG("%1%: Accepting connection failed [error = %2% %3%]", formatRemoteAddress(mSocket->mId), ec,
                ec.message());

        std::error_code ec2;
        mSocket->close(ec2);
        if (ec2) {
            SOCKET_ERROR("%1%: Destroying failed connection failed [error = %2% %3%]",
                    formatRemoteAddress(mSocket->mId), ec2, ec2.message());
        }
        mSocket.reset();
        return nullptr;
    }
    return mSocket.release();
}

void ConnectionRequest::reject() {
    std::error_code ec;
    mSocket->reject(ec);
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

void InfinibandAcceptor::listen(int backlog, std::error_code& ec) {
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

void InfinibandSocketHandler::onConnected(const std::error_code& ec) {
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

void InfinibandSocket::connect(Endpoint& addr, std::error_code& ec) {
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

void InfinibandSocket::disconnect(std::error_code& ec) {
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

void InfinibandSocket::send(InfinibandBuffer& buffer, uint32_t userId, std::error_code& ec) {
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

void InfinibandSocket::read(const RemoteMemoryRegion& src, size_t offset, InfinibandBuffer& dst, uint32_t userId,
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

void InfinibandSocket::write(InfinibandBuffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
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

uint32_t InfinibandSocket::bufferLength() const {
    return mContext->bufferLength();
}

InfinibandBuffer InfinibandSocket::acquireSendBuffer() {
    return mContext->acquireSendBuffer();
}

InfinibandBuffer InfinibandSocket::acquireSendBuffer(uint32_t length) {
    return mContext->acquireSendBuffer(length);
}

void InfinibandSocket::releaseSendBuffer(InfinibandBuffer& buffer) {
    mContext->releaseSendBuffer(buffer);
}

void InfinibandSocket::accept(std::error_code& ec) {
    SOCKET_LOG("%1%: Accepting connection", formatRemoteAddress(mId));

    // Add connection to queue handler
    mContext->addConnection(this, ec);
    if (ec) {
        return;
    }

    struct rdma_conn_param cm_params;
    memset(&cm_params, 0, sizeof(cm_params));

    errno = 0;
    if (rdma_accept(mId, &cm_params) != 0) {
        auto res = errno;

        mContext->removeConnection(this, ec);
        if (ec) {
            // There is nothing we can do if removing the incomplete connection failed
            SOCKET_ERROR("%1%: Removing invalid connection failed [error = %2% %3%]", formatRemoteAddress(mId), ec,
                    ec.message());
        }

        ec = std::error_code(res, std::system_category());
        return;
    }
}

void InfinibandSocket::reject(std::error_code& ec) {
    SOCKET_LOG("%1%: Rejecting connection", formatRemoteAddress(mId));

    errno = 0;
    if (rdma_reject(mId, nullptr, 0) != 0) {
        ec = std::error_code(errno, std::system_category());
    }
}

void InfinibandSocket::doSend(struct ibv_send_wr* wr, std::error_code& ec) {
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

void InfinibandSocket::onAddressResolved() {
    SOCKET_LOG("%1%: Address resolved", formatRemoteAddress(mId));
    errno = 0;
    if (rdma_resolve_route(mId, gTimeout.count()) != 0) {
        mHandler->onConnected(std::error_code(errno, std::system_category()));
        return;
    }
}

void InfinibandSocket::onRouteResolved() {
    SOCKET_LOG("%1%: Route resolved", formatRemoteAddress(mId));

    // Add connection to queue handler
    std::error_code ec;
    mContext->addConnection(this, ec);
    if (ec) {
        mHandler->onConnected(ec);
        return;
    }

    struct rdma_conn_param cm_params;
    memset(&cm_params, 0, sizeof(cm_params));

    errno = 0;
    if (rdma_connect(mId, &cm_params) != 0) {
        auto res = errno;

        mContext->removeConnection(this, ec);
        if (ec) {
            // There is nothing we can do if removing the incomplete connection failed
            SOCKET_ERROR("%1%: Remove of invalid connection failed [error = %2% %3%]", formatRemoteAddress(mId), ec,
                    ec.message());
        }

        mHandler->onConnected(std::error_code(res, std::system_category()));
    }
}

void InfinibandSocket::onConnectionError(error::network_errors err) {
    std::error_code ec;
    mContext->removeConnection(this, ec);
    if (ec) {
        // There is nothing we can do if removing the incomplete connection failed
        SOCKET_ERROR("%1%: Removing invalid connection failed [error = %2% %3%]", formatRemoteAddress(mId), ec,
                ec.message());
    }

    mHandler->onConnected(err);
}

void InfinibandSocket::onTimewaitExit() {
    std::error_code ec;
    mContext->drainConnection(this, ec);
    if (ec) {
        // TODO How to handle this?
        SOCKET_ERROR("%1%: Draining connection failed [error = %2% %3%]", formatRemoteAddress(mId), ec, ec.message());
    }
}

void InfinibandSocket::removeWork() {
    auto work = --mWork;
    if (work == 0) {
        std::error_code ec;
        mContext->removeConnection(this, ec);
        if (ec) {
            // TODO Error
        }

        work = mWork.exchange(0x1u);
        if (work != 0) {
            SOCKET_ERROR("%1%: Work count reached zero and was incremented again");
        }
        mHandler->onDisconnected();
    }
}

} // namespace infinio
} // namespace crossbow
