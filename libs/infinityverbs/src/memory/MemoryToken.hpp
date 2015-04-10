/*
 * MemoryToken.hpp
 *
 *  Created on: Feb 17, 2015
 *      Author: claudeb
 */

#ifndef MEMORY_MEMORYTOKEN_HPP_
#define MEMORY_MEMORYTOKEN_HPP_

#include <stdint.h>

#include "MemoryRegionType.hpp"

namespace infinityverbs {
namespace queues {
	class QueuePair;
}
}


namespace infinityverbs {
namespace memory {

class MemoryToken {
	public:

		MemoryToken(uint32_t userToken, infinityverbs::queues::QueuePair *queuePair, MemoryRegionType memoryRegionType, uint64_t address, uint32_t key, uint64_t sizeInBytes);

		uint32_t getUserToken();
		MemoryRegionType getMemoryRegionType();

		infinityverbs::queues::QueuePair *getAssociatedQueuePair();

		uint64_t getAddress();
		uint64_t getAddressWithOffset(uint64_t offsetInBytes);

		uint32_t getKey();

		uint64_t getSizeInBytes();

	protected:

		uint32_t userToken;
		infinityverbs::queues::QueuePair *queuePair;

		MemoryRegionType memoryRegionType;

		uint64_t address;
		uint32_t key;
		uint64_t sizeInBytes;

};

} /* namespace memory */
} /* namespace infinityverbs */

#endif /* MEMORY_MEMORYTOKEN_HPP_ */
