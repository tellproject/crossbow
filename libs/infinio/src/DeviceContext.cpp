#include "DeviceContext.hpp"

#include <crossbow/infinio/ErrorCode.hpp>
#include <crossbow/logger.hpp>

#include "AddressHelper.hpp"
#include "WorkRequestId.hpp"

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <sys/mman.h>

namespace crossbow {
namespace infinio {

MmapRegion::MmapRegion(size_t length)
        : mData(mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, 0, 0)),
          mLength(length) {
    // TODO Size might have to be a multiple of the page size
    if (mData == MAP_FAILED) {
        throw std::system_error(errno, std::generic_category());
    }
    LOG_TRACE("Mapped %1% bytes of buffer space", mLength);
}

MmapRegion::~MmapRegion() {
    if (mData != nullptr && munmap(mData, mLength)) {
        std::error_code ec(errno, std::generic_category());
        LOG_ERROR("Failed to unmap memory region [error = %1% %2%]", ec, ec.message());
    }
}

MmapRegion& MmapRegion::operator=(MmapRegion&& other) {
    if (mData != nullptr && munmap(mData, mLength)) {
        throw std::system_error(errno, std::generic_category());
    }

    mData = other.mData;
    other.mData = nullptr;
    mLength = other.mLength;
    other.mLength = 0;
    return *this;
}

ProtectionDomain::ProtectionDomain(ibv_context* context)
        : mDomain(ibv_alloc_pd(context)) {
    if (mDomain == nullptr) {
        throw std::system_error(errno, std::generic_category());
    }
    LOG_TRACE("Allocated protection domain");
}

ProtectionDomain::~ProtectionDomain() {
    if (mDomain != nullptr && ibv_dealloc_pd(mDomain)) {
        std::error_code ec(errno, std::generic_category());
        LOG_ERROR("Failed to deallocate protection domain [error = %1% %2%]", ec, ec.message());
    }
}

ProtectionDomain& ProtectionDomain::operator=(ProtectionDomain&& other) {
    if (mDomain != nullptr && ibv_dealloc_pd(mDomain)) {
        throw std::system_error(errno, std::generic_category());
    }

    mDomain = other.mDomain;
    other.mDomain = nullptr;
    return *this;
}

SharedReceiveQueue::SharedReceiveQueue(const ProtectionDomain& domain, uint32_t length) {
    struct ibv_srq_init_attr srq_attr;
    memset(&srq_attr, 0, sizeof(srq_attr));
    srq_attr.attr.max_wr = length;
    srq_attr.attr.max_sge = 1;

    mQueue = ibv_create_srq(domain.get(), &srq_attr);
    if (mQueue == nullptr) {
        throw std::system_error(errno, std::generic_category());
    }
    LOG_TRACE("Created shared receive queue");
}

SharedReceiveQueue::~SharedReceiveQueue() {
    if (mQueue != nullptr && ibv_destroy_srq(mQueue)) {
        std::error_code ec(errno, std::generic_category());
        LOG_ERROR("[SharedReceiveQueue] Failed to destroy receive queue [error = %1% %2%]", ec, ec.message());
    }
}

SharedReceiveQueue& SharedReceiveQueue::operator=(SharedReceiveQueue&& other) {
    if (mQueue != nullptr && ibv_destroy_srq(mQueue)) {
        throw std::system_error(errno, std::generic_category());
    }

    mQueue = other.mQueue;
    other.mQueue = nullptr;
    return *this;
}

void SharedReceiveQueue::postBuffer(InfinibandBuffer& buffer, std::error_code& ec) {
    if (!buffer.valid()) {
        ec = error::invalid_buffer;
        return;
    }
    WorkRequestId workId(0x0u, buffer.id(), WorkType::RECEIVE);

    // Prepare work request
    struct ibv_recv_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = workId.id();
    wr.sg_list = buffer.handle();
    wr.num_sge = 1;

    // Repost receives on shared queue
    struct ibv_recv_wr* bad_wr = nullptr;
    if (ibv_post_srq_recv(mQueue, &wr, &bad_wr)) {
        ec = std::error_code(errno, std::generic_category());
        return;
    }
}

CompletionChannel::CompletionChannel(ibv_context* context)
        : mChannel(ibv_create_comp_channel(context)) {
    if (mChannel == nullptr) {
        throw std::system_error(errno, std::generic_category());
    }
    LOG_TRACE("Created completion channel");
}

CompletionChannel::~CompletionChannel() {
    if (mChannel != nullptr && ibv_destroy_comp_channel(mChannel)) {
        std::error_code ec(errno, std::generic_category());
        LOG_ERROR("Failed to destroy completion channel [error = %1% %2%]", ec, ec.message());
    }
}

CompletionChannel& CompletionChannel::operator=(CompletionChannel&& other) {
    if (mChannel != nullptr && ibv_destroy_comp_channel(mChannel)) {
        throw std::system_error(errno, std::generic_category());
    }

    mChannel = other.mChannel;
    other.mChannel = nullptr;
    return *this;
}

void CompletionChannel::nonBlocking(bool mode) {
    auto flags = fcntl(mChannel->fd, F_GETFL);
    if (fcntl(mChannel->fd, F_SETFL, (mode ? flags | O_NONBLOCK : flags & ~O_NONBLOCK))) {
        throw std::system_error(errno, std::generic_category());
    }
}

int CompletionChannel::retrieveEvents(ibv_cq** cq) {
    void* context;
    return ibv_get_cq_event(mChannel, cq, &context);
}

CompletionQueue::CompletionQueue(ibv_context* context, const CompletionChannel& channel, int length)
        : mQueue(ibv_create_cq(context, length, nullptr, channel.get(), 0)) {
    if (mQueue == nullptr) {
        throw std::system_error(errno, std::generic_category());
    }
    LOG_TRACE("Created completion queue");
}

CompletionQueue::~CompletionQueue() {
    if (mQueue != nullptr && ibv_destroy_cq(mQueue)) {
        std::error_code ec(errno, std::generic_category());
        LOG_ERROR("Failed to destroy completion queue [error = %1% %2%]", ec, ec.message());
    }
}

CompletionQueue& CompletionQueue::operator=(CompletionQueue&& other) {
    if (mQueue != nullptr && ibv_destroy_cq(mQueue)) {
        throw std::system_error(errno, std::generic_category());
    }

    mQueue = other.mQueue;
    other.mQueue = nullptr;
    return *this;
}

void CompletionQueue::requestEvent(std::error_code& ec) {
    if (ibv_req_notify_cq(mQueue, 0)) {
        ec = std::error_code(errno, std::generic_category());
        return;
    }
}

CompletionContext::CompletionContext(EventProcessor& processor, std::shared_ptr<DeviceContext> device,
        const InfinibandLimits& limits)
        : mProcessor(processor),
          mDevice(std::move(device)),
          mSendBufferCount(limits.sendBufferCount),
          mSendBufferLength(limits.bufferLength),
          mSendQueueLength(limits.sendQueueLength),
          mMaxScatterGather(limits.maxScatterGather),
          mCompletionQueueLength(limits.completionQueueLength),
          mSendData(static_cast<size_t>(mSendBufferCount) * static_cast<size_t>(mSendBufferLength)),
          mSendDataRegion(mDevice->registerMemoryRegion(mSendData, IBV_ACCESS_LOCAL_WRITE)),
          mCompletionChannel(mDevice->createCompletionChannel()),
          mCompletionQueue(mDevice->createCompletionQueue(mCompletionChannel, mCompletionQueueLength)),
          mSleeping(false),
          mShutdown(false) {
    mSocketMap.set_empty_key(0x1u << 25);
    mSocketMap.set_deleted_key(0x1u << 26);

    mCompletionChannel.nonBlocking(true);
    mProcessor.registerPoll(mCompletionChannel.fd(), this);

    LOG_TRACE("Add %1% buffers to send buffer queue", mSendBufferCount);
    for (decltype(mSendBufferCount) id = 0; id < mSendBufferCount; ++id) {
        mSendBufferQueue.push(id);
    }
}

CompletionContext::~CompletionContext() {
    try {
        mProcessor.deregisterPoll(mCompletionChannel.fd(), this);
    } catch (std::system_error& e) {
        LOG_ERROR("Failed to deregister from EventProcessor [error = %1% %2%]", e.code(), e.what());
    }
}

void CompletionContext::shutdown() {
    mShutdown.store(true);
    // TODO Implement correctly
}

void CompletionContext::addConnection(struct rdma_cm_id* id, InfinibandSocket socket) {
    struct ibv_qp_init_attr_ex qp_attr;
    memset(&qp_attr, 0, sizeof(qp_attr));
    qp_attr.send_cq = mCompletionQueue.get();
    qp_attr.recv_cq = mCompletionQueue.get();
    qp_attr.srq = mDevice->receiveQueue();
    qp_attr.cap.max_send_wr = mSendQueueLength;
    qp_attr.cap.max_send_sge = mMaxScatterGather;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.comp_mask = IBV_QP_INIT_ATTR_PD;
    qp_attr.pd = mDevice->protectionDomain();

    LOG_TRACE("%1%: Creating queue pair", formatRemoteAddress(id));
    if (rdma_create_qp_ex(id, &qp_attr)) {
        throw std::error_code(errno, std::generic_category());
    }

    // TODO Make this an assertion
    if (id->qp->qp_num >= (0x1u << 25)) {
        LOG_ERROR("QP number is larger than 24 bits");
    }

    if (!mSocketMap.insert(std::make_pair(id->qp->qp_num, std::move(socket))).second) {
        // TODO Insert failed
        return;
    }
}

void CompletionContext::drainConnection(InfinibandSocket socket) {
    mDrainingQueue.emplace_back(std::move(socket));
}

void CompletionContext::removeConnection(struct rdma_cm_id* id) {
    if (id->qp == nullptr) {
        // Queue Pair already destroyed
        return;
    }

    LOG_TRACE("%1%: Destroying queue pair", formatRemoteAddress(id));
    mSocketMap.erase(id->qp->qp_num);
    rdma_destroy_qp(id);
}

InfinibandBuffer CompletionContext::acquireSendBuffer(uint32_t length) {
    if (mSendBufferQueue.empty()) {
        return InfinibandBuffer(InfinibandBuffer::INVALID_ID);
    }
    if (length > mSendBufferLength) {
        return InfinibandBuffer(InfinibandBuffer::INVALID_ID);
    }

    auto id = mSendBufferQueue.top();
    mSendBufferQueue.pop();
    auto offset = static_cast<size_t>(id) * static_cast<size_t>(mSendBufferLength);
    return mSendDataRegion.acquireBuffer(id, offset, length);
}

void CompletionContext::releaseSendBuffer(InfinibandBuffer& buffer) {
    if (!mSendDataRegion.belongsToRegion(buffer)) {
        LOG_ERROR("Trying to release send buffer registered to another region");
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

bool CompletionContext::poll() {
    std::vector<InfinibandSocket> draining;
    struct ibv_wc wc[mCompletionQueueLength];

    if (!mDrainingQueue.empty()) {
        draining.swap(mDrainingQueue);
    }

    // Poll the completion queue
    auto num = mCompletionQueue.poll(mCompletionQueueLength, wc);

    // Check if polling the completion queue failed
    if (num < 0 && !mShutdown.load()) {
        LOG_ERROR("Polling completion queue failed [error = %1% %2%]", errno, strerror(errno));
    }

    // Process all polled work completions
    for (int i = 0; i < num; ++i) {
        processWorkComplete(&wc[i]);
    }

    // Process all drained connections
    for (auto& socket: draining) {
        socket->onDrained();
    }
    draining.clear();

    return (num > 0);
}

void CompletionContext::prepareSleep() {
    if (mSleeping) {
        return;
    }

    LOG_TRACE("Activating completion channel");
    std::error_code ec;
    mCompletionQueue.requestEvent(ec);
    if (ec) {
        LOG_ERROR("Error while requesting completion queue notification [error = %1% %2%]", ec, ec.message());
        // TODO Error handling
        std::terminate();
    }
    mSleeping = true;

    // Consume work completions that arrived between the last poll and the activation of the completion channel
    poll();
}

void CompletionContext::wakeup() {
    LOG_TRACE("Completion channel ready");
    struct ibv_cq* cq;
    int num = 0;
    while (mCompletionChannel.retrieveEvents(&cq) == 0) {
        if (cq != mCompletionQueue.get()) {
            LOG_ERROR("Unknown completion queue");
            break;
        }
        ++num;
    }
    if (num > 0) {
        mCompletionQueue.ackEvents(num);
    }
    mSleeping = false;
}

void CompletionContext::processWorkComplete(struct ibv_wc* wc) {
    LOG_TRACE("Processing WC with ID %1% on queue %2% with status %3% %4%", wc->wr_id, wc->qp_num, wc->status,
            ibv_wc_status_str(wc->status));

    WorkRequestId workId(wc->wr_id);
    std::error_code ec;

    auto i = mSocketMap.find(wc->qp_num);
    if (i == mSocketMap.end()) {
        LOG_ERROR("No matching socket for qp_num %1%", wc->qp_num);

        // In the case that we have no socket associated with the qp_num we just repost the buffer to the shared receive
        // queue or release the buffer in the case of send
        switch (workId.workType()) {

        // In the case the work request was a receive, we try to repost the shared receive buffer
        case WorkType::RECEIVE: {
            mDevice->postReceiveBuffer(workId.bufferId());
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
    InfinibandSocketImpl* socket = i->second.get();

    if (wc->status != IBV_WC_SUCCESS) {
        ec = std::error_code(wc->status, error::get_work_completion_category());
    } else {
        assert(workId.workType() != WorkType::RECEIVE || wc->opcode & IBV_WC_RECV);
        assert(workId.workType() != WorkType::SEND || wc->opcode == IBV_WC_SEND);
        assert(workId.workType() != WorkType::READ || wc->opcode == IBV_WC_RDMA_READ);
        assert(workId.workType() != WorkType::WRITE || wc->opcode == IBV_WC_RDMA_WRITE);
    }

    switch (workId.workType()) {
    case WorkType::RECEIVE: {
        LOG_TRACE("Executing receive event of buffer %1%", workId.bufferId());
        auto buffer = mDevice->acquireReceiveBuffer(workId.bufferId());
        if (!buffer.valid()) {
            socket->onReceive(nullptr, 0x0u, error::invalid_buffer);
            break;
        }

        if (wc->opcode == IBV_WC_RECV_RDMA_WITH_IMM) {
            socket->onImmediate(ntohl(wc->imm_data));
        } else {
            socket->onReceive(buffer.data(), wc->byte_len, ec);
        }
        mDevice->postReceiveBuffer(buffer);
    } break;

    case WorkType::SEND: {
        LOG_TRACE("Executing send event of buffer %1%", workId.bufferId());
        socket->onSend(workId.userId(), ec);
        releaseSendBuffer(workId.bufferId());
    } break;

    case WorkType::READ: {
        LOG_TRACE("Executing read event of buffer %1%", workId.bufferId());
        socket->onRead(workId.userId(), workId.bufferId(), ec);
    } break;

    case WorkType::WRITE: {
        LOG_TRACE("Executing write event of buffer %1%", workId.bufferId());
        socket->onWrite(workId.userId(), workId.bufferId(), ec);
    } break;

    default: {
        LOG_TRACE("Unknown work type");
    } break;
    }
}

DeviceContext::DeviceContext(const InfinibandLimits& limits, ibv_context* verbs)
        : mReceiveBufferCount(limits.receiveBufferCount),
          mReceiveBufferLength(limits.bufferLength),
          mVerbs(verbs),
          mProtectionDomain(mVerbs),
          mReceiveData(static_cast<size_t>(mReceiveBufferCount) * static_cast<size_t>(mReceiveBufferLength)),
          mReceiveDataRegion(mProtectionDomain, mReceiveData, IBV_ACCESS_LOCAL_WRITE),
          mReceiveQueue(mProtectionDomain, mReceiveBufferCount),
          mShutdown(false) {
    LOG_TRACE("Post %1% buffers to shared receive queue", mReceiveBufferCount);
    std::error_code ec;
    for (decltype(mReceiveBufferCount) id = 0; id < mReceiveBufferCount; ++id) {
        auto buffer = acquireReceiveBuffer(id);
        mReceiveQueue.postBuffer(buffer, ec);
        if (ec) {
            throw std::system_error(ec);
        }
    }
}

void DeviceContext::shutdown() {
    if (mShutdown.load()) {
        return;
    }
    mShutdown.store(true);

    // TODO Implement
}

void DeviceContext::postReceiveBuffer(InfinibandBuffer& buffer) {
    std::error_code ec;
    mReceiveQueue.postBuffer(buffer, ec);
    if (ec) {
        LOG_ERROR("Failed to post receive buffer [error = %1% %2%]", ec, ec.message());
    }
}

} // namespace infinio
} // namespace crossbow
