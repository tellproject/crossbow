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
