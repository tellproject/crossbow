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
 * MemoryTokenStore.hpp
 *
 *  Created on: Feb 17, 2015
 *      Author: claudeb
 */

#ifndef CORE_MEMORYTOKENSTORE_HPP_
#define CORE_MEMORYTOKENSTORE_HPP_

#include <infiniband/verbs.h>

#include "Context.hpp"
#include "SerializedMemoryToken.hpp"
#include "../memory/MemoryToken.hpp"

namespace infinityverbs {
namespace core {

class MemoryTokenStore {
	public:

		MemoryTokenStore(infinityverbs::core::Context *context, uint32_t numberOfTokens);
		virtual ~MemoryTokenStore();

		void publishTokenData(infinityverbs::memory::MemoryToken *memoryToken);
		void unpublishTokenData(uint32_t userToken);

		uint32_t getStoreKey();
		uint64_t getStoreAddress();
		uint32_t getStoreSize();

	protected:

		infinityverbs::core::Context *context;
		uint32_t numberOfTokens;

		SerializedMemoryToken *dataBuffer;

		/**
		 * IB verbs memory region
		 */
		ibv_mr *ibvMemoryRegion;

};

} /* namespace core */
} /* namespace infinityverbs */

#endif /* CORE_MEMORYTOKENSTORE_HPP_ */
