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
