#include <crossbow/infinio/InfinibandService.hpp>

#include <crossbow/infinio/Endpoint.hpp>
#include <crossbow/infinio/InfinibandSocket.hpp>

#include <crossbow/infinio/ErrorCode.hpp>

#include "AddressHelper.hpp"
#include "DeviceContext.hpp"
#include "Logging.hpp"
#include "WorkRequestId.hpp"

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iostream>

#define SERVICE_LOG(...) INFINIO_LOG("[InfinibandService] " __VA_ARGS__)
#define SERVICE_ERROR(...) INFINIO_ERROR("[InfinibandService] " __VA_ARGS__)

namespace crossbow {
namespace infinio {

InfinibandService::InfinibandService(const InfinibandLimits& limits)
        : mLimits(limits),
          mDevice(nullptr),
          mShutdown(false) {
    SERVICE_LOG("Create event channel");
    errno = 0;
    mChannel = rdma_create_event_channel();
    if (!mChannel) {
        SERVICE_ERROR("Unable to create RDMA Event Channel [error = %1% %2%]", errno, strerror(errno));
        return;
    }

    SERVICE_LOG("Initialize device context");
    int numDevices = 0;
    auto devices = rdma_get_devices(&numDevices);
    if (numDevices != 1) {
        SERVICE_ERROR("Only one Infiniband device is supported at this moment");
        std::terminate();
    }
    mDevice.reset(new DeviceContext(mLimits, *devices));

    std::error_code ec;
    mDevice->init(ec);
    rdma_free_devices(devices);
}

InfinibandService::~InfinibandService() {
    std::error_code ec;
    shutdown(ec);
    if (ec) {
        SERVICE_ERROR("Error while shutting down Infiniband service [error = %1% %2%]", ec, ec.message());
    }
}

void InfinibandService::run() {
    SERVICE_LOG("Start RDMA CM event polling");
    struct rdma_cm_event* event = nullptr;
    errno = 0;
    while (rdma_get_cm_event(mChannel, &event) == 0) {
        processEvent(event);
        rdma_ack_cm_event(event);
    }

    // Check if the system is shutting down
    if (mShutdown.load()) {
        SERVICE_LOG("Exit RDMA CM event polling");
        return;
    }

    SERVICE_LOG("Error while processing RDMA CM event loop [error = %1% %2%]", errno, strerror(errno));
    std::terminate();
}

void InfinibandService::shutdown(std::error_code& ec) {
    if (mShutdown.load()) {
        return;
    }
    mShutdown.store(true);

    if (mDevice) {
        mDevice->shutdown(ec);
        if (ec) {
            SERVICE_ERROR("Unable to destroy Device Context [error = %1% %2%]", ec, ec.message());
            return;
        }
        mDevice.reset();
    }

    if (mChannel) {
        SERVICE_LOG("Destroy event channel");
        errno = 0;
        rdma_destroy_event_channel(mChannel);
        if (errno) {
            SERVICE_ERROR("Unable to destroy RDMA Event Channel [error = %1% %2%]", errno, strerror(errno));
            return;
        }
        mChannel = nullptr;
    }
}

InfinibandSocket InfinibandService::createSocket(uint64_t thread) {
    return InfinibandSocket(new InfinibandSocketImpl(*this, mChannel, mDevice->context(thread)));
}

CompletionContext* InfinibandService::context(uint64_t num) {
    return mDevice->context(num);
}

void InfinibandService::processEvent(struct rdma_cm_event* event) {
    SERVICE_LOG("Processing event %1%", rdma_event_str(event->event));

#define HANDLE_EVENT(__case, __handler, ...)\
    case __case: {\
        std::error_code ec;\
        auto socket = reinterpret_cast<InfinibandSocketImpl*>(event->id->context);\
        socket->execute([socket] () {\
            socket->__handler(__VA_ARGS__);\
        }, ec);\
        if (ec) {\
            SERVICE_ERROR("Unable to execute event on socket [error = %1% %2%]", ec, ec.message());\
        }\
    } break;

#define HANDLE_DATA_EVENT(__case, __handler)\
    case __case: {\
        crossbow::string data(reinterpret_cast<const char*>(event->param.conn.private_data),\
                event->param.conn.private_data_len);\
        std::error_code ec;\
        auto socket = reinterpret_cast<InfinibandSocketImpl*>(event->id->context);\
        socket->execute([socket, data] () {\
            socket->__handler(data);\
        }, ec);\
        if (ec) {\
            SERVICE_ERROR("Unable to execute event on socket [error = %1% %2%]", ec, ec.message());\
        }\
    } break;

    switch (event->event) {
    HANDLE_EVENT(RDMA_CM_EVENT_ADDR_RESOLVED, onAddressResolved);
    HANDLE_EVENT(RDMA_CM_EVENT_ADDR_ERROR, onResolutionError, error::address_resolution);
    HANDLE_EVENT(RDMA_CM_EVENT_ROUTE_RESOLVED, onRouteResolved);
    HANDLE_EVENT(RDMA_CM_EVENT_ROUTE_ERROR, onResolutionError, error::route_resolution);

    case RDMA_CM_EVENT_CONNECT_REQUEST: {
        InfinibandSocket socket(new InfinibandSocketImpl(*this, event->id));
        crossbow::string data(reinterpret_cast<const char*>(event->param.conn.private_data),
                event->param.conn.private_data_len);
        auto listener = reinterpret_cast<InfinibandAcceptorImpl*>(event->listen_id->context);
        listener->onConnectionRequest(std::move(socket), data);
    } break;

    HANDLE_EVENT(RDMA_CM_EVENT_CONNECT_ERROR, onConnectionError, error::connection_error);
    HANDLE_EVENT(RDMA_CM_EVENT_UNREACHABLE, onConnectionError, error::unreachable);

    HANDLE_DATA_EVENT(RDMA_CM_EVENT_REJECTED, onConnectionRejected);
    HANDLE_DATA_EVENT(RDMA_CM_EVENT_ESTABLISHED, onConnectionEstablished);

    HANDLE_EVENT(RDMA_CM_EVENT_DISCONNECTED, onDisconnected);
    HANDLE_EVENT(RDMA_CM_EVENT_TIMEWAIT_EXIT, onTimewaitExit);

    default:
        break;
    }
}

} // namespace infinio
} // namespace crossbow
