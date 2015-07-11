#include <crossbow/infinio/InfinibandSocket.hpp>

#include <crossbow/infinio/InfinibandService.hpp>
#include <crossbow/logger.hpp>

#include "AddressHelper.hpp"
#include "DeviceContext.hpp"
#include "WorkRequestId.hpp"

#include <cerrno>

namespace crossbow {
namespace infinio {

namespace {

/**
 * @brief Timeout value for resolving addresses and routes
 */
constexpr std::chrono::milliseconds gTimeout = std::chrono::milliseconds(10);

} // anonymous namespace

template <typename SocketType>
void InfinibandBaseSocket<SocketType>::open() {
    if (mId != nullptr) {
        throw std::system_error(error::already_open);
    }

    LOG_TRACE("Open socket");
    if (rdma_create_id(mChannel, &mId, static_cast<SocketType*>(this), RDMA_PS_TCP)) {
        throw std::system_error(errno, std::system_category());
    }

    // Increment the reference count by one because we assigned it as context
    intrusive_ptr_add_ref(this);

    assert(mId != nullptr);
}

template <typename SocketType>
void InfinibandBaseSocket<SocketType>::close() {
    if (mId == nullptr) {
        return;
    }

    if (mId->qp != nullptr) {
        throw std::system_error(EISCONN, std::system_category());
    }

    LOG_TRACE("Close socket");
    if (rdma_destroy_id(mId)) {
        throw std::system_error(errno, std::system_category());
    }
    mId = nullptr;

    // Decrement the reference count
    intrusive_ptr_release(this);
}

template <typename SocketType>
void InfinibandBaseSocket<SocketType>::bind(Endpoint& addr) {
    if (mId == nullptr) {
        throw std::system_error(EBADF, std::system_category());
    }

    LOG_TRACE("Bind on address %1%", addr);
    if (rdma_bind_addr(mId, addr.handle())) {
        throw std::system_error(errno, std::system_category());
    }
}

InfinibandAcceptorHandler::~InfinibandAcceptorHandler() {
}

void InfinibandAcceptorHandler::onConnection(InfinibandSocket socket, const crossbow::string& data) {
    // Reject by default
    try {
        socket->reject(crossbow::string());
    } catch (std::system_error& e) {
        LOG_ERROR("%1%: Rejecting connection failed [error = %2% %3%]", socket->remoteAddress(), e.code(), e.what());
    }
    try {
        socket->close();
    } catch (std::system_error& e) {
        LOG_ERROR("%1%: Closing rejected connection failed [error = %2% %3%]", socket->remoteAddress(), e.code(),
                        e.what());
    }
}

void InfinibandAcceptorImpl::listen(int backlog) {
    if (mId == nullptr) {
        throw std::system_error(EBADF, std::system_category());
    }

    LOG_TRACE("Listen on socket with backlog %1%", backlog);
    if (rdma_listen(mId, backlog)) {
        throw std::system_error(errno, std::system_category());
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

void InfinibandSocketHandler::onRead(uint32_t userId, uint16_t bufferId, const std::error_code& ec) {
    // Empty default function
}

void InfinibandSocketHandler::onWrite(uint32_t userId, uint16_t bufferId, const std::error_code& ec) {
    // Empty default function
}

void InfinibandSocketHandler::onImmediate(uint32_t data) {
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

void InfinibandSocketImpl::connect(Endpoint& addr) {
    if (mId == nullptr) {
        throw std::system_error(EBADF, std::system_category());
    }

    LOG_TRACE("%1%: Connect to address", addr);
    if (rdma_resolve_addr(mId, nullptr, addr.handle(), gTimeout.count())) {
        throw std::system_error(errno, std::system_category());
    }
}

void InfinibandSocketImpl::connect(Endpoint& addr, const crossbow::string& data) {
    mData = data;
    connect(addr);
}

void InfinibandSocketImpl::disconnect() {
    if (mId == nullptr) {
        throw std::system_error(EBADF, std::system_category());
    }

    LOG_TRACE("%1%: Disconnect from address", formatRemoteAddress(mId));
    if (rdma_disconnect(mId)) {
        throw std::system_error(errno, std::system_category());
    }
}

void InfinibandSocketImpl::accept(const crossbow::string& data, uint64_t thread) {
    LOG_TRACE("%1%: Accepting connection", formatRemoteAddress(mId));
    if (mContext != nullptr) {
        throw std::system_error(error::already_initialized);
    }
    mContext = mService.context(thread);

    // Add connection to queue handler
    mContext->addConnection(mId, this);

    struct rdma_conn_param cm_params;
    memset(&cm_params, 0, sizeof(cm_params));
    cm_params.private_data = data.c_str();
    cm_params.private_data_len = data.length();

    if (rdma_accept(mId, &cm_params)) {
        auto res = errno;
        mContext->removeConnection(mId);
        throw std::system_error(res, std::system_category());
    }
}

void InfinibandSocketImpl::reject(const crossbow::string& data) {
    LOG_TRACE("%1%: Rejecting connection", formatRemoteAddress(mId));
    if (rdma_reject(mId, data.c_str(), data.length())) {
        throw std::system_error(errno, std::system_category());
    }
}

Endpoint InfinibandSocketImpl::remoteAddress() const {
    return Endpoint(rdma_get_peer_addr(mId));
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

    LOG_TRACE("%1%: Send %2% bytes from buffer %3%", formatRemoteAddress(mId), buffer.length(), buffer.id());
    doSend(&wr, ec);
}

void InfinibandSocketImpl::read(const RemoteMemoryRegion& src, size_t offset, InfinibandBuffer& dst, uint32_t userId,
        std::error_code& ec) {
    doRead(src, offset, dst, userId, IBV_SEND_SIGNALED, ec);
}

void InfinibandSocketImpl::read(const RemoteMemoryRegion& src, size_t offset, ScatterGatherBuffer& dst, uint32_t userId,
        std::error_code& ec) {
    doRead(src, offset, dst, userId, IBV_SEND_SIGNALED, ec);
}

void InfinibandSocketImpl::readUnsignaled(const RemoteMemoryRegion& src, size_t offset, InfinibandBuffer& dst,
        uint32_t userId, std::error_code& ec) {
    doRead(src, offset, dst, userId, 0x0u, ec);
}

void InfinibandSocketImpl::readUnsignaled(const RemoteMemoryRegion& src, size_t offset, ScatterGatherBuffer& dst,
        uint32_t userId, std::error_code& ec) {
    doRead(src, offset, dst, userId, 0x0u, ec);
}

void InfinibandSocketImpl::write(InfinibandBuffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
        std::error_code& ec) {
    doWrite(src, dst, offset, userId, IBV_SEND_SIGNALED, ec);
}

void InfinibandSocketImpl::write(ScatterGatherBuffer& src, const RemoteMemoryRegion& dst, size_t offset,
        uint32_t userId, std::error_code& ec) {
    doWrite(src, dst, offset, userId, IBV_SEND_SIGNALED, ec);
}

void InfinibandSocketImpl::write(InfinibandBuffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
        uint32_t immediate, std::error_code& ec) {
    doWrite(src, dst, offset, userId, immediate, IBV_SEND_SIGNALED, ec);
}

void InfinibandSocketImpl::write(ScatterGatherBuffer& src, const RemoteMemoryRegion& dst, size_t offset,
        uint32_t userId, uint32_t immediate, std::error_code& ec) {
    doWrite(src, dst, offset, userId, immediate, IBV_SEND_SIGNALED, ec);
}

void InfinibandSocketImpl::writeUnsignaled(InfinibandBuffer& src, const RemoteMemoryRegion& dst, size_t offset,
        uint32_t userId, std::error_code& ec) {
    doWrite(src, dst, offset, userId, 0x0u, ec);
}

void InfinibandSocketImpl::writeUnsignaled(ScatterGatherBuffer& src, const RemoteMemoryRegion& dst, size_t offset,
        uint32_t userId, std::error_code& ec) {
    doWrite(src, dst, offset, userId, 0x0u, ec);
}

void InfinibandSocketImpl::writeUnsignaled(InfinibandBuffer& src, const RemoteMemoryRegion& dst, size_t offset,
        uint32_t userId, uint32_t immediate, std::error_code& ec) {
    doWrite(src, dst, offset, userId, immediate, 0x0u, ec);
}

void InfinibandSocketImpl::writeUnsignaled(ScatterGatherBuffer& src, const RemoteMemoryRegion& dst, size_t offset,
        uint32_t userId, uint32_t immediate, std::error_code& ec) {
    doWrite(src, dst, offset, userId, immediate, 0x0u, ec);
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

void InfinibandSocketImpl::doSend(struct ibv_send_wr* wr, std::error_code& ec) {
    if (mId == nullptr) {
        ec = std::error_code(EBADF, std::system_category());
        return;
    }

    struct ibv_send_wr* bad_wr = nullptr;
    if (auto res = ibv_post_send(mId->qp, wr, &bad_wr)) {
        ec = std::error_code(res, std::system_category());
        return;
    }
}

template <typename Buffer>
void InfinibandSocketImpl::doRead(const RemoteMemoryRegion& src, size_t offset, Buffer& dst, uint32_t userId, int flags,
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
    wr.num_sge = dst.count();
    wr.send_flags = flags;
    wr.wr.rdma.remote_addr = src.address();
    wr.wr.rdma.rkey = src.key();

    LOG_TRACE("%1%: RDMA read %2% bytes from remote %3% into %4% buffer with ID %5%", formatRemoteAddress(mId),
            dst.length(), reinterpret_cast<void*>(src.address()), dst.count(), dst.id());
    doSend(&wr, ec);
}

template <typename Buffer>
void InfinibandSocketImpl::doWrite(Buffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
        int flags, std::error_code& ec) {
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
    wr.num_sge = src.count();
    wr.send_flags = flags;
    wr.wr.rdma.remote_addr = dst.address();
    wr.wr.rdma.rkey = dst.key();

    LOG_TRACE("%1%: RDMA write %2% bytes to remote %3% from %4% buffer with ID %5%", formatRemoteAddress(mId),
            src.length(), reinterpret_cast<void*>(dst.address()), src.count(), src.id());
    doSend(&wr, ec);
}

template <typename Buffer>
void InfinibandSocketImpl::doWrite(Buffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
        uint32_t immediate, int flags, std::error_code& ec) {
    if (offset +  src.length() > dst.length()) {
        ec = error::out_of_range;
        return;
    }

    WorkRequestId workId(userId, src.id(), WorkType::WRITE);

    struct ibv_send_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.opcode = IBV_WR_RDMA_WRITE_WITH_IMM;
    wr.wr_id = workId.id();
    wr.sg_list = src.handle();
    wr.num_sge = src.count();
    wr.imm_data = htonl(immediate);
    wr.send_flags = flags;
    wr.wr.rdma.remote_addr = dst.address();
    wr.wr.rdma.rkey = dst.key();

    LOG_TRACE("%1%: RDMA write with immediate %2% bytes to remote %3% from %4% buffer with ID %5%",
            formatRemoteAddress(mId), src.length(), reinterpret_cast<void*>(dst.address()), src.count(), src.id());
    doSend(&wr, ec);
}

void InfinibandSocketImpl::onAddressResolved() {
    LOG_TRACE("%1%: Address resolved", formatRemoteAddress(mId));
    if (rdma_resolve_route(mId, gTimeout.count())) {
        onConnectionEvent(mData, std::error_code(errno, std::system_category()));
        return;
    }
}

void InfinibandSocketImpl::onRouteResolved() {
    LOG_TRACE("%1%: Route resolved", formatRemoteAddress(mId));

    // Add connection to queue handler
    try {
        mContext->addConnection(mId, this);
    } catch (std::system_error& e) {
        onConnectionEvent(mData, e.code());
        return;
    }

    struct rdma_conn_param cm_params;
    memset(&cm_params, 0, sizeof(cm_params));
    cm_params.private_data = mData.c_str();
    cm_params.private_data_len = mData.length();

    if (rdma_connect(mId, &cm_params)) {
        std::error_code ec(errno, std::system_category());
        mContext->removeConnection(mId);
        onConnectionEvent(mData, ec);
    }
}

void InfinibandSocketImpl::onConnectionError(error::network_errors err) {
    mContext->removeConnection(mId);
    onConnectionEvent(mData, err);
}

void InfinibandSocketImpl::onConnectionRejected(const crossbow::string& data) {
    mContext->removeConnection(mId);
    onConnectionEvent(data, std::error_code(ECONNREFUSED, std::system_category()));
}

void InfinibandSocketImpl::onTimewaitExit() {
    mContext->drainConnection(this);
}

void InfinibandSocketImpl::onDrained() {
    mContext->removeConnection(mId);
    mHandler->onDisconnected();
}

template class InfinibandBaseSocket<InfinibandAcceptorImpl>;
template class InfinibandBaseSocket<InfinibandSocketImpl>;

} // namespace infinio
} // namespace crossbow
