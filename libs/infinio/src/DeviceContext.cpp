#include "DeviceContext.hpp"

#include <crossbow/infinio/InfinibandSocket.hpp>

#include "AddressHelper.hpp"
#include "ErrorCode.hpp"
#include "InfinibandSocketImpl.hpp"
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

void CompletionContext::init(boost::system::error_code& ec) {
    if (mCompletionQueue) {
        ec = error::already_initialized;
        return;
    }

    COMPLETION_LOG("Create completion queue");
    errno = 0;
    mCompletionQueue = ibv_create_cq(mDevice.mVerbs, mCompletionQueueLength, nullptr, nullptr, 0);
    if (mCompletionQueue == nullptr) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }
}

void CompletionContext::shutdown(boost::system::error_code& ec) {
    if (mShutdown.load()) {
        ec = boost::system::error_code();
        return;
    }
    mShutdown.store(true);

    COMPLETION_LOG("Destroy completion queue");
    if (auto res = ibv_destroy_cq(mCompletionQueue) != 0) {
        ec = boost::system::error_code(res, boost::system::system_category());
        return;
    }
    mCompletionQueue = nullptr;
}

void CompletionContext::addConnection(SocketImplementation* impl, boost::system::error_code& ec) {
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

    COMPLETION_LOG("%1%: Creating queue pair", formatRemoteAddress(impl->id));
    errno = 0;
    if (rdma_create_qp_ex(impl->id, &qp_attr) != 0) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }

    if (!mSocketMap.insert(impl->id->qp->qp_num, impl).first) {
        // TODO Insert failed
        return;
    }
}

void CompletionContext::drainConnection(SocketImplementation* impl, boost::system::error_code& ec) {
    mDrainingQueue.push(impl);
}

void CompletionContext::removeConnection(SocketImplementation* impl, boost::system::error_code& ec) {
    if (!mSocketMap.erase(impl->id->qp->qp_num).first) {
        // TODO Socket not associated with this completion context
    }

    COMPLETION_LOG("%1%: Destroying queue pair", formatRemoteAddress(impl->id));
    errno = 0;
    rdma_destroy_qp(impl->id);
    if (errno != 0) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }
}

void CompletionContext::poll(EventDispatcher& dispatcher, boost::system::error_code& ec) {
    std::vector<SocketImplementation*> draining;
    struct ibv_wc wc[mCompletionQueueLength];

    for (size_t j = 0; j < mPollCycles; ++j) {
        while (true) {
            SocketImplementation* impl = nullptr;
            if (!mDrainingQueue.pop(impl)) {
                break;
            }
            draining.push_back(impl);
        }

        // Poll the completion queue
        errno = 0;
        auto num = ibv_poll_cq(mCompletionQueue, mCompletionQueueLength, wc);

        // Check if polling the completion queue failed
        if (num < 0) {
            if (!mShutdown.load()) {
                ec = boost::system::error_code(errno, boost::system::system_category());
            }
            return;
        }

        // Process all polled work completions
        for (size_t i = 0; i < num; ++i) {
            processWorkComplete(dispatcher, &wc[i]);
        }

        // Process all drained connections
        for (auto impl: draining) {
            processDrainedConnection(dispatcher, impl, ec);
            if (ec) {
                return;
            }
        }
        draining.clear();

        // Return immediately if we processed at least once work completion
        if (num > 0) {
            return;
        }
    }
}

void CompletionContext::processWorkComplete(EventDispatcher& dispatcher, struct ibv_wc* wc) {
    COMPLETION_LOG("Processing WC with ID %1% on queue %2% with status %3% %4%", wc->wr_id, wc->qp_num, wc->status,
            ibv_wc_status_str(wc->status));

    WorkRequestId workId(wc->wr_id);
    boost::system::error_code ec;

    auto i = mSocketMap.at(wc->qp_num);
    if (!i.first) {
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
            mDevice.releaseSendBuffer(workId.bufferId());
        } break;

        default:
            break;
        }

        return;
    }
    SocketImplementation* impl = i.second;

    // TODO Better error handling
    // Right now we just propagate the error to the connection
    if (wc->status != IBV_WC_SUCCESS) {
        ec = boost::system::error_code(wc->status, error::get_work_completion_category());
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

    // Increase amount of work
    addWork(impl);


    switch (workId.workType()) {
    case WorkType::RECEIVE: {
        auto byte_len = wc->byte_len;
        dispatcher.post([this, impl, workId, byte_len, ec] () {
            COMPLETION_LOG("Executing successful receive event of id %1%", workId.bufferId());

            if (workId.bufferId() >= mDevice.mReceiveBufferCount) {
                // TODO Invalid buffer
                return;
            }
            auto buffer = reinterpret_cast<uintptr_t>(mDevice.mReceiveData) + mDevice.bufferOffset(workId.bufferId());
            impl->handler->onReceive(reinterpret_cast<const void*>(buffer), byte_len, ec);

            boost::system::error_code ec2;
            mDevice.postReceiveBuffer(workId.bufferId(), ec2);
            if (ec2) {
                COMPLETION_ERROR("Failed to post receive buffer on wc complete [error = %1% %2%]", ec2, ec2.message());
                // TODO Error Handling
            }

            // Decrease amount of work
            removeWork(impl);
        });
    } break;

    case WorkType::SEND: {
        dispatcher.post([this, impl, workId, ec] () {
            COMPLETION_LOG("Executing successful send event of id %1%", workId.bufferId());

            impl->handler->onSend(workId.userId(), ec);
            mDevice.releaseSendBuffer(workId.bufferId());

            // Decrease amount of work
            removeWork(impl);
        });
    } break;

    case WorkType::READ: {
        dispatcher.post([this, impl, workId, ec] () {
            COMPLETION_LOG("Executing successful read event of id %1%", workId.bufferId());

            impl->handler->onRead(workId.userId(), ec);

            // Decrease amount of work
            removeWork(impl);
        });
    } break;

    case WorkType::WRITE: {
        dispatcher.post([this, impl, workId, ec] () {
            COMPLETION_LOG("Executing successful read event of id %1%", workId.bufferId());

            impl->handler->onWrite(workId.userId(), ec);

            // Decrease amount of work
            removeWork(impl);
        });
    } break;

    default: {
        COMPLETION_LOG("Unknown work type");
    } break;
    }
}

void CompletionContext::addWork(SocketImplementation* impl) {
    ++(impl->work);
}

void CompletionContext::removeWork(SocketImplementation* impl) {
    auto work = --(impl->work);
    auto state = impl->state.load();
    if (work == 0 && state == ConnectionState::DRAINING) {
        if (!impl->state.compare_exchange_strong(state, ConnectionState::DISCONNECTED)) {
            return;
        }

        boost::system::error_code ec;
        removeConnection(impl, ec);

        // Post on io service?
        impl->handler->onDisconnected();
    }
}

void CompletionContext::processDrainedConnection(EventDispatcher& dispatcher, SocketImplementation* impl,
        boost::system::error_code& ec) {
    auto state = ConnectionState::DISCONNECTING;

    // If the work count is not 0 then the connection has running handlers, we have to set the state to DRAINING
    // so the handlers can cleanup the connection once they are done
    if (impl->work.load() != 0) {
        impl->state.store(ConnectionState::DRAINING);

        // If the work count dropped to zero while setting the new state we might have to cleanup the connection
        if (impl->work.load() != 0) {
            return;
        }
        state = ConnectionState::DRAINING;
    }

    // Try to mark the connection as disconnected, if this fails somebody else already disconnected
    if (!impl->state.compare_exchange_strong(state, ConnectionState::DISCONNECTED)) {
        return;
    }

    // Remove the connection
    removeConnection(impl, ec);
    if (ec) {
        return;
    }

    dispatcher.post([impl] () {
        impl->handler->onDisconnected();
    });
}

void DeviceContext::init(boost::system::error_code& ec) {
    if (mProtectionDomain) {
        return; // Already initialized
    }

    DEVICE_LOG("Allocate protection domain");
    errno = 0;
    mProtectionDomain = ibv_alloc_pd(mVerbs);
    if (mProtectionDomain == nullptr) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }

    initMemoryRegion(ec);
    if (ec) {
        return;
    }

    initReceiveQueue(ec);
    if (ec) {
        return;
    }

    mCompletion.init(ec);
    if (ec) {
        return;
    }

    DEVICE_LOG("Starting event polling");
    mDispatcher.post([this] () {
        doPoll();
    });
}

void DeviceContext::shutdown(boost::system::error_code& ec) {
    if (mShutdown.load()) {
        return;
    }
    mShutdown.store(true);

    DEVICE_LOG("Shutdown completion context");
    mCompletion.shutdown(ec);
    if (ec) {
        return;
    }

    DEVICE_LOG("Destroy shared receive queue");
    if (auto res = ibv_destroy_srq(mReceiveQueue) != 0) {
        ec = boost::system::error_code(res, boost::system::system_category());
        return;
    }

    shutdownMemoryRegion(ec);
    if (ec) {
        return;
    }

    DEVICE_LOG("Destroy protection domain");
    if (auto res = ibv_dealloc_pd(mProtectionDomain) != 0) {
        ec = boost::system::error_code(res, boost::system::system_category());
        return;
    }
}

InfinibandBuffer DeviceContext::acquireSendBuffer(uint32_t length) {
    uint16_t id = 0x0u;
    if (!mSendBufferQueue.pop(id)) {
        return InfinibandBuffer(InfinibandBuffer::INVALID_ID);
    }
    if (length > mBufferLength) {
        return InfinibandBuffer(InfinibandBuffer::INVALID_ID);
    }

    InfinibandBuffer buffer(id);
    buffer.handle()->addr = reinterpret_cast<uintptr_t>(mSendData) + bufferOffset(id);
    buffer.handle()->length = length;
    buffer.handle()->lkey = mDataRegion->lkey;
    return buffer;
}

void DeviceContext::releaseSendBuffer(InfinibandBuffer& buffer) {
    if (buffer.handle()->lkey != mDataRegion->lkey) {
        return;
    }
    releaseSendBuffer(buffer.id());
}

LocalMemoryRegion DeviceContext::registerMemoryRegion(void* data, size_t length, int access,
        boost::system::error_code& ec) {
    DEVICE_LOG("Create memory region at %1%", data);
    errno = 0;
    mDataRegion = ibv_reg_mr(mProtectionDomain, data, length, access);
    if (mDataRegion == nullptr) {
        ec = boost::system::error_code(errno, boost::system::system_category());
    }
    return LocalMemoryRegion(mDataRegion);
}

void DeviceContext::initMemoryRegion(boost::system::error_code& ec) {
    auto dataLength = bufferOffset(mReceiveBufferCount) + bufferOffset(mSendBufferCount);

    DEVICE_LOG("Map %1% bytes of shared buffer space", dataLength);
    errno = 0;
    mReceiveData = mmap(nullptr, dataLength, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
    if (mReceiveData == MAP_FAILED) {
        mReceiveData = nullptr;
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }
    mSendData = reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(mReceiveData) + bufferOffset(mReceiveBufferCount));

    DEVICE_LOG("Create memory region at %1%", mReceiveData);
    errno = 0;
    mDataRegion = ibv_reg_mr(mProtectionDomain, mReceiveData, dataLength, IBV_ACCESS_LOCAL_WRITE);
    if (mDataRegion == nullptr) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }

    DEVICE_LOG("Add %1% buffers to send buffer queue", mSendBufferCount);
    for (uint16_t id = 0x0u; id < mSendBufferCount; ++id) {
        mSendBufferQueue.push(id);
    }
}

void DeviceContext::shutdownMemoryRegion(boost::system::error_code& ec) {
    DEVICE_LOG("Destroy memory region at %1%", mReceiveData);
    if (auto res = ibv_dereg_mr(mDataRegion) != 0) {
        ec = boost::system::error_code(res, boost::system::system_category());
        return;
    }

    auto dataLength = bufferOffset(mReceiveBufferCount) + bufferOffset(mSendBufferCount);

    // TODO Size has to be a multiple of the page size
    DEVICE_LOG("Unmap buffer space at %1%", mReceiveData);
    errno = 0;
    if (munmap(mReceiveData, dataLength) != 0) {
        ec = boost::system::error_code(errno, boost::system::system_category());
    }
    mReceiveData = nullptr;
    mSendData = nullptr;
}

void DeviceContext::initReceiveQueue(boost::system::error_code& ec) {
    DEVICE_LOG("Create shared receive queue");
    struct ibv_srq_init_attr srq_attr;
    memset(&srq_attr, 0, sizeof(srq_attr));
    srq_attr.attr.max_wr = mReceiveBufferCount;
    srq_attr.attr.max_sge = 1;

    errno = 0;
    mReceiveQueue = ibv_create_srq(mProtectionDomain, &srq_attr);
    if (mReceiveQueue == nullptr) {
        ec = boost::system::error_code(errno, boost::system::system_category());
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

void DeviceContext::doPoll() {
    if (mShutdown.load()) {
        return;
    }

    boost::system::error_code ec;
    mCompletion.poll(mDispatcher, ec);
    if (ec) {
        DEVICE_ERROR("Failure while processing completion queue [error = %1% %2%]", ec, ec.message());
    }

    mDispatcher.post([this] () {
        doPoll();
    });
}

void DeviceContext::releaseSendBuffer(uint16_t id) {
    if (id == InfinibandBuffer::INVALID_ID) {
        return;
    }
    mSendBufferQueue.push(id);
}

/**
 * @brief Helper function posting the buffer to the shared receive queue
 */
void DeviceContext::postReceiveBuffer(uint16_t id, boost::system::error_code& ec) {
    WorkRequestId workId(0x0u, id, WorkType::RECEIVE);

    // Prepare work request
    struct ibv_sge sge;
    sge.addr = reinterpret_cast<uintptr_t>(mReceiveData) + bufferOffset(id);
    sge.length = mBufferLength;
    sge.lkey = mDataRegion->lkey;

    struct ibv_recv_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = workId.id();
    wr.sg_list = &sge;
    wr.num_sge = 1;

    // Repost receives on shared queue
    struct ibv_recv_wr* bad_wr = nullptr;
    if (auto res = ibv_post_srq_recv(mReceiveQueue, &wr, &bad_wr)) {
        ec = boost::system::error_code(res, boost::system::system_category());
        return;
    }
}

} // namespace infinio
} // namespace crossbow
