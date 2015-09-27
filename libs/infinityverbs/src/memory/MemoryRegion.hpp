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
