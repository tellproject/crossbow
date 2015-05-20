#include <crossbow/infinio/InfinibandSocket.hpp>

#include <crossbow/infinio/InfinibandService.hpp>

#include "DeviceContext.hpp"
#include "ErrorCode.hpp"
#include "InfinibandSocketImpl.hpp"

namespace crossbow {
namespace infinio {

void InfinibandBaseSocket::open(std::error_code& ec) {
    if (mImpl) {
        ec = error::already_open;
        return;
    }

    mImpl = new SocketImplementation();
    mTransport.open(mImpl, ec);
}

void InfinibandBaseSocket::close(std::error_code& ec) {
    if (!mImpl) {
        ec = error::bad_descriptor;
        return;
    }

    mTransport.close(mImpl, ec);
    if (ec) {
        return;
    }

    delete mImpl;
    mImpl = nullptr;
}

void InfinibandBaseSocket::bind(const Endpoint& addr, std::error_code& ec) {
    mTransport.bind(mImpl, addr, ec);
}

void InfinibandBaseSocket::setHandler(InfinibandSocketHandler* handler) {
    mImpl->handler = handler;
}

void InfinibandAcceptor::listen(int backlog, std::error_code& ec) {
    mTransport.listen(mImpl, backlog, ec);
}

void InfinibandSocket::connect(const Endpoint& addr, std::error_code& ec) {
    mTransport.connect(mImpl, addr, ec);
}

void InfinibandSocket::disconnect(std::error_code& ec) {
    mTransport.disconnect(mImpl, ec);
}

void InfinibandSocket::send(InfinibandBuffer& buffer, uint32_t userId, std::error_code& ec) {
    mTransport.send(mImpl, buffer, userId, ec);
}

void InfinibandSocket::read(const RemoteMemoryRegion& src, size_t offset, InfinibandBuffer& dst, uint32_t userId,
        std::error_code& ec) {
    mTransport.read(mImpl, src, offset, dst, userId, ec);
}

void InfinibandSocket::write(InfinibandBuffer& src, const RemoteMemoryRegion& dst, size_t offset, uint32_t userId,
        std::error_code& ec) {
    mTransport.write(mImpl, src, dst, offset, userId, ec);
}

uint32_t InfinibandSocket::bufferLength() const {
    return mImpl->device->bufferLength();
}

InfinibandBuffer InfinibandSocket::acquireSendBuffer() {
    return mImpl->device->acquireSendBuffer();
}

InfinibandBuffer InfinibandSocket::acquireSendBuffer(uint32_t length) {
    return mImpl->device->acquireSendBuffer(length);
}

void InfinibandSocket::releaseSendBuffer(InfinibandBuffer& buffer) {
    mImpl->device->releaseSendBuffer(buffer);
}

InfinibandSocketHandler::~InfinibandSocketHandler() {
}

bool InfinibandSocketHandler::onConnection(InfinibandSocket socket) {
    // Empty default function
    return false;
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

} // namespace infinio
} // namespace crossbow
