#include "DeviceContext.hpp"

#include <crossbow/infinio/ErrorCode.hpp>
#include <crossbow/infinio/InfinibandSocket.hpp>

#include "AddressHelper.hpp"
#include "Logging.hpp"
#include "WorkRequestId.hpp"

#include <cerrno>
#include <cstring>
#include <vector>

#include <sys/mman.h>

#define COMPLETION_LOG(...) INFINIO_LOG("[CompletionContext] " __VA_ARGS__)
#define COMPLETION_ERROR(...) INFINIO_ERROR("[CompletionContext] " __VA_ARGS__)
#define DEVICE_LOG(...) INFINIO_LOG("[DeviceContext] " __VA_ARGS__)
#define DEVICE_ERROR(...) INFINIO_ERROR("[DeviceContext] " __VA_ARGS__)

namespace crossbow {
namespace infinio {

void CompletionContext::init(std::error_code& ec) {
    if (mCompletionQueue) {
        ec = error::already_initialized;
        return;
    }

    COMPLETION_LOG("Allocate send buffer memory");
    mSendDataRegion = mDevice.allocateMemoryRegion(bufferOffset(mSendBufferCount), ec);
    if (ec) {
        return;
    }
    mSendData = mSendDataRegion->addr;

    COMPLETION_LOG("Add %1% buffers to send buffer queue", mSendBufferCount);
    for (uint16_t id = 0x0u; id < mSendBufferCount; ++id) {
        mSendBufferQueue.push(id);
    }

    COMPLETION_LOG("Create completion queue");
    errno = 0;
    mCompletionQueue = ibv_create_cq(mDevice.mVerbs, mCompletionQueueLength, nullptr, nullptr, 0);
    if (mCompletionQueue == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return;
    }

    COMPLETION_LOG("Create poll thread");
    mPollThread = std::thread([this] () {
        while (!mShutdown.load()) {
            poll();
        }
    });
}

void CompletionContext::shutdown(std::error_code& ec) {
    if (mShutdown.load()) {
        ec = std::error_code();
        return;
    }
    mShutdown.store(true);

    // TODO We have to join the poll thread
    // We are not allowed to call join in the same thread as the poll loop is

    COMPLETION_LOG("Destroy completion queue");
    if (auto res = ibv_destroy_cq(mCompletionQueue) != 0) {
        ec = std::error_code(res, std::system_category());
        return;
    }
    mCompletionQueue = nullptr;

    COMPLETION_LOG("Destroy send buffer memory");
    mDevice.destroyMemoryRegion(mSendDataRegion, ec);
    if (ec) {
        return;
    }
    mSendData = nullptr;
    mSendDataRegion = nullptr;
}

void CompletionContext::addConnection(InfinibandSocket* socket, std::error_code& ec) {
    struct ibv_qp_init_attr_ex qp_attr;
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq = mCompletionQueue;
    qp_attr.recv_cq = mCompletionQueue;
    qp_attr.srq = mDevice.mReceiveQueue;
    qp_attr.cap.max_send_wr = mSendQueueLength;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.comp_mask = IBV_QP_INIT_ATTR_PD;
    qp_attr.pd = mDevice.mProtectionDomain;

    auto id = socket->mId;

    COMPLETION_LOG("%1%: Creating queue pair", formatRemoteAddress(id));
    errno = 0;
    if (rdma_create_qp_ex(id, &qp_attr) != 0) {
        ec = std::error_code(errno, std::system_category());
        return;
    }

    // TODO Make this an assertion
    if (id->qp->qp_num >= (0x1u << 25)) {
        COMPLETION_ERROR("QP number is larger than 24 bits");
    }

    if (!mSocketMap.insert(std::make_pair(id->qp->qp_num, socket)).second) {
        // TODO Insert failed
        return;
    }
}

void CompletionContext::drainConnection(InfinibandSocket* socket, std::error_code& ec) {
    mDrainingQueue.push_back(socket);
}

void CompletionContext::removeConnection(InfinibandSocket* socket, std::error_code& ec) {
    auto id = socket->mId;

    if (mSocketMap.erase(id->qp->qp_num) == 0) {
        // TODO Socket not associated with this completion context
    }

    COMPLETION_LOG("%1%: Destroying queue pair", formatRemoteAddress(id));
    errno = 0;
    rdma_destroy_qp(id);
    if (errno != 0) {
        ec = std::error_code(errno, std::system_category());
        return;
    }
}

InfinibandBuffer CompletionContext::acquireSendBuffer(uint32_t length) {
    if (mSendBufferQueue.empty()) {
        return InfinibandBuffer(InfinibandBuffer::INVALID_ID);
    }
    auto id = mSendBufferQueue.top();
    mSendBufferQueue.pop();
    if (length > mSendBufferLength) {
        return InfinibandBuffer(InfinibandBuffer::INVALID_ID);
    }

    InfinibandBuffer buffer(id);
    buffer.handle()->addr = reinterpret_cast<uintptr_t>(mSendData) + bufferOffset(id);
    buffer.handle()->length = length;
    buffer.handle()->lkey = mSendDataRegion->lkey;
    return buffer;
}

void CompletionContext::releaseSendBuffer(InfinibandBuffer& buffer) {
    if (buffer.handle()->lkey != mSendDataRegion->lkey) {
        COMPLETION_ERROR("Trying to release send buffer registered to another region");
        return;
    }
    releaseSendBuffer(buffer.id());
}

void CompletionContext::releaseSendBuffer(uint16_t id) {
    if (id == InfinibandBuffer::INVALID_ID) {
        return;
    }
    mSendBufferQueue.push(id);
}

void CompletionContext::poll() {
    std::vector<InfinibandSocket*> draining;
    struct ibv_wc wc[mCompletionQueueLength];

    if (!mDrainingQueue.empty()) {
        draining.swap(mDrainingQueue);
    }

    // Poll the completion queue
    errno = 0;
    auto num = ibv_poll_cq(mCompletionQueue, mCompletionQueueLength, wc);

    // Check if polling the completion queue failed
    if (num < 0 && !mShutdown.load()) {
        COMPLETION_ERROR("Polling completion queue failed [error = %1% %2%]", errno, strerror(errno));
    }

    // Process all polled work completions
    for (int i = 0; i < num; ++i) {
        processWorkComplete(&wc[i]);
    }

    // Process all task from the task queue
    std::function<void()> fun;
    while (mTaskQueue.read(fun)) {
        fun();
    }

    // Process all drained connections
    for (auto socket: draining) {
        socket->onDrained();
    }
    draining.clear();
}

void CompletionContext::processWorkComplete(struct ibv_wc* wc) {
    COMPLETION_LOG("Processing WC with ID %1% on queue %2% with status %3% %4%", wc->wr_id, wc->qp_num, wc->status,
            ibv_wc_status_str(wc->status));

    WorkRequestId workId(wc->wr_id);
    std::error_code ec;

    auto i = mSocketMap.find(wc->qp_num);
    if (i == mSocketMap.end()) {
        COMPLETION_LOG("No matching socket for qp_num %1%", wc->qp_num);

        // In the case that we have no socket associated with the qp_num we just repost the buffer to the shared receive
        // queue or release the buffer in the case of send
        switch (workId.workType()) {

        // In the case the work request was a receive, we try to repost the shared receive buffer
        case WorkType::RECEIVE: {
            mDevice.postReceiveBuffer(workId.bufferId(), ec);
            if (ec) {
                COMPLETION_ERROR("Failed to post receive buffer on wc complete [errno = %1% - %2%]", ec, ec.message());
                // TODO Error Handling (this is more or less a memory leak)
            }
        } break;

        // In the case the work request was a send we just release the send buffer
        case WorkType::SEND: {
            releaseSendBuffer(workId.bufferId());
        } break;

        default:
            break;
        }

        return;
    }
    InfinibandSocket* socket = i->second;

    // TODO Better error handling
    // Right now we just propagate the error to the connection
    if (wc->status != IBV_WC_SUCCESS) {
        ec = std::error_code(wc->status, error::get_work_completion_category());
    } else {
        if (workId.workType() == WorkType::SEND && wc->opcode != IBV_WC_SEND) {
            COMPLETION_ERROR("Send buffer but opcode %1% not send", wc->opcode);
            // TODO Send buffer but opcode not send
        }
        if (workId.workType() == WorkType::RECEIVE && wc->opcode != IBV_WC_RECV) {
            COMPLETION_ERROR("Receive buffer but opcode %1% not receive", wc->opcode);
            // TODO receive buffer but opcode not receive
        }
        if (workId.workType() == WorkType::READ && wc->opcode != IBV_WC_RDMA_READ) {
            COMPLETION_ERROR("Read buffer but opcode %1% not read", wc->opcode);
            // TODO read buffer but opcode not read
        }
        if (workId.workType() == WorkType::WRITE && wc->opcode != IBV_WC_RDMA_WRITE) {
            COMPLETION_ERROR("Write buffer but opcode %1% not write", wc->opcode);
            // TODO write buffer but opcode not write
        }
    }

    switch (workId.workType()) {
    case WorkType::RECEIVE: {
        COMPLETION_LOG("Executing successful receive event of id %1%", workId.bufferId());

        if (workId.bufferId() >= mDevice.mReceiveBufferCount) {
            // TODO Invalid buffer
            return;
        }
        auto buffer = reinterpret_cast<uintptr_t>(mDevice.mReceiveData) + mDevice.bufferOffset(workId.bufferId());
        socket->onReceive(reinterpret_cast<const void*>(buffer), wc->byte_len, ec);

        std::error_code ec2;
        mDevice.postReceiveBuffer(workId.bufferId(), ec2);
        if (ec2) {
            COMPLETION_ERROR("Failed to post receive buffer on wc complete [error = %1% %2%]", ec2, ec2.message());
            // TODO Error Handling
        }
    } break;

    case WorkType::SEND: {
        COMPLETION_LOG("Executing successful send event of id %1%", workId.bufferId());
        socket->onSend(workId.userId(), ec);
        releaseSendBuffer(workId.bufferId());
    } break;

    case WorkType::READ: {
        COMPLETION_LOG("Executing successful read event of id %1%", workId.bufferId());
        socket->onRead(workId.userId(), ec);
    } break;

    case WorkType::WRITE: {
        COMPLETION_LOG("Executing successful read event of id %1%", workId.bufferId());
        socket->onWrite(workId.userId(), ec);
    } break;

    default: {
        COMPLETION_LOG("Unknown work type");
    } break;
    }
}

void DeviceContext::init(std::error_code& ec) {
    if (mProtectionDomain) {
        return; // Already initialized
    }

    DEVICE_LOG("Allocate protection domain");
    errno = 0;
    mProtectionDomain = ibv_alloc_pd(mVerbs);
    if (mProtectionDomain == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return;
    }

    DEVICE_LOG("Allocate receive buffer memory");
    mReceiveDataRegion = allocateMemoryRegion(bufferOffset(mReceiveBufferCount), ec);
    if (ec) {
        return;
    }
    mReceiveData = mReceiveDataRegion->addr;

    initReceiveQueue(ec);
    if (ec) {
        return;
    }

    for (auto& context : mCompletion) {
        context->init(ec);
        if (ec) {
            return;
        }
    }
}

void DeviceContext::shutdown(std::error_code& ec) {
    if (mShutdown.load()) {
        return;
    }
    mShutdown.store(true);

    DEVICE_LOG("Shutdown completion context");
    for (auto& context : mCompletion) {
        context->shutdown(ec);
        if (ec) {
            return;
        }
    }
    mCompletion.clear();

    DEVICE_LOG("Destroy shared receive queue");
    if (auto res = ibv_destroy_srq(mReceiveQueue) != 0) {
        ec = std::error_code(res, std::system_category());
        return;
    }

    DEVICE_LOG("Destroy receive buffer memory");
    destroyMemoryRegion(mReceiveDataRegion, ec);
    if (ec) {
        return;
    }

    DEVICE_LOG("Destroy protection domain");
    if (auto res = ibv_dealloc_pd(mProtectionDomain) != 0) {
        ec = std::error_code(res, std::system_category());
        return;
    }
}

LocalMemoryRegion DeviceContext::registerMemoryRegion(void* data, size_t length, int access, std::error_code& ec) {
    DEVICE_LOG("Create memory region at %1%", data);
    errno = 0;
    mReceiveDataRegion = ibv_reg_mr(mProtectionDomain, data, length, access);
    if (mReceiveDataRegion == nullptr) {
        ec = std::error_code(errno, std::system_category());
    }
    return LocalMemoryRegion(mReceiveDataRegion);
}

struct ibv_mr* DeviceContext::allocateMemoryRegion(size_t length, std::error_code& ec) {
    DEVICE_LOG("Map %1% bytes of buffer space", length);
    errno = 0;
    auto data = mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    if (data == MAP_FAILED) {
        ec = std::error_code(errno, std::system_category());
        return nullptr;
    }

    DEVICE_LOG("Create memory region at %1%", data);
    errno = 0;
    auto memoryRegion = ibv_reg_mr(mProtectionDomain, data, length, IBV_ACCESS_LOCAL_WRITE);
    if (memoryRegion == nullptr) {
        ec = std::error_code(errno, std::system_category());
        munmap(data, length);
        return nullptr;
    }
    return memoryRegion;
}

void DeviceContext::destroyMemoryRegion(struct ibv_mr* memoryRegion, std::error_code& ec) {
    auto data = memoryRegion->addr;
    auto length = memoryRegion->length;
    DEVICE_LOG("Destroy memory region at %1%", data);
    if (auto res = ibv_dereg_mr(memoryRegion) != 0) {
        ec = std::error_code(res, std::system_category());
        return;
    }

    // TODO Size has to be a multiple of the page size
    DEVICE_LOG("Unmap buffer space at %1%", data);
    errno = 0;
    if (munmap(data, length) != 0) {
        ec = std::error_code(errno, std::system_category());
    }
}

void DeviceContext::initReceiveQueue(std::error_code& ec) {
    DEVICE_LOG("Create shared receive queue");
    struct ibv_srq_init_attr srq_attr;
    memset(&srq_attr, 0, sizeof(srq_attr));
    srq_attr.attr.max_wr = mReceiveBufferCount;
    srq_attr.attr.max_sge = 1;

    errno = 0;
    mReceiveQueue = ibv_create_srq(mProtectionDomain, &srq_attr);
    if (mReceiveQueue == nullptr) {
        ec = std::error_code(errno, std::system_category());
        return;
    }

    DEVICE_LOG("Post %1% buffers to shared receive queue", mReceiveBufferCount);
    for (uint16_t id = 0x0u; id < mReceiveBufferCount; ++id) {
        postReceiveBuffer(id, ec);
        if (ec) {
            DEVICE_ERROR("Failed to post receive buffer on wc complete [error = %1% %2%]");
            return;
        }
    }
}

/**
 * @brief Helper function posting the buffer to the shared receive queue
 */
void DeviceContext::postReceiveBuffer(uint16_t id, std::error_code& ec) {
    WorkRequestId workId(0x0u, id, WorkType::RECEIVE);

    // Prepare work request
    struct ibv_sge sge;
    sge.addr = reinterpret_cast<uintptr_t>(mReceiveData) + bufferOffset(id);
    sge.length = mReceiveBufferLength;
    sge.lkey = mReceiveDataRegion->lkey;

    struct ibv_recv_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = workId.id();
    wr.sg_list = &sge;
    wr.num_sge = 1;

    // Repost receives on shared queue
    struct ibv_recv_wr* bad_wr = nullptr;
    if (auto res = ibv_post_srq_recv(mReceiveQueue, &wr, &bad_wr)) {
        ec = std::error_code(res, std::system_category());
        return;
    }
}

} // namespace infinio
} // namespace crossbow
