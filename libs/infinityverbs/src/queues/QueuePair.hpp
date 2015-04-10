/*
 * QueuePair.hpp
 *
 *  Created on: Dec 10, 2014
 *      Author: claudeb
 */

#ifndef QUEUES_QUEUEPAIR_HPP_
#define QUEUES_QUEUEPAIR_HPP_

#include <stdlib.h>
#include <stdint.h>
#include <infiniband/verbs.h>

#include "../core/Context.hpp"
#include "../core/MemoryTokenStore.hpp"
#include "../memory/MemoryToken.hpp"
#include "../memory/MemoryRegion.hpp"
#include "../memory/RdmaBuffer.hpp"
#include "../memory/AtomicValue.hpp"

namespace infinityverbs {
namespace queues {

class QueuePair {
	public:

		QueuePair(infinityverbs::core::Context *context);
		~QueuePair();

		void activate(uint16_t remoteDeviceId, uint32_t remoteQueuePairNumber, uint32_t remoteSequenceNumber);
		void activateRemoteMemoryTokenStore(uint64_t address, uint32_t key, uint32_t storeSize);

		/**
		 * Queue pair information
		 */
		uint16_t getLocalDeviceId();
		uint32_t getQueuePairNumber();
		uint32_t getSequenceNumber();

		/**
		 * Local tokens
		 */
		bool isLocalUserTokenSet();
		void setLocalUserToken(uint32_t userToken);
		uint32_t getLocalUserToken();

		/**
		 * Remote tokens
		 */
		bool isRemoteUserTokenSet();
		void setRemoteUserToken(uint32_t userToken);
		uint32_t getRemoteUserToken();

		/**
		 * Memory store information
		 */
		bool isRemoteMemoryTokenStoreActive();
		opid_t synchronizeRemoteMemoryTokenStore();
		infinityverbs::memory::MemoryToken* getRemoteMemoryToken(uint32_t userToken);
		bool isRemoteMemoryTokenValid(infinityverbs::memory::MemoryToken *memoryToken);

		/**
		 * Buffer operations
		 */
		opid_t send(infinityverbs::memory::RdmaBuffer *buffer);
		opid_t write(infinityverbs::memory::RdmaBuffer *buffer, infinityverbs::memory::MemoryToken *destination);
		opid_t read(infinityverbs::memory::RdmaBuffer *buffer, infinityverbs::memory::MemoryToken *source);

		/**
		 * Atomic value operations
		 */
		opid_t compareAndSwap(infinityverbs::memory::MemoryToken *destination, infinityverbs::memory::AtomicValue *previousValue, uint64_t compare, uint64_t swap);
		opid_t fetchAndAdd(infinityverbs::memory::MemoryToken *destination, infinityverbs::memory::AtomicValue *previousValue, uint64_t add);

	protected:

		infinityverbs::core::Context *context;

		ibv_qp* ibvQueuePair;
		uint32_t sequenceNumber;

		bool localUserTokenSet;
		uint32_t localUserToken;

		bool remoteUserTokenSet;
		uint32_t remoteUserToken;

		bool isRemoteMemoryStoreActive;
		uint64_t remoteMemoryStoreAddress;
		uint32_t remoteMemoryStoreKey;
		uint32_t remoteMemoryStoreSize;
		infinityverbs::core::SerializedMemoryToken *remoteMemoryStoreCache;
		ibv_mr *remoteMemoryStoreCacheMR;

};

} /* namespace queues */
} /* namespace infinityverbs */

#endif /* QUEUES_QUEUEPAIR_HPP_ */
