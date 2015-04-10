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


