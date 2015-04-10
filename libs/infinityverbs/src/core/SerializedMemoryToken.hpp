/*
 * SerializedMemoryToken.hpp
 *
 *  Created on: Feb 18, 2015
 *      Author: claudeb
 */

#ifndef CORE_SERIALIZEDMEMORYTOKEN_HPP_
#define CORE_SERIALIZEDMEMORYTOKEN_HPP_

#include <stdint.h>

#include "../memory/MemoryRegionType.hpp"

namespace infinityverbs {
namespace core {

class SerializedMemoryToken {

	public:

		bool enabled;
		uint32_t userToken;
		MemoryRegionType memoryRegionType;
		uint64_t address;
		uint32_t key;
		uint64_t sizeInBytes;

};

} /* namespace core */
} /* namespace infinityverbs */

#endif /* CORE_SERIALIZEDMEMORYTOKEN_HPP_ */
