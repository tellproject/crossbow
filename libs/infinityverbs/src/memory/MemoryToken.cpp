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
 * MemoryToken.cpp
 *
 *  Created on: Feb 17, 2015
 *      Author: claudeb
 */

#include "MemoryToken.hpp"

#include "../queues/QueuePair.hpp"

namespace infinityverbs {
namespace memory {

MemoryToken::MemoryToken(uint32_t userToken,
		infinityverbs::queues::QueuePair* queuePair, MemoryRegionType memoryRegionType,
		uint64_t address, uint32_t key, uint64_t sizeInBytes) {

	this->userToken = userToken;
	this->queuePair = queuePair;
	this->memoryRegionType = memoryRegionType;
	this->address = address;
	this->key = key;
	this->sizeInBytes = sizeInBytes;

}

uint32_t MemoryToken::getUserToken() {
	return this->userToken;
}

MemoryRegionType MemoryToken::getMemoryRegionType() {
	return this->memoryRegionType;
}

infinityverbs::queues::QueuePair* MemoryToken::getAssociatedQueuePair() {
	return this->queuePair;
}

uint64_t MemoryToken::getAddress() {
	return this->address;
}

uint64_t MemoryToken::getAddressWithOffset(uint64_t offsetInBytes) {
	return this->address+offsetInBytes;
}

uint32_t MemoryToken::getKey() {
	return this->key;
}

uint64_t MemoryToken::getSizeInBytes() {
	return this->sizeInBytes;
}

} /* namespace memory */
} /* namespace infinityverbs */


