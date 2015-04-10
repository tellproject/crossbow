/*
 * Context.cpp
 *
 *  Created on: Nov 24, 2014
 *      Author: claudeb
 */

#include "Context.hpp"

#include <string.h>
#include <stdlib.h>
#include <utility>
#include <limits>

#include "Configuration.hpp"
#include "MemoryTokenStore.hpp"
#include "../queues/QueuePair.hpp"
#include "../queues/QueuePairFactory.hpp"

#define MAX(a,b) ((a) > (b) ? (a) : (b))

namespace infinityverbs {
namespace core {

Context::Context() {
	initialize(INFINITY_VERBS_DEFAULT_DEVICE, INFINITY_VERBS_DEFAULT_DEVICE_PORT,
	INFINITY_VERBS_DEFAULT_SEND_QUEUE_LENGTH, INFINITY_VERBS_DEFAULT_RECEIVE_QUEUE_LENGTH);
}

Context::Context(uint32_t sendQueueLength, uint32_t receiveQueueLength) {
	initialize(INFINITY_VERBS_DEFAULT_DEVICE, INFINITY_VERBS_DEFAULT_DEVICE_PORT, sendQueueLength,
			receiveQueueLength);
}

Context::Context(uint16_t ibDevice, uint16_t ibDevicePort, uint32_t sendQueueLength,
		uint32_t receiveQueueLength) {
	initialize(ibDevice, ibDevicePort, sendQueueLength, receiveQueueLength);
}

Context::~Context() {

	delete this->memoryTokenStore;
	delete this->loopBackQueuePair;

	int returnValue = ibv_destroy_srq(this->ibvSharedReceiveQueue);
	INFINITY_VERBS_ASSERT(returnValue == 0,
			"[INFINITYVERBS][CORE][CONTEXT] Could not delete shared receive queue\n");

	returnValue = ibv_dealloc_pd(this->ibvProtectionDomain);
	INFINITY_VERBS_ASSERT(returnValue == 0,
			"[INFINITYVERBS][CORE][CONTEXT] Could not delete protection domain\n");

	returnValue = ibv_close_device(this->ibvContext);
	INFINITY_VERBS_ASSERT(returnValue == 0,
			"[INFINITYVERBS][CORE][CONTEXT] Could not close device\n");

}

void Context::initialize(uint16_t ibDevice, uint16_t ibDevicePort, uint32_t sendQueueLength,
		uint32_t receiveQueueLength) {

	// Set maximum buffer size and queue lengths
	this->sendQueueLength = sendQueueLength;
	this->receiveQueueLength = receiveQueueLength;

	// Get IB device list
	int32_t numberOfInstalledDevices = 0;
	ibv_device **ibvDeviceList = ibv_get_device_list(&numberOfInstalledDevices);
	INFINITY_VERBS_ASSERT(numberOfInstalledDevices > 0,
			"[INFINITYVERBS][CORE][CONTEXT] No InfiniBand devices found.\n");
	INFINITY_VERBS_ASSERT(ibDevice < numberOfInstalledDevices,
			"[INFINITYVERBS][CORE][CONTEXT] Requested device %d not found. There are %d devices available.\n",
			ibDevice, numberOfInstalledDevices);
	INFINITY_VERBS_ASSERT(ibvDeviceList != NULL,
			"[INFINITYVERBS][CORE][CONTEXT] Device list was NULL.\n");

	// Get IB device
	this->ibvDevice = ibvDeviceList[ibDevice];
	INFINITY_VERBS_ASSERT(this->ibvDevice != NULL,
			"[INFINITYVERBS][CORE][CONTEXT] Requested device %d was NULL.\n", ibDevice);

	// Open IB device and allocate protection domain
	this->ibvContext = ibv_open_device(ibvDevice);
	INFINITY_VERBS_ASSERT(this->ibvContext != NULL,
			"[INFINITYVERBS][CORE][CONTEXT] Could not open device %d.\n", ibDevice);
	this->ibvProtectionDomain = ibv_alloc_pd(this->ibvContext);
	INFINITY_VERBS_ASSERT(this->ibvProtectionDomain != NULL,
			"[INFINITYVERBS][CORE][CONTEXT] Could not allocate protection domain.\n");

	// Get the LID of desired port
	ibv_port_attr portAttributes;
	ibv_query_port(this->ibvContext, ibDevicePort, &portAttributes);
	this->ibvDeviceId = portAttributes.lid;
	this->ibvDevicePort = ibDevicePort;

	// Allocate completion queues
	this->ibvSendCompletionQueue = ibv_create_cq(this->ibvContext, MAX(sendQueueLength, 1), NULL,
			NULL, 0);
	this->ibvReceiveCompletionQueue = ibv_create_cq(this->ibvContext, MAX(receiveQueueLength, 1),
			NULL, NULL, 0);

	// Allocate shared receive queue
	ibv_srq_init_attr sia;
	memset(&sia, 0, sizeof(ibv_srq_init_attr));
	sia.srq_context = this->ibvContext;
	sia.attr.max_wr = MAX(receiveQueueLength, 1);
	sia.attr.max_sge = 1;
	this->ibvSharedReceiveQueue = ibv_create_srq(this->ibvProtectionDomain, &sia);
	INFINITY_VERBS_ASSERT(this->ibvSharedReceiveQueue != NULL,
			"[INFINITYVERBS][CORE][CONTEXT] Could not allocate shared receive queue.\n");

	// Create memory store
	this->memoryTokenStore = new MemoryTokenStore(this, INFINITY_VERBS_DEFAULT_TOKEN_STORE_SIZE);

	//Create local loop-back queue pair
	infinityverbs::queues::QueuePairFactory *factory = new infinityverbs::queues::QueuePairFactory(this);
	this->loopBackQueuePair = factory->createLoopback();
	delete factory;

	operationIdCounter.store(1);
	completedBaseCounter = 0;
}

ibv_context* Context::getInfiniBandContext() {
	return ibvContext;
}

uint16_t Context::getLocalDeviceId() {
	return ibvDeviceId;
}

uint16_t Context::getLocalDevicePort() {
	return ibvDevicePort;
}

ibv_pd* Context::getInfiniBandProtectionDomain() {
	return ibvProtectionDomain;
}

ibv_cq* Context::getSendCompletionQueue() {
	return ibvSendCompletionQueue;
}

ibv_cq* Context::getReceiveCompletionQueue() {
	return ibvReceiveCompletionQueue;
}

ibv_srq* Context::getSharedReceiveQueue() {
	return ibvSharedReceiveQueue;
}

uint32_t Context::getSendQueueLength() {
	return sendQueueLength;
}

uint32_t Context::getReceiveQueueLength() {
	return receiveQueueLength;
}

infinityverbs::core::MemoryTokenStore* Context::getMemoryTokenStore() {
	return this->memoryTokenStore;
}

infinityverbs::queues::QueuePair* Context::getLoopBackQueuePair() {
	return this->loopBackQueuePair;
}

opid_t Context::getNextOperationId() {
	return operationIdCounter.fetch_add(1);
}

void Context::waitUntilOperationCompleted(opid_t operationId) {
	while(!checkIfOperationCompleted(operationId));
	INFINITY_VERBS_DEBUG_STATUS(1, "[INFINITYVERBS][CORE][CONTEXT] Request %lu completed\n", operationId);
}

bool Context::checkIfOperationCompleted(opid_t operationId) {

	registerNextComplitionElement();
	completedSetLock.lock();
	bool result = (operationId <= completedBaseCounter || requestCompletionSet.count(operationId) > 0);
	completedSetLock.unlock();
	return result;

}

void Context::registerNextComplitionElement() {

	opid_t requestId = pollNextComplitionElement();
	if(requestId > 0) {
		completedSetLock.lock();
		if(requestId == completedBaseCounter + 1) {
			++completedBaseCounter;
			while(requestCompletionSet.count(completedBaseCounter+1) > 0) {
				requestCompletionSet.erase(completedBaseCounter+1);
				++completedBaseCounter;
			}
		} else {
			requestCompletionSet.insert(requestId);
		}
		completedSetLock.unlock();
	}

}

opid_t Context::pollNextComplitionElement() {

	ibv_wc wc;
	if(ibv_poll_cq(this->ibvSendCompletionQueue, 1, &wc) > 0) {
		INFINITY_VERBS_ASSERT(wc.status == IBV_WC_SUCCESS, "[INFINITYVERBS][CORE][CONTEXT] Request %lu failed with failure code: %s.\n", wc.wr_id, ibv_wc_status_str(wc.status));
		return wc.wr_id;
	} else {
		return 0;
	}

}

void Context::postReceiveBuffer(infinityverbs::memory::RdmaBuffer* buffer) {

	INFINITY_VERBS_ASSERT(buffer->getSizeInBytes() <= std::numeric_limits<uint32_t>::max(), "[INFINITYVERBS][CORE][CONTEXT] Cannot post receive buffer which is larger than 32 bytes.\n");

	ibv_sge isge;
	memset(&isge, 0, sizeof(ibv_sge));
	isge.addr = buffer->getAddress();
	isge.length = (uint32_t) buffer->getSizeInBytes();
	isge.lkey = buffer->getMemoryRegion()->lkey;

	ibv_recv_wr wr;
	memset(&wr, 0, sizeof(ibv_recv_wr));
	wr.wr_id = reinterpret_cast<uint64_t>(buffer);
	wr.next = NULL;
	wr.sg_list = &isge;
	wr.num_sge = 1;

	ibv_recv_wr *badwr;
	uint32_t ret = ibv_post_srq_recv(this->ibvSharedReceiveQueue, &wr, &badwr);
	INFINITY_VERBS_ASSERT(ret == 0, "[INFINITYVERBS][CORE][CONTEXT] Cannot post buffer to receive queue.\n");

}

bool Context::receive(infinityverbs::memory::RdmaBuffer** buffer, uint32_t* bytesWritten) {

	ibv_wc wc;
	if(ibv_poll_cq(this->ibvReceiveCompletionQueue, 1, &wc) > 0) {

		*(buffer) = reinterpret_cast<infinityverbs::memory::RdmaBuffer*>(wc.wr_id);
		*(bytesWritten) = wc.byte_len;

		return true;
	}

	return false;

}

} /* namespace core */
} /* namespace infiniverbs */


