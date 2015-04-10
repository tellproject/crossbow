/*
 * DirectMemoryAccessStore.hpp
 *
 *  Created on: Dec 16, 2014
 *      Author: claudeb
 */

#ifndef CORE_DIRECTMEMORYACCESSSTORE_HPP_
#define CORE_DIRECTMEMORYACCESSSTORE_HPP_

#include <infiniband/verbs.h>
#include <stdint.h>

namespace infinityverbs {
namespace core {

class DirectMemoryAccessStore {

	friend class Context;

	public:

		void registerMemoryRegionForDirectAccess(int32_t userToken, uint32_t rKey, uint64_t address, uint64_t length);

		void deregisterMemoryRegionForDirectAccess(int32_t userToken);

	protected:

		DirectMemoryAccessStore(uint32_t numberOfEntries);

		~DirectMemoryAccessStore();

		uint32_t getRKeyOfDirectAccessStore();

		uint64_t getAddressOfDirectAccessStore();

		typedef struct {
			int32_t userToken;
			uint32_t rKey;
			uint64_t address;
			uint64_t length;
			bool valid;
		} MemoryRegionDirectAccessParameters;

		uint32_t const numberOfEntries;

		MemoryRegionDirectAccessParameters *directAccessRegister;
		ibv_mr *ibvDirectAccessRegister;

};

} /* namespace core */
} /* namespace infinityverbs */

#endif /* CORE_DIRECTMEMORYACCESSSTORE_HPP_ */
