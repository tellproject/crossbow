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
