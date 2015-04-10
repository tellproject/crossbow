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
