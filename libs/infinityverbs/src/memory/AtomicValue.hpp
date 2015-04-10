/*
 * AtomicValue.hpp
 *
 *  Created on: Feb 17, 2015
 *      Author: claudeb
 */

#ifndef MEMORY_ATOMICVALUE_HPP_
#define MEMORY_ATOMICVALUE_HPP_

#include "../core/Configuration.hpp"
#include "MemoryRegion.hpp"

namespace infinityverbs {
namespace memory {

class AtomicValue : public MemoryRegion {

	public:

		AtomicValue(infinityverbs::core::Context *context);
		~AtomicValue();

		uint64_t readValue();

		uint64_t getAddress();
		MemoryToken * getMemoryToken(uint32_t userToken);

	protected:

		uint64_t value;

};

} /* namespace memory */
} /* namespace infinityverbs */

#endif /* MEMORY_ATOMICVALUE_HPP_ */
