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


