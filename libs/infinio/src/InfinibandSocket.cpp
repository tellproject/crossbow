#include <crossbow/infinio/InfinibandSocket.hpp>

#include <crossbow/infinio/InfinibandService.hpp>

#include "DeviceContext.hpp"
#include "ErrorCode.hpp"
#include "InfinibandSocketImpl.hpp"

namespace crossbow {
namespace infinio {

void InfinibandBaseSocket::open(boost::system::error_code& ec) {
    if (mImpl) {
        ec = error::already_open;
        return;
    }

    mImpl = new SocketImplementation();
    mTransport.open(mImpl, ec);
}

void InfinibandBaseSocket::close(boost::system::error_code& ec) {
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

void InfinibandBaseSocket::bind(const Endpoint& addr, boost::system::error_code& ec) {
    mTransport.bind(mImpl, addr, ec);
}

void InfinibandBaseSocket::setHandler(InfinibandSocketHandler* handler) {
    mImpl->handler = handler;
}

void InfinibandAcceptor::listen(int backlog, boost::system::error_code& ec) {
    mTransport.listen(mImpl, backlog, ec);
}

void InfinibandSocket::connect(const Endpoint& addr, boost::system::error_code& ec) {
    mTransport.connect(mImpl, addr, ec);
}

void InfinibandSocket::disconnect(boost::system::error_code& ec) {
    mTransport.disconnect(mImpl, ec);
}

void InfinibandSocket::send(InfinibandBuffer& buffer, uint32_t userId, boost::system::error_code& ec) {
    mTransport.send(mImpl, buffer, userId, ec);
}

uint32_t InfinibandSocket::bufferLength() const {
    return mImpl->device->bufferLength();
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

void InfinibandSocketHandler::onConnected(const boost::system::error_code& ec) {
    // Empty default function
}

void InfinibandSocketHandler::onReceive(const void* buffer, size_t length, const boost::system::error_code& ec) {
    // Empty default function
}

void InfinibandSocketHandler::onSend(uint32_t userId, const boost::system::error_code& ec) {
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
