/*
 * RdmaBuffer.cpp
 *
 *  Created on: Feb 17, 2015
 *      Author: claudeb
 */

#include <string.h>

#include "RdmaBuffer.hpp"
#include "../core/Configuration.hpp"

namespace infinityverbs {
namespace memory {

RdmaBuffer::RdmaBuffer(infinityverbs::core::Context* context,
		uint64_t sizeInBytes) {

	this->context = context;
	this->sizeInBytes = sizeInBytes;

	int res = posix_memalign(&(this->data), INFINITY_VERBS_BUFFER_ALIGNMENT, sizeInBytes);
	INFINITY_VERBS_ASSERT(res == 0, "[INFINITYVERBS][MEMORY][BUFFER] Cannot allocate and align buffer.\n");

	memset(this->data, 0, sizeInBytes);

	this->ibvMemoryRegion = ibv_reg_mr(this->context->getInfiniBandProtectionDomain(), this->data, this->sizeInBytes, IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ);

	deregOnDelete = true;
	deallocOnDelete = true;
}

RdmaBuffer::RdmaBuffer(infinityverbs::core::Context* context, void* buffer,
		uint64_t sizeInBytes) {

	this->context = context;
	this->sizeInBytes = sizeInBytes;
	this->data = buffer;

	this->ibvMemoryRegion = ibv_reg_mr(this->context->getInfiniBandProtectionDomain(), this->data, this->sizeInBytes, IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ);

	deregOnDelete = true;
	deallocOnDelete = false;
}

RdmaBuffer::RdmaBuffer(infinityverbs::core::Context* context, ibv_mr* mr,
		void* buffer, uint64_t sizeInBytes) {

	this->context = context;
	this->sizeInBytes = sizeInBytes;
	this->data = buffer;
	this->ibvMemoryRegion = mr;

	deregOnDelete = false;
	deallocOnDelete = false;

}


RdmaBuffer::~RdmaBuffer() {

	if(deregOnDelete) {
		ibv_dereg_mr(this->ibvMemoryRegion);
	}

	if(deallocOnDelete) {
		free(this->data);
	}

}

MemoryToken* RdmaBuffer::getMemoryToken(uint32_t userToken) {

	return new MemoryToken(userToken, this->context->getLoopBackQueuePair(), RDMA_BUFFER, (uint64_t) (this->data), this->ibvMemoryRegion->rkey, this->sizeInBytes);

}

void* RdmaBuffer::getData() {
	return this->data;
}

uint64_t RdmaBuffer::getAddress() {
	return (uint64_t) (this->data);
}

} /* namespace memory */
} /* namespace infinityverbs */


