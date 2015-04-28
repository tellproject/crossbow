#include <crossbow/infinio/InfinibandService.hpp>

#include <crossbow/infinio/EventDispatcher.hpp>
#include <crossbow/infinio/InfinibandSocket.hpp>

#include "AddressHelper.hpp"
#include "DeviceContext.hpp"
#include "ErrorCode.hpp"
#include "InfinibandSocketImpl.hpp"
#include "Logging.hpp"

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iostream>

#define SERVICE_LOG(...) INFINIO_LOG("[InfinibandService] " __VA_ARGS__)

namespace crossbow {
namespace infinio {

namespace {

/**
 * @brief Timeout value for resolving addresses and routes
 */
constexpr std::chrono::milliseconds gTimeout = std::chrono::milliseconds(10);

/**
 * @brief Helper function to dispatch any errors while connecting
 */
void dispatchConnectionError(EventDispatcher& dispatcher, struct rdma_cm_id* id, error::network_errors res) {
    auto impl = reinterpret_cast<SocketImplementation*>(id->context);
    dispatcher.post([impl, res] () {
        boost::system::error_code ec;
        impl->device->removeConnection(impl, ec);
        if (ec) {
            // There is nothing we can do if removing the incomplete connection failed
            SERVICE_LOG("%1%: Removing invalid connection failed [%2%: %3%]", formatRemoteAddress(impl->id), ec,
                    ec.message());
        }

        ec = res;
        impl->handler->onConnected(ec);
    });
}

} // anonymous namespace

InfinibandService::InfinibandService(EventDispatcher& dispatcher)
        : mDispatcher(dispatcher),
          mDevice(nullptr),
          mShutdown(false) {
    SERVICE_LOG("Create event channel");
    errno = 0;
    mChannel = rdma_create_event_channel();
    if (!mChannel) {
        std::cerr << "Unable to create RDMA Event Channel [errno = " << errno << " " << strerror(errno) << "]"
                  << std::endl;
        return;
    }

    SERVICE_LOG("Start event poll thread");
    mPollingThread = std::thread([this] () {
        struct rdma_cm_event* event = nullptr;
        errno = 0;
        while (rdma_get_cm_event(mChannel, &event) == 0) {
            processEvent(event);
            rdma_ack_cm_event(event);
        }

        // Check if the system is shutting down
        if (mShutdown.load()) {
            SERVICE_LOG("Exit event poll thread");
            return;
        }

        SERVICE_LOG("Error while processing event loop [errcode = %1% %2%]", errno, strerror(errno));
        std::terminate();
    });
}

InfinibandService::~InfinibandService() {
    boost::system::error_code ec;
    shutdown(ec);
    if (ec) {
        std::cerr << "Error while shutting down Infiniband service [errno = " << ec << " " << ec.message() << "]"
                  << std::endl;
    }
}

void InfinibandService::shutdown(boost::system::error_code& ec) {
    if (mShutdown.load()) {
        return;
    }
    mShutdown.store(true);

    if (mDevice) {
        mDevice->shutdown(ec);
        if (ec) {
            std::cerr << "Unable to destroy Device Context [err = " << ec << " - " << ec.message() << std::endl;
            return;
        }
    }

    SERVICE_LOG("Destroy event channel");
    errno = 0;
    rdma_destroy_event_channel(mChannel);
    if (errno) {
        std::cerr << "Unable to destroy RDMA Event Channel [errno = " << errno << " " << strerror(errno) << "]"
                  << std::endl;
        return;
    }
    mChannel = nullptr;

    SERVICE_LOG("Wait for event poll thread");
    // TODO: Ask Jonas whether there is a better solution to thish
    mPollingThread.detach();
    mPollingThread.join();
}

void InfinibandService::open(SocketImplementation* impl, boost::system::error_code& ec) {
    if (impl->id || impl->state.load() != ConnectionState::CLOSED) {
        ec = error::already_open;
        return;
    }

    SERVICE_LOG("Open socket");
    errno = 0;
    if (rdma_create_id(mChannel, &impl->id, impl, RDMA_PS_TCP) != 0) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }
    impl->state.store(ConnectionState::OPEN);
}

void InfinibandService::close(SocketImplementation* impl, boost::system::error_code& ec) {
    if (!impl->id) {
        ec = error::bad_descriptor;
        return;
    }

    SERVICE_LOG("Close socket");
    errno = 0;
    if (rdma_destroy_id(impl->id) != 0) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }
    impl->id = nullptr;
    impl->state.store(ConnectionState::CLOSED);
}

void InfinibandService::bind(SocketImplementation* impl, sockaddr* addr, boost::system::error_code& ec) {
    if (!impl->id || impl->state.load() != ConnectionState::OPEN) {
        ec = error::bad_descriptor;
        return;
    }

    SERVICE_LOG("Bind on address %1%", formatAddress(addr));
    errno = 0;
    if (rdma_bind_addr(impl->id, addr) != 0) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }
}

void InfinibandService::listen(SocketImplementation* impl, int backlog, boost::system::error_code& ec) {
    if (!impl->id || impl->state.load() != ConnectionState::OPEN) {
        ec = error::bad_descriptor;
        return;
    }

    SERVICE_LOG("Listen on socket with backlog %1%", backlog);
    errno = 0;
    if (rdma_listen(impl->id, backlog) != 0) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }
    impl->state.store(ConnectionState::LISTENING);
}

void InfinibandService::connect(SocketImplementation* impl, sockaddr* addr, boost::system::error_code& ec) {
    if (!impl->id || impl->state.load() != ConnectionState::OPEN) {
        ec = error::bad_descriptor;
        return;
    }

    SERVICE_LOG("%1%: Connect to address", formatAddress(addr));
    errno = 0;
    if (rdma_resolve_addr(impl->id, nullptr, addr, gTimeout.count()) != 0) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }

    // TODO Compare and swap?
    impl->state.store(ConnectionState::CONNECTING);
}

void InfinibandService::disconnect(SocketImplementation* impl, boost::system::error_code& ec) {
    if (!impl->id) {
        ec = error::bad_descriptor;
        return;
    }

    SERVICE_LOG("%1%: Disconnect from address", formatRemoteAddress(impl->id));
    errno = 0;
    if (rdma_disconnect(impl->id) != 0) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }

    // TODO Compare and swap?
    impl->state.store(ConnectionState::DISCONNECTING);
}

void InfinibandService::send(SocketImplementation* impl, InfinibandBuffer& buffer, boost::system::error_code& ec) {
    if (!impl->id) {
        ec = error::bad_descriptor;
        return;
    }

    if (impl->state == ConnectionState::DISCONNECTED) {
        ec = error::disconnected;
        return;
    }

    struct ibv_send_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.opcode = IBV_WR_SEND;
    wr.wr_id = (buffer.id() << 1);
    wr.sg_list = buffer.handle();
    wr.num_sge = 1;
    wr.send_flags = IBV_SEND_SIGNALED;

    SERVICE_LOG("%1%: Send %2% bytes from buffer %3%", formatRemoteAddress(impl->id), buffer.length(),
                buffer.id());
    struct ibv_send_wr* bad_wr = nullptr;
    if (auto res = ibv_post_send(impl->id->qp, &wr, &bad_wr) != 0) {
        ec = boost::system::error_code(res, boost::system::system_category());
        return;
    }
}

DeviceContext* InfinibandService::getDevice(struct ibv_context* verbs) {
    if (!mDevice) {
        SERVICE_LOG("Initialize device context");
        mDevice.reset(new DeviceContext(mDispatcher, verbs));

        boost::system::error_code ec;
        mDevice->init(ec);
        if (ec) {
            SERVICE_LOG("Failure to initialize context [%1%: %2%]", ec, ec.message());
            std::terminate();
        }
    }

    // Right now we only support one device
    if (verbs != mDevice->context()) {
        SERVICE_LOG("Incoming connection from different device context");
        std::terminate();
    }

    return mDevice.get();
}

void InfinibandService::processEvent(struct rdma_cm_event* event) {
    SERVICE_LOG("Processing event %1%", rdma_event_str(event->event));

    switch (event->event) {
    case RDMA_CM_EVENT_ADDR_RESOLVED:
        onAddressResolved(event->id);
        break;

    case RDMA_CM_EVENT_ADDR_ERROR:
        onAddressError(event->id);
        break;

    case RDMA_CM_EVENT_ROUTE_RESOLVED:
        onRouteResolved(event->id);
        break;

    case RDMA_CM_EVENT_ROUTE_ERROR:
        onRouteError(event->id);
        break;

    case RDMA_CM_EVENT_CONNECT_REQUEST:
        onConnectionRequest(event->listen_id, event->id);
        break;

    case RDMA_CM_EVENT_CONNECT_RESPONSE:
        // TODO Do we have to do something here? We do not support UD so we have always a QP associated
        break;

    case RDMA_CM_EVENT_CONNECT_ERROR:
        onConnectionError(event->id);
        break;

    case RDMA_CM_EVENT_UNREACHABLE:
        onUnreachable(event->id);
        break;

    case RDMA_CM_EVENT_REJECTED:
        onRejected(event->id);
        break;

    case RDMA_CM_EVENT_ESTABLISHED:
        onEstablished(event->id);
        break;

    case RDMA_CM_EVENT_DISCONNECTED:
        onDisconnected(event->id);
        break;

    case RDMA_CM_EVENT_TIMEWAIT_EXIT:
        onTimewaitExit(event->id);
        break;

    default:
        break;
    }
}

void InfinibandService::onAddressResolved(struct rdma_cm_id* id) {
    SERVICE_LOG("%1%: Address resolved", formatRemoteAddress(id));
    errno = 0;
    if (rdma_resolve_route(id, gTimeout.count()) != 0) {
        auto impl = reinterpret_cast<SocketImplementation*>(id->context);
        auto res = errno;
        mDispatcher.post([impl, res] () {
            boost::system::error_code ec(res, boost::system::system_category());
            impl->handler->onConnected(ec);
        });
    }
}

void InfinibandService::onAddressError(struct rdma_cm_id* id) {
    SERVICE_LOG("%1%: Address resolve failed", formatRemoteAddress(id));
    auto impl = reinterpret_cast<SocketImplementation*>(id->context);
    mDispatcher.post([impl] () {
        boost::system::error_code ec = error::address_resolution;
        impl->handler->onConnected(ec);
    });
}

void InfinibandService::onRouteResolved(struct rdma_cm_id* id) {
    SERVICE_LOG("%1%: Route resolved", formatRemoteAddress(id));
    auto impl = reinterpret_cast<SocketImplementation*>(id->context);
    mDispatcher.post([this, impl] () {
        // Set the device context on the socket (this can not be done earlier because the context might not exist)
        auto device = getDevice(impl->id->verbs);
        impl->device = device;

        // Add connection to queue handler
        boost::system::error_code ec;
        device->addConnection(impl, ec);
        if (ec) {
            impl->handler->onConnected(ec);
            return;
        }

        struct rdma_conn_param cm_params;
        memset(&cm_params, 0, sizeof(cm_params));

        errno = 0;
        if (rdma_connect(impl->id, &cm_params) != 0) {
            auto res = errno;

            device->removeConnection(impl, ec);
            if (ec) {
                // There is nothing we can do if removing the incomplete connection failed
                SERVICE_LOG("%1%: Remove of invalid connection failed [%2%: %3%]", formatRemoteAddress(impl->id), ec,
                        ec.message());
            }

            boost::system::error_code ec(res, boost::system::system_category());
            impl->handler->onConnected(ec);
        }
    });
}

void InfinibandService::onRouteError(rdma_cm_id* id) {
    SERVICE_LOG("%1%: Route resolve failed", formatRemoteAddress(id));
    auto impl = reinterpret_cast<SocketImplementation*>(id->context);
    mDispatcher.post([impl] () {
        boost::system::error_code ec = error::route_resolution;
        impl->handler->onConnected(ec);
    });
}

void InfinibandService::onConnectionRequest(struct rdma_cm_id* listener, struct rdma_cm_id* id) {
    auto limpl = reinterpret_cast<SocketImplementation*>(listener->context);
    mDispatcher.post([this, limpl, id] () {
        auto device = getDevice(id->verbs);
        auto impl = new SocketImplementation(id, device);
        id->context = impl;

        // Invoke the connection handler
        if (!limpl->handler->onConnection(InfinibandSocket(*this, impl))) {
            delete impl;
            errno = 0;
            if (rdma_reject(id, nullptr, 0) != 0) {
                // There is nothing we can do if rejecting the incomplete connection failed
                SERVICE_LOG("%1%: Rejecting invalid connection failed [%2% - %3%]", formatRemoteAddress(id), errno,
                        strerror(errno));
            }
            return;
        }

        // Add connection to queue handler
        boost::system::error_code ec;
        device->addConnection(impl, ec);
        if (ec) {
            impl->handler->onConnected(ec);
            return;
        }

        struct rdma_conn_param cm_params;
        memset(&cm_params, 0, sizeof(cm_params));

        errno = 0;
        if (rdma_accept(id, &cm_params) != 0) {
            auto res = errno;

            device->removeConnection(impl, ec);
            if (ec) {
                // There is nothing we can do if removing the incomplete connection failed
                SERVICE_LOG("%1%: Removing invalid connection failed [%2%: %3%]", formatRemoteAddress(id), ec,
                        ec.message());
            }

            boost::system::error_code ec(res, boost::system::system_category());
            impl->handler->onConnected(ec);
        }
    });
}

void InfinibandService::onConnectionError(rdma_cm_id* id) {
    dispatchConnectionError(mDispatcher, id, error::connection_error);
}

void InfinibandService::onUnreachable(rdma_cm_id* id) {
    dispatchConnectionError(mDispatcher, id, error::unreachable);
}

void InfinibandService::onRejected(rdma_cm_id* id) {
    dispatchConnectionError(mDispatcher, id, error::connection_rejected);
}

void InfinibandService::onEstablished(struct rdma_cm_id* id) {
    auto impl = reinterpret_cast<SocketImplementation*>(id->context);
    mDispatcher.post([impl] () {
        boost::system::error_code ec;
        impl->handler->onConnected(ec);
    });
}

void InfinibandService::onDisconnected(struct rdma_cm_id* id) {
    auto impl = reinterpret_cast<SocketImplementation*>(id->context);
    mDispatcher.post([impl] () {
        impl->handler->onDisconnect();

        // TODO Call disconnect here directly? The socket has to call it anyway to succesfully shutdown the connection
    });
}

void InfinibandService::onTimewaitExit(struct rdma_cm_id* id) {
    auto impl = reinterpret_cast<SocketImplementation*>(id->context);
    mDispatcher.post([impl] () {
        boost::system::error_code ec;
        impl->device->drainConnection(impl, ec);
        if (ec) {
            // TODO How to handle this?
            SERVICE_LOG("%1%: Draining connection failed [%2%: %3%]", formatRemoteAddress(impl->id), ec, ec.message());
        }
    });
}

} // namespace infinio
} // namespace crossbow
