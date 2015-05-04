#include "DeviceContext.hpp"

#include <crossbow/infinio/EventDispatcher.hpp>
#include <crossbow/infinio/InfinibandSocket.hpp>

#include "AddressHelper.hpp"
#include "ErrorCode.hpp"
#include "InfinibandSocketImpl.hpp"
#include "Logging.hpp"

#include <cerrno>
#include <cstring>
#include <vector>

#define COMPLETION_LOG(...) INFINIO_LOG("[CompletionContext] " __VA_ARGS__)
#define DEVICE_LOG(...) INFINIO_LOG("[DeviceContext] " __VA_ARGS__)

namespace crossbow {
namespace infinio {

namespace {

/**
 * @brief Helper function posting the buffer to the shared receive queue
 */
void postReceiveBuffer(struct ibv_srq* srq, InfinibandBuffer& buffer, boost::system::error_code& ec) {
    // Prepare work request
    struct ibv_recv_wr wr;
    memset(&wr, 0, sizeof(wr));
    wr.wr_id = ((buffer.id() << 1) | 0x1u);
    wr.sg_list = buffer.handle();
    wr.num_sge = 1;

    // Repost receives on shared queue
    struct ibv_recv_wr* bad_wr = nullptr;
    if (auto res = ibv_post_srq_recv(srq, &wr, &bad_wr)) {
        ec = boost::system::error_code(res, boost::system::system_category());
        return;
    }
}

} // anonymous namespace

void CompletionContext::init(struct ibv_pd* protectionDomain, struct ibv_srq* receiveQueue,
        boost::system::error_code& ec) {
    if (mCompletionQueue) {
        ec = error::already_initialized;
        return;
    }

    mProtectionDomain = protectionDomain;
    mReceiveQueue = receiveQueue;

    COMPLETION_LOG("Create completion queue");
    errno = 0;
    mCompletionQueue = ibv_create_cq(mProtectionDomain->context, mCompletionQueueLength, nullptr, nullptr, 0);
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
    qp_attr.srq = mReceiveQueue;
    qp_attr.cap.max_send_wr = mSendQueueLength;
    qp_attr.cap.max_send_sge = 1;
    qp_attr.qp_type = IBV_QPT_RC;
    qp_attr.comp_mask = IBV_QP_INIT_ATTR_PD;
    qp_attr.pd = mProtectionDomain;

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

    // wc->opcode is not valid when status is not success - so we use the LSB of the wr_id to encode if the buffer came
    // from a send or a receive (this is important so that we can repost the receive buffer to the shared queue)
    auto op = (wc->wr_id & 0x1u);
    auto bufferid = (wc->wr_id >> 1);
    uint32_t byte_len = 0x0u;
    boost::system::error_code ec;

    auto i = mSocketMap.at(wc->qp_num);
    if (!i.first) {
        COMPLETION_LOG("No matching socket for qp_num %1%", wc->qp_num);

        // In the case that we have no socket associated with the qp_num we just repost the buffer to the shared receive
        // queue or release the buffer in the case of send
        if (op == 0x1u) {
            auto buffer = mBufferManager.acquireBuffer(bufferid, mBufferManager.bufferLength());
            postReceiveBuffer(mReceiveQueue, buffer, ec);
            if (ec) {
                mBufferManager.releaseBuffer(bufferid);
                COMPLETION_LOG("Failed to post receive buffer on wc complete [errno = %1% - %2%]", ec, ec.message());
                // TODO Error Handling
            }
        } else {
            mBufferManager.releaseBuffer(bufferid);
        }

        return;
    }
    SocketImplementation* impl = i.second;

    // TODO Better error handling
    // Right now we just propagate the error to the connection
    if (wc->status != IBV_WC_SUCCESS) {
        ec = boost::system::error_code(wc->status, error::get_work_completion_category());
    } else {
        byte_len = wc->byte_len;
        if (op == 0x0u && wc->opcode != IBV_WC_SEND) {
            COMPLETION_LOG("Send buffer but opcode %1% not send", wc->opcode);
            // TODO Send buffer but opcode not send
        }
        if (op == 0x1u && wc->opcode != IBV_WC_RECV) {
            COMPLETION_LOG("Receive buffer but opcode %1% not receive", wc->opcode);
            // TODO receive buffer but opcode not receive
        }
    }

    // Increase amount of work
    addWork(impl);

    // Receive
    if (op == 0x1u) {
        dispatcher.post([this, impl, bufferid, byte_len, ec] () {
            COMPLETION_LOG("Executing successful receive event of id %1%", bufferid);

            auto buffer = mBufferManager.acquireBuffer(bufferid, mBufferManager.bufferLength());
            impl->handler->onReceive(buffer, byte_len, ec);

            boost::system::error_code ec2;
            postReceiveBuffer(mReceiveQueue, buffer, ec2);
            if (ec2) {
                mBufferManager.releaseBuffer(bufferid);
                COMPLETION_LOG("Failed to post receive buffer on wc complete [errno = %1% - %2%]", ec2, ec2.message());
                // TODO Error Handling
            }

            // Decrease amount of work
            removeWork(impl);
        });
    } else {
        dispatcher.post([this, impl, bufferid, byte_len, ec] () {
            COMPLETION_LOG("Executing successful send event of id %1%", bufferid);

            auto buffer = mBufferManager.acquireBuffer(bufferid, mBufferManager.bufferLength());
            impl->handler->onSend(buffer, byte_len, ec);

            mBufferManager.releaseBuffer(bufferid);

            // Decrease amount of work
            removeWork(impl);
        });
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

    DEVICE_LOG("Create shared receive queue");
    struct ibv_srq_init_attr srq_attr;
    memset(&srq_attr, 0, sizeof(srq_attr));
    srq_attr.attr.max_wr = mReceiveQueueLength;
    srq_attr.attr.max_sge = 1;

    errno = 0;
    mReceiveQueue = ibv_create_srq(mProtectionDomain, &srq_attr);
    if (mReceiveQueue == nullptr) {
        ec = boost::system::error_code(errno, boost::system::system_category());
        return;
    }

    DEVICE_LOG("Initialize buffer manager");
    mBufferManager.init(mProtectionDomain, ec);
    if (ec) {
        return;
    }

    DEVICE_LOG("Post %1% buffers to shared receive queue", mReceiveQueueLength);
    for (size_t i = 0; i < mReceiveQueueLength; ++i) {
        auto buffer = mBufferManager.acquireBuffer(mBufferManager.bufferLength());
        postReceiveBuffer(mReceiveQueue, buffer, ec);
        if (ec) {
            mBufferManager.releaseBuffer(buffer.id());
            return;
        }
    }

    DEVICE_LOG("Initialize completion context");
    mCompletion.init(mProtectionDomain, mReceiveQueue, ec);

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

    DEVICE_LOG("Shutdown buffer manager");
    mBufferManager.shutdown(ec);
    if (ec) {
        return;
    }

    DEVICE_LOG("Destroy protection domain");
    if (auto res = ibv_dealloc_pd(mProtectionDomain) != 0) {
        ec = boost::system::error_code(res, boost::system::system_category());
        return;
    }
}

void DeviceContext::doPoll() {
    if (mShutdown.load()) {
        return;
    }

    boost::system::error_code ec;
    mCompletion.poll(mDispatcher, ec);
    if (ec) {
        DEVICE_LOG("Failure while processing completion queue [errcode = %1% %2%]", ec, ec.message());
    }

    mDispatcher.post([this] () {
        doPoll();
    });
}

} // namespace infinio
} // namespace crossbow
