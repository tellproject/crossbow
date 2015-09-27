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
 * AtomicValue.cpp
 *
 *  Created on: Feb 17, 2015
 *      Author: claudeb
 */

#include "AtomicValue.hpp"

namespace infinityverbs {
namespace memory {

AtomicValue::AtomicValue(infinityverbs::core::Context *context) {

	this->context = context;
	this->sizeInBytes = sizeof(uint64_t);

	value = 0;

	this->ibvMemoryRegion = ibv_reg_mr(this->context->getInfiniBandProtectionDomain(), &(this->value), this->sizeInBytes, IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE);

}

AtomicValue::~AtomicValue() {

	ibv_dereg_mr(this->ibvMemoryRegion);

}

uint64_t AtomicValue::readValue() {

	return value;

}

MemoryToken* AtomicValue::getMemoryToken(uint32_t userToken) {

	return new MemoryToken(userToken, this->context->getLoopBackQueuePair(), ATOMIC_VALUE, (uint64_t) &(this->value), this->ibvMemoryRegion->rkey, this->sizeInBytes);

}

uint64_t AtomicValue::getAddress() {
	return (uint64_t) (&value);
}

} /* namespace memory */
} /* namespace infinityverbs */


