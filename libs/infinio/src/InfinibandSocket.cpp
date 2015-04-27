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

void InfinibandBaseSocket::bind(struct sockaddr* addr, boost::system::error_code& ec) {
    mTransport.bind(mImpl, addr, ec);
}

void InfinibandBaseSocket::setHandler(InfinibandBaseHandler* handler) {
    mImpl->handler = handler;
}

void InfinibandAcceptor::listen(int backlog, boost::system::error_code& ec) {
    mTransport.listen(mImpl, backlog, ec);
}

void InfinibandSocket::connect(sockaddr* addr, boost::system::error_code& ec) {
    mTransport.connect(mImpl, addr, ec);
}

void InfinibandSocket::disconnect(boost::system::error_code& ec) {
    mTransport.disconnect(mImpl, ec);
}

void InfinibandSocket::send(InfinibandBuffer& buffer, boost::system::error_code& ec) {
    mTransport.send(mImpl, buffer, ec);
}

InfinibandBuffer InfinibandSocket::acquireBuffer(size_t length) {
    return mImpl->device->acquireBuffer(length);
}

void InfinibandSocket::releaseBuffer(uint32_t id) {
    mImpl->device->releaseBuffer(id);
}

InfinibandBaseHandler::~InfinibandBaseHandler() {
}

bool InfinibandSocketHandler::onConnection(InfinibandSocket socket) {
    // TODO This should never be called
    return false;
}

void InfinibandAcceptorHandler::onConnected(const boost::system::error_code& ec) {
    // TODO This should never be called
}

void InfinibandAcceptorHandler::onReceive(const void* buffer, size_t length, const boost::system::error_code& ec) {
    // TODO This should never be called
}

void InfinibandAcceptorHandler::onSend(const void* buffer, size_t length, const boost::system::error_code& ec) {
    // TODO This should never be called
}

void InfinibandAcceptorHandler::onDisconnect() {
    // TODO This should never be called
}

void InfinibandAcceptorHandler::onDisconnected() {
    // TODO This should never be called
}

} // namespace infinio
} // namespace crossbow
