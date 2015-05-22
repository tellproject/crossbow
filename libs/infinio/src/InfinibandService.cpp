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

InfinibandService::InfinibandService(EventDispatcher& dispatcher, const InfinibandLimits& limits)
        : mDispatcher(dispatcher),
          mLimits(limits),
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

    SERVICE_LOG("Initialize device context");
    int numDevices = 0;
    auto devices = rdma_get_devices(&numDevices);
    if (numDevices != 1) {
        SERVICE_ERROR("Only one Infiniband device is supported at this moment");
        std::terminate();
    }
    mDevice.reset(new DeviceContext(mDispatcher, mLimits, *devices));

    std::error_code ec;
    mDevice->init(ec);
    rdma_free_devices(devices);

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
    std::error_code ec;
    shutdown(ec);
    if (ec) {
        std::cerr << "Error while shutting down Infiniband service [errno = " << ec << " " << ec.message() << "]"
                  << std::endl;
    }
}

void InfinibandService::shutdown(std::error_code& ec) {
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

    if (mChannel) {
        SERVICE_LOG("Destroy event channel");
        errno = 0;
        rdma_destroy_event_channel(mChannel);
        if (errno) {
            std::cerr << "Unable to destroy RDMA Event Channel [errno = " << errno << " " << strerror(errno) << "]"
                      << std::endl;
            return;
        }
        mChannel = nullptr;
    }

    if (mPollingThread.joinable()) {
        SERVICE_LOG("Wait for event poll thread");
        // TODO: Ask Jonas whether there is a better solution to thish
        mPollingThread.detach();
    }
}

CompletionContext* InfinibandService::context() {
    return mDevice->context();
}

void InfinibandService::processEvent(struct rdma_cm_event* event) {
    SERVICE_LOG("Processing event %1%", rdma_event_str(event->event));

#define HANDLE_EVENT(__case, __handler, ...)\
    case __case: {\
        auto id = event->id;\
        mDispatcher.post([id] () {\
            reinterpret_cast<InfinibandSocket*>(id->context)->__handler(__VA_ARGS__);\
        });\
    } break;

    switch (event->event) {
    HANDLE_EVENT(RDMA_CM_EVENT_ADDR_RESOLVED, onAddressResolved);
    HANDLE_EVENT(RDMA_CM_EVENT_ADDR_ERROR, onAddressError);
    HANDLE_EVENT(RDMA_CM_EVENT_ROUTE_RESOLVED, onRouteResolved);
    HANDLE_EVENT(RDMA_CM_EVENT_ROUTE_ERROR, onRouteError);

    case RDMA_CM_EVENT_CONNECT_REQUEST: {
        auto listen_id = event->listen_id;
        auto id = event->id;
        mDispatcher.post([this, listen_id, id] () {
            ConnectionRequest request(new InfinibandSocket(*this, id));
            reinterpret_cast<InfinibandAcceptor*>(listen_id->context)->onConnectionRequest(std::move(request));
        });
    } break;

    HANDLE_EVENT(RDMA_CM_EVENT_CONNECT_ERROR, onConnectionError, error::connection_error);
    HANDLE_EVENT(RDMA_CM_EVENT_UNREACHABLE, onConnectionError, error::unreachable);
    HANDLE_EVENT(RDMA_CM_EVENT_REJECTED, onConnectionError, error::connection_rejected);

    HANDLE_EVENT(RDMA_CM_EVENT_ESTABLISHED, onConnectionEstablished);
    HANDLE_EVENT(RDMA_CM_EVENT_DISCONNECTED, onDisconnected);
    HANDLE_EVENT(RDMA_CM_EVENT_TIMEWAIT_EXIT, onTimewaitExit);

    default:
        break;
    }
}

} // namespace infinio
} // namespace crossbow
