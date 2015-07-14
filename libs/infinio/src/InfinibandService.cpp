#include <crossbow/infinio/InfinibandService.hpp>

#include <crossbow/infinio/Endpoint.hpp>
#include <crossbow/infinio/ErrorCode.hpp>
#include <crossbow/infinio/InfinibandSocket.hpp>
#include <crossbow/logger.hpp>

#include "AddressHelper.hpp"
#include "DeviceContext.hpp"
#include "WorkRequestId.hpp"

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <iostream>

namespace crossbow {
namespace infinio {

namespace {

/**
 * @brief Wrapper managing the list of Infiniband devices
 */
class DeviceList : crossbow::non_copyable {
public:
    DeviceList()
            : mSize(0),
              mDevices(rdma_get_devices(&mSize)) {
        if (mDevices == nullptr) {
            throw std::system_error(errno, std::system_category());
        }
        LOG_TRACE("Queried %1% device(s)", mSize);
    }

    ~DeviceList() {
        if (mDevices != nullptr) {
            rdma_free_devices(mDevices);
        }
    }

    DeviceList(DeviceList&& other)
            : mSize(other.mSize),
              mDevices(other.mDevices) {
        other.mSize = 0;
        other.mDevices = nullptr;
    }

    DeviceList& operator=(DeviceList&& other) {
        if (mDevices != nullptr) {
            rdma_free_devices(mDevices);
        }

        mSize = other.mSize;
        other.mSize = 0;
        mDevices = other.mDevices;
        other.mDevices = nullptr;
    }

    size_t size() const {
        return mSize;
    }

    struct ibv_context* at(size_t index) {
        if (index >= mSize) {
            throw std::out_of_range("Index out of range");
        }
        return mDevices[index];
    }

private:
    int mSize;
    struct ibv_context** mDevices;
};

} // anonymous namespace

InfinibandProcessor::InfinibandProcessor(std::shared_ptr<DeviceContext> device, const InfinibandLimits& limits)
        : mProcessor(limits.pollCycles),
          mTaskQueue(mProcessor),
          mContext(new CompletionContext(mProcessor, std::move(device), limits)) {
    mProcessor.start();
}

InfinibandProcessor::~InfinibandProcessor() = default;

InfinibandService::InfinibandService(const InfinibandLimits& limits)
        : mLimits(limits),
          mShutdown(false) {
    LOG_TRACE("Create event channel");
    errno = 0;
    mChannel = rdma_create_event_channel();
    if (!mChannel) {
        LOG_ERROR("Unable to create RDMA Event Channel [error = %1% %2%]", errno, strerror(errno));
        return;
    }

    LOG_TRACE("Initialize device context");
    DeviceList devices;
    if (devices.size() != 1) {
        LOG_ERROR("Only one Infiniband device is supported at this moment");
        std::terminate();
    }
    mDevice = std::make_shared<DeviceContext>(mLimits, devices.at(0));
}

InfinibandService::~InfinibandService() {
    shutdown();
}

void InfinibandService::run() {
    LOG_TRACE("Start RDMA CM event polling");
    struct rdma_cm_event* event = nullptr;
    errno = 0;
    while (rdma_get_cm_event(mChannel, &event) == 0) {
        processEvent(event);
        rdma_ack_cm_event(event);
    }

    // Check if the system is shutting down
    if (mShutdown.load()) {
        LOG_TRACE("Exit RDMA CM event polling");
        return;
    }

    LOG_ERROR("Error while processing RDMA CM event loop [error = %1% %2%]", errno, strerror(errno));
    std::terminate();
}

void InfinibandService::shutdown() {
    if (mShutdown.load()) {
        return;
    }
    mShutdown.store(true);

    if (mDevice) {
        mDevice->shutdown();
    }

    if (mChannel) {
        LOG_TRACE("Destroy event channel");
        errno = 0;
        rdma_destroy_event_channel(mChannel);
        if (errno) {
            LOG_ERROR("Unable to destroy RDMA Event Channel [error = %1% %2%]", errno, strerror(errno));
            return;
        }
        mChannel = nullptr;
    }
}

std::unique_ptr<InfinibandProcessor> InfinibandService::createProcessor() {
    return std::unique_ptr<InfinibandProcessor>(new InfinibandProcessor(mDevice, mLimits));
}

InfinibandSocket InfinibandService::createSocket(InfinibandProcessor& processor) {
    return InfinibandSocket(new InfinibandSocketImpl(processor, mChannel));
}

LocalMemoryRegion InfinibandService::registerMemoryRegion(void* data, size_t length, int access) {
    return mDevice->registerMemoryRegion(data, length, access);
}

void InfinibandService::processEvent(struct rdma_cm_event* event) {
    LOG_TRACE("Processing event %1%", rdma_event_str(event->event));

#define HANDLE_EVENT(__case, __handler, ...)\
    case __case: {\
        reinterpret_cast<InfinibandSocketImpl*>(event->id->context)->__handler(__VA_ARGS__);\
    } break;

#define HANDLE_DATA_EVENT(__case, __handler)\
    case __case: {\
        crossbow::string data(reinterpret_cast<const char*>(event->param.conn.private_data),\
                event->param.conn.private_data_len);\
        reinterpret_cast<InfinibandSocketImpl*>(event->id->context)->__handler(data);\
    } break;

    switch (event->event) {
    HANDLE_EVENT(RDMA_CM_EVENT_ADDR_RESOLVED, onAddressResolved);
    HANDLE_EVENT(RDMA_CM_EVENT_ADDR_ERROR, onResolutionError, error::address_resolution);
    HANDLE_EVENT(RDMA_CM_EVENT_ROUTE_RESOLVED, onRouteResolved);
    HANDLE_EVENT(RDMA_CM_EVENT_ROUTE_ERROR, onResolutionError, error::route_resolution);

    case RDMA_CM_EVENT_CONNECT_REQUEST: {
        InfinibandSocket socket(new InfinibandSocketImpl(event->id));
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
