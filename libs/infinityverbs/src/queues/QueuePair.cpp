/*
 * QueuePair.cpp
 *
 *  Created on: Dec 10, 2014
 *      Author: claudeb
 */

#include "QueuePair.hpp"

#include <random>
#include <string.h>
#include <limits>

#include "../core/Configuration.hpp"

namespace infinityverbs {
namespace queues {

#define MAX(a,b) ((a) > (b) ? (a) : (b))

QueuePair::QueuePair(infinityverbs::core::Context *context) {

	this->context = context;

	ibv_qp_init_attr qpInitAttributes;
	memset(&qpInitAttributes, 0, sizeof(qpInitAttributes));

	qpInitAttributes.send_cq = context->getSendCompletionQueue();
	qpInitAttributes.recv_cq = context->getReceiveCompletionQueue();
	qpInitAttributes.srq = context->getSharedReceiveQueue();
	qpInitAttributes.cap.max_send_wr = MAX(context->getSendQueueLength(), 1);
	qpInitAttributes.cap.max_send_sge = 1;
	qpInitAttributes.cap.max_recv_wr = MAX(context->getReceiveQueueLength(), 1);
	qpInitAttributes.cap.max_recv_sge = 1;
	qpInitAttributes.qp_type = IBV_QPT_RC;
	qpInitAttributes.sq_sig_all = 0;

	this->ibvQueuePair = ibv_create_qp(context->getInfiniBandProtectionDomain(),
			&(qpInitAttributes));
	INFINITY_VERBS_ASSERT(this->ibvQueuePair != NULL,
			"[INFINITYVERBS][QUEUES][QUEUEPAIR] Cannot create queue pair.\n");

	ibv_qp_attr qpAttributes;
	memset(&qpAttributes, 0, sizeof(qpAttributes));

	qpAttributes.qp_state = IBV_QPS_INIT;
	qpAttributes.pkey_index = 0;
	qpAttributes.port_num = context->getLocalDevicePort();
	qpAttributes.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_ATOMIC;

	int32_t returnValue = ibv_modify_qp(this->ibvQueuePair, &(qpAttributes),
			IBV_QP_STATE | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS | IBV_QP_PKEY_INDEX);

	INFINITY_VERBS_ASSERT(returnValue == 0,
			"[INFINITYVERBS][QUEUES][QUEUEPAIR] Cannot transition to INIT state.\n");

	std::random_device randomGenerator;
	this->sequenceNumber = randomGenerator();

	this->localUserTokenSet = false;
	this->localUserToken =  std::numeric_limits<uint32_t>::max();

	this->remoteUserTokenSet = false;
	this->remoteUserToken =  std::numeric_limits<uint32_t>::max();

	this->isRemoteMemoryStoreActive = false;
	this->remoteMemoryStoreAddress = (uint64_t) NULL;
	this->remoteMemoryStoreKey = (uint32_t) NULL;
	this->remoteMemoryStoreSize = (uint32_t) NULL;
	this->remoteMemoryStoreCache = NULL;
	this->remoteMemoryStoreCacheMR = NULL;

}

QueuePair::~QueuePair() {

	if(isRemoteMemoryStoreActive) {
		int32_t returnValue = ibv_dereg_mr(this->remoteMemoryStoreCacheMR);
		INFINITY_VERBS_ASSERT(returnValue == 0,
					"[INFINITYVERBS][QUEUES][QUEUEPAIR] Cannot delete MR for memory region store.\n");
		free(remoteMemoryStoreCache);
	}

	int32_t returnValue = ibv_destroy_qp(this->ibvQueuePair);
	INFINITY_VERBS_ASSERT(returnValue == 0,
			"[INFINITYVERBS][QUEUES][QUEUEPAIR] Cannot delete queue pair.\n");

}

void QueuePair::activate(uint16_t remoteDeviceId, uint32_t remoteQueuePairNumber,
		uint32_t remoteSequenceNumber) {

	ibv_qp_attr qpAttributes;
	memset(&(qpAttributes), 0, sizeof(qpAttributes));

	qpAttributes.qp_state = IBV_QPS_RTR;
	qpAttributes.path_mtu = IBV_MTU_4096;
	qpAttributes.dest_qp_num = remoteQueuePairNumber;
	qpAttributes.rq_psn = remoteSequenceNumber;
	qpAttributes.max_dest_rd_atomic = 1;
	qpAttributes.min_rnr_timer = 12;
	qpAttributes.ah_attr.is_global = 0;
	qpAttributes.ah_attr.dlid = remoteDeviceId;
	qpAttributes.ah_attr.sl = 0;
	qpAttributes.ah_attr.src_path_bits = 0;

	int32_t returnValue = ibv_modify_qp(this->ibvQueuePair, &qpAttributes,
			IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | IBV_QP_RQ_PSN
					| IBV_QP_MIN_RNR_TIMER | IBV_QP_MAX_DEST_RD_ATOMIC);

	INFINITY_VERBS_ASSERT(returnValue == 0,
			"[INFINITYVERBS][QUEUES][QUEUEPAIR] Cannot transition to RTR state.\n");

	qpAttributes.qp_state = IBV_QPS_RTS;
	qpAttributes.timeout = 14;
	qpAttributes.retry_cnt = 7;
	qpAttributes.rnr_retry = 7;
	qpAttributes.sq_psn = this->getSequenceNumber();
	qpAttributes.max_rd_atomic = 1;

	returnValue = ibv_modify_qp(this->ibvQueuePair, &qpAttributes,
			IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN
					| IBV_QP_MAX_QP_RD_ATOMIC);

	INFINITY_VERBS_ASSERT(returnValue == 0,
			"[INFINITYVERBS][QUEUES][QUEUEPAIR] Cannot transition to RTS state.\n");

}

uint16_t QueuePair::getLocalDeviceId() {

	return this->context->getLocalDeviceId();

}

uint32_t QueuePair::getQueuePairNumber() {

	return this->ibvQueuePair->qp_num;

}

uint32_t QueuePair::getSequenceNumber() {

	return this->sequenceNumber;

}

bool QueuePair::isLocalUserTokenSet() {
	return this->localUserTokenSet;
}

void QueuePair::setLocalUserToken(uint32_t userToken) {
	this->localUserToken = userToken;
	this->localUserTokenSet = true;
}

uint32_t QueuePair::getLocalUserToken() {
	return this->localUserToken;
}

bool QueuePair::isRemoteUserTokenSet() {
	return this->remoteUserTokenSet;
}

void QueuePair::setRemoteUserToken(uint32_t userToken) {
	this->remoteUserToken = userToken;
	this->remoteUserTokenSet = true;
}

uint32_t QueuePair::getRemoteUserToken() {
	return this->remoteUserToken;
}

void QueuePair::activateRemoteMemoryTokenStore(uint64_t address, uint32_t key, uint32_t storeSize) {
	INFINITY_VERBS_DEBUG_STATUS(1, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Store detected at address %lu and key %d\n", address, key);
	this->remoteMemoryStoreAddress = address;
	this->remoteMemoryStoreKey = key;
	this->remoteMemoryStoreSize = storeSize;
	this->remoteMemoryStoreCache = (infinityverbs::core::SerializedMemoryToken *) calloc(storeSize, sizeof(infinityverbs::core::SerializedMemoryToken));
	this->remoteMemoryStoreCacheMR = ibv_reg_mr(this->context->getInfiniBandProtectionDomain(), this->remoteMemoryStoreCache, storeSize * sizeof(infinityverbs::core::SerializedMemoryToken), IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);
	this->isRemoteMemoryStoreActive = true;
}

bool QueuePair::isRemoteMemoryTokenStoreActive() {
	return isRemoteMemoryStoreActive;
}

opid_t QueuePair::synchronizeRemoteMemoryTokenStore() {

	if(isRemoteMemoryStoreActive) {

		uint64_t requestId = this->context->getNextOperationId();

		struct ibv_sge sgElement;
		struct ibv_send_wr workRequest;
		struct ibv_send_wr *badWorkRequest;

		memset(&sgElement, 0, sizeof(ibv_sge));
		sgElement.addr = (uint64_t) this->remoteMemoryStoreCache;
		sgElement.length = this->remoteMemoryStoreSize * sizeof(infinityverbs::core::SerializedMemoryToken);
		sgElement.lkey = this->remoteMemoryStoreCacheMR->lkey;

		memset(&workRequest, 0, sizeof(ibv_send_wr));
		workRequest.wr_id = requestId;
		workRequest.sg_list = &sgElement;
		workRequest.num_sge = 1;
		workRequest.opcode = IBV_WR_RDMA_READ;
		workRequest.send_flags = IBV_SEND_SIGNALED;
		workRequest.wr.rdma.remote_addr = this->remoteMemoryStoreAddress;
		workRequest.wr.rdma.rkey = this->remoteMemoryStoreKey;

		int returnValue = ibv_post_send(this->ibvQueuePair, &workRequest, &badWorkRequest);

		INFINITY_VERBS_ASSERT(returnValue == 0, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Posting read request to synchronize token store failed.\n");

		INFINITY_VERBS_DEBUG_STATUS(1, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Synchronizing token store (request %lu)\n", requestId);

		return requestId;

	} else {
		INFINITY_VERBS_ASSERT(false, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Called synchronizeRemoteMemoryTokenStore() but remote memory store was not discovered.\n");
	}

}

infinityverbs::memory::MemoryToken* QueuePair::getRemoteMemoryToken(uint32_t userToken) {
	for(uint32_t i=0; i<remoteMemoryStoreSize; ++i) {
		if(remoteMemoryStoreCache[i].enabled && remoteMemoryStoreCache[i].userToken == userToken) {
			return new infinityverbs::memory::MemoryToken(remoteMemoryStoreCache[i].userToken, this, remoteMemoryStoreCache[i].memoryRegionType, remoteMemoryStoreCache[i].address, remoteMemoryStoreCache[i].key, remoteMemoryStoreCache[i].sizeInBytes);
		}
	}
	return NULL;
}

bool QueuePair::isRemoteMemoryTokenValid(infinityverbs::memory::MemoryToken* memoryToken) {
	for(uint32_t i=0; i<remoteMemoryStoreSize; ++i) {
		if(remoteMemoryStoreCache[i].userToken == memoryToken->getUserToken()) {
			return remoteMemoryStoreCache[i].enabled;
		}
	}
	return false;
}


opid_t QueuePair::send(infinityverbs::memory::RdmaBuffer* buffer) {

	uint64_t requestId = this->context->getNextOperationId();

	struct ibv_sge sgElement;
	struct ibv_send_wr workRequest;
	struct ibv_send_wr *badWorkRequest;

	memset(&sgElement, 0, sizeof(ibv_sge));
	sgElement.addr = buffer->getAddress();
	sgElement.length = buffer->getSizeInBytes();
	sgElement.lkey = buffer->getKey();

	memset(&workRequest, 0, sizeof(ibv_send_wr));
	workRequest.wr_id = requestId;
	workRequest.sg_list = &sgElement;
	workRequest.num_sge = 1;
	workRequest.opcode = IBV_WR_SEND;
	workRequest.send_flags = IBV_SEND_SIGNALED;

	int returnValue = ibv_post_send(this->ibvQueuePair, &workRequest, &badWorkRequest);

	INFINITY_VERBS_ASSERT(returnValue == 0, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Posting send request failed.\n");

	INFINITY_VERBS_DEBUG_STATUS(1, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Sending to remote machine (request %lu)\n", requestId);

	return requestId;

}

opid_t QueuePair::write(infinityverbs::memory::RdmaBuffer* buffer,
		infinityverbs::memory::MemoryToken* destination) {

	uint64_t requestId = this->context->getNextOperationId();

	struct ibv_sge sgElement;
	struct ibv_send_wr workRequest;
	struct ibv_send_wr *badWorkRequest;

	memset(&sgElement, 0, sizeof(ibv_sge));
	sgElement.addr = buffer->getAddress();
	sgElement.length = buffer->getSizeInBytes();
	sgElement.lkey = buffer->getKey();

	memset(&workRequest, 0, sizeof(ibv_send_wr));
	workRequest.wr_id = requestId;
	workRequest.sg_list = &sgElement;
	workRequest.num_sge = 1;
	workRequest.opcode = IBV_WR_RDMA_WRITE;
	workRequest.send_flags = IBV_SEND_SIGNALED;
	workRequest.wr.rdma.remote_addr = destination->getAddress();
	workRequest.wr.rdma.rkey = destination->getKey();

	int returnValue = ibv_post_send(this->ibvQueuePair, &workRequest, &badWorkRequest);

	INFINITY_VERBS_ASSERT(returnValue == 0, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Posting write request failed.\n");

	INFINITY_VERBS_DEBUG_STATUS(1, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Writing to remote memory (request %lu)\n", requestId);

	return requestId;

}

opid_t QueuePair::read(infinityverbs::memory::RdmaBuffer* buffer,
		infinityverbs::memory::MemoryToken* source) {

	uint64_t requestId = this->context->getNextOperationId();

	struct ibv_sge sgElement;
	struct ibv_send_wr workRequest;
	struct ibv_send_wr *badWorkRequest;

	memset(&sgElement, 0, sizeof(ibv_sge));
	sgElement.addr = buffer->getAddress();
	sgElement.length = buffer->getSizeInBytes();
	sgElement.lkey = buffer->getKey();

	memset(&workRequest, 0, sizeof(ibv_send_wr));
	workRequest.wr_id = requestId;
	workRequest.sg_list = &sgElement;
	workRequest.num_sge = 1;
	workRequest.opcode = IBV_WR_RDMA_READ;
	workRequest.send_flags = IBV_SEND_SIGNALED;
	workRequest.wr.rdma.remote_addr = source->getAddress();
	workRequest.wr.rdma.rkey = source->getKey();

	int returnValue = ibv_post_send(this->ibvQueuePair, &workRequest, &badWorkRequest);

	INFINITY_VERBS_ASSERT(returnValue == 0, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Posting read request failed.\n");

	INFINITY_VERBS_DEBUG_STATUS(1, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Reading from remote memory (request %lu)\n", requestId);

	return requestId;

}

opid_t QueuePair::compareAndSwap(infinityverbs::memory::MemoryToken* destination,
		infinityverbs::memory::AtomicValue* previousValue, uint64_t compare, uint64_t swap) {

	uint64_t requestId = this->context->getNextOperationId();

	struct ibv_sge sgElement;
	struct ibv_send_wr workRequest;
	struct ibv_send_wr *badWorkRequest;

	memset(&sgElement, 0, sizeof(ibv_sge));
	sgElement.addr = previousValue->getAddress();
	sgElement.length = previousValue->getSizeInBytes();
	sgElement.lkey = previousValue->getKey();

	memset(&workRequest, 0, sizeof(ibv_send_wr));
	workRequest.wr_id = requestId;
	workRequest.sg_list = &sgElement;
	workRequest.num_sge = 1;
	workRequest.opcode = IBV_WR_ATOMIC_CMP_AND_SWP;
	workRequest.send_flags = IBV_SEND_SIGNALED;
	workRequest.wr.atomic.remote_addr = destination->getAddress();
	workRequest.wr.atomic.rkey = destination->getKey();
	workRequest.wr.atomic.compare_add = compare;
	workRequest.wr.atomic.swap = swap;

	int returnValue = ibv_post_send(this->ibvQueuePair, &workRequest, &badWorkRequest);

	INFINITY_VERBS_ASSERT(returnValue == 0, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Posting read request failed.\n");

	INFINITY_VERBS_DEBUG_STATUS(1, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Reading from remote memory (request %lu)\n", requestId);

	return requestId;

}

opid_t QueuePair::fetchAndAdd(infinityverbs::memory::MemoryToken* destination,
		infinityverbs::memory::AtomicValue* previousValue, uint64_t add) {

	uint64_t requestId = this->context->getNextOperationId();

	struct ibv_sge sgElement;
	struct ibv_send_wr workRequest;
	struct ibv_send_wr *badWorkRequest;

	memset(&sgElement, 0, sizeof(ibv_sge));
	sgElement.addr = previousValue->getAddress();
	sgElement.length = previousValue->getSizeInBytes();
	sgElement.lkey = previousValue->getKey();

	memset(&workRequest, 0, sizeof(ibv_send_wr));
	workRequest.wr_id = requestId;
	workRequest.sg_list = &sgElement;
	workRequest.num_sge = 1;
	workRequest.opcode = IBV_WR_ATOMIC_FETCH_AND_ADD;
	workRequest.send_flags = IBV_SEND_SIGNALED;
	workRequest.wr.atomic.remote_addr = destination->getAddress();
	workRequest.wr.atomic.rkey = destination->getKey();
	workRequest.wr.atomic.compare_add = add;

	int returnValue = ibv_post_send(this->ibvQueuePair, &workRequest, &badWorkRequest);

	INFINITY_VERBS_ASSERT(returnValue == 0, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Posting read request failed.\n");

	INFINITY_VERBS_DEBUG_STATUS(1, "[INFINITYVERBS][QUEUES][QUEUEPAIR] Reading from remote memory (request %lu)\n", requestId);

	return requestId;

}

} /* namespace queues */
} /* namespace infinityverbs */


