/*
 * (C) Copyright 2015 ETH Zurich Systems Group (http://www.systems.ethz.ch/) and others.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Contributors:
 *     Markus Pilman <mpilman@inf.ethz.ch>
 *     Simon Loesing <sloesing@inf.ethz.ch>
 *     Thomas Etter <etterth@gmail.com>
 *     Kevin Bocksrocker <kevin.bocksrocker@gmail.com>
 *     Lucas Braun <braunl@inf.ethz.ch>
 */
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


