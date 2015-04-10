/*
 * RdmaBuffer.hpp
 *
 *  Created on: Feb 17, 2015
 *      Author: claudeb
 */

#ifndef MEMORY_RDMABUFFER_HPP_
#define MEMORY_RDMABUFFER_HPP_

#include <stdint.h>

#include "../core/Configuration.hpp"
#include "MemoryRegion.hpp"

namespace infinityverbs {
namespace memory {

class RdmaBuffer : public MemoryRegion {

	public:

		RdmaBuffer(infinityverbs::core::Context *context, uint64_t sizeInBytes);
		RdmaBuffer(infinityverbs::core::Context *context, void *buffer, uint64_t sizeInBytes);
		RdmaBuffer(infinityverbs::core::Context *context, ibv_mr *mr, void *buffer, uint64_t sizeInBytes);

		~RdmaBuffer();

		uint64_t getAddress();
		void* getData();
		MemoryToken * getMemoryToken(uint32_t userToken);

	protected:

		void *data;

		bool deregOnDelete;
		bool deallocOnDelete;

};

} /* namespace memory */
} /* namespace infinityverbs */

#endif /* MEMORY_RDMABUFFER_HPP_ */
