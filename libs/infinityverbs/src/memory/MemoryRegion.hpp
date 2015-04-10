/*
 * MemoryRegion.hpp
 *
 *  Created on: Dec 15, 2014
 *      Author: claudeb
 */

#ifndef MEMORY_MEMORYREGION_HPP_
#define MEMORY_MEMORYREGION_HPP_

#include "MemoryToken.hpp"
#include "MemoryRegionType.hpp"
#include "../core/Context.hpp"

namespace infinityverbs {
namespace memory {

class MemoryRegion {
	public:

		virtual ~MemoryRegion() {};
		virtual MemoryToken * getMemoryToken(uint32_t userToken) {return NULL;};

		uint64_t getAddress() {return 0;};
		uint64_t getSizeInBytes() {return sizeInBytes;};
		uint32_t getKey() {return ibvMemoryRegion->lkey;};
		ibv_mr * getMemoryRegion() {return this->ibvMemoryRegion;};

	protected:

		MemoryRegionType memoryRegionType;

		ibv_mr *ibvMemoryRegion;
		uint64_t sizeInBytes;

		infinityverbs::core::Context* context;

};

} /* namespace memory */
} /* namespace infinityverbs */

#endif /* MEMORY_MEMORYREGION_HPP_ */
