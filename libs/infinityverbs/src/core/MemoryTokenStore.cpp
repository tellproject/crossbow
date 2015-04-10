/*
 * MemoryTokenStore.cpp
 *
 *  Created on: Feb 17, 2015
 *      Author: claudeb
 */

#include "MemoryTokenStore.hpp"

#include <stdlib.h>

#include "Configuration.hpp"

namespace infinityverbs {
namespace core {

MemoryTokenStore::MemoryTokenStore(Context* context,
		uint32_t numberOfTokens) {

	this->context = context;
	this->numberOfTokens = numberOfTokens;

	// Allocate memory
	int res = posix_memalign((void **) (&(this->dataBuffer)), INFINITY_VERBS_PAGE_ALIGNMENT, numberOfTokens * sizeof(SerializedMemoryToken));
	INFINITY_VERBS_ASSERT(res == 0, "[INFINITYVERBS][CORE][STORE] Cannot allocate required memory\n");

	for(uint64_t i=0; i<numberOfTokens; ++i) {
		dataBuffer[i].enabled = false;
	}

	// Register memory with the IB card
	this->ibvMemoryRegion = ibv_reg_mr(this->context->getInfiniBandProtectionDomain(), this->dataBuffer, numberOfTokens * sizeof(SerializedMemoryToken), IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE);

	INFINITY_VERBS_DEBUG_STATUS(1, "[INFINITYVERBS][CORE][STORE] Store is allocated at address %lu and key %d\n", (uint64_t) this->ibvMemoryRegion->addr, this->ibvMemoryRegion->rkey);

}

MemoryTokenStore::~MemoryTokenStore() {

	ibv_dereg_mr(this->ibvMemoryRegion);
	free(this->dataBuffer);

}

void MemoryTokenStore::publishTokenData(infinityverbs::memory::MemoryToken *memoryToken) {

	for(uint64_t i=0; i<numberOfTokens; ++i) {
		if(dataBuffer[i].enabled == false) {
			dataBuffer[i].userToken = memoryToken->getUserToken();
			dataBuffer[i].memoryRegionType = memoryToken->getMemoryRegionType();
			dataBuffer[i].address = memoryToken->getAddress();
			dataBuffer[i].key = memoryToken->getKey();
			dataBuffer[i].sizeInBytes = memoryToken->getSizeInBytes();
			dataBuffer[i].enabled = true;
			return;
		}
	}
	INFINITY_VERBS_ASSERT(false, "[INFINITYVERBS][CORE][STORE] Store is out of free space.\n");

}

void MemoryTokenStore::unpublishTokenData(uint32_t userToken) {

	for(uint64_t i=0; i<numberOfTokens; ++i) {
		if(dataBuffer[i].userToken == userToken) {
			dataBuffer[i].enabled = false;
			return;
		}
	}

}

uint32_t MemoryTokenStore::getStoreKey() {
	return this->ibvMemoryRegion->rkey;
}

uint64_t MemoryTokenStore::getStoreAddress() {
	return (uint64_t) (this->dataBuffer);
}

uint32_t MemoryTokenStore::getStoreSize() {
	return this->numberOfTokens;
}


} /* namespace core */
} /* namespace infinityverbs */

