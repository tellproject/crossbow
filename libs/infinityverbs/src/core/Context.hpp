/*
 * Context.h
 *
 *  Created on: Nov 24, 2014
 *      Author: claudeb
 */

#ifndef INFINITYVERBS_SRC_CORE_CONTEXT_HPP_
#define INFINITYVERBS_SRC_CORE_CONTEXT_HPP_

#include <infiniband/verbs.h>
#include <stdint.h>
#include <atomic>
#include <unordered_set>
#include "../tools/Lock.hpp"

typedef uint64_t opid_t;


namespace infinityverbs {
namespace queues {
	class QueuePair;
}
}

namespace infinityverbs {
namespace core {
	class MemoryTokenStore;
	class RequestCompletion;
}
}

namespace infinityverbs {
namespace memory {
	class RdmaBuffer;
}
}


namespace infinityverbs {
namespace core {

class Context {

	public:

		/**
		 * Constructors
		 */
		Context();
		Context(uint32_t sendQueueLength, uint32_t receiveQueueLength);
		Context(uint16_t ibDevice, uint16_t ibDevicePort, uint32_t sendQueueLength, uint32_t receiveQueueLength);

		/**
		 * Destructor
		 */
		~Context();

		/**
		 * Returns ibVerbs context
		 */
		ibv_context * getInfiniBandContext();

		/**
		 * Returns local device id
		 */
		uint16_t getLocalDeviceId();

		/**
		 * Returns port of local device
		 */
		uint16_t getLocalDevicePort();

		/**
		 * Returns ibVerbs protection domain
		 */
		ibv_pd * getInfiniBandProtectionDomain();

		/**
		 * Returns ibVerbs completion queue for sending
		 */
		ibv_cq * getSendCompletionQueue();

		/**
		 * Returns ibVerbs completion queue for receiving
		 */
		ibv_cq * getReceiveCompletionQueue();

		/**
		 * Returns ibVerbs shared receive queue
		 */
		ibv_srq * getSharedReceiveQueue();

		/**
		 * Returns length of send queue
		 */
		uint32_t getSendQueueLength();

		/**
		 * Returns length of receive queue
		 */
		uint32_t getReceiveQueueLength();

		/**
		 * Get the local memory token store
		 */
		infinityverbs::core::MemoryTokenStore * getMemoryTokenStore();

		/**
		 * Returns local loop-back queue pair
		 */
		infinityverbs::queues::QueuePair* getLoopBackQueuePair();

		/**
		 * Return next operation id
		 */
		opid_t getNextOperationId();

		/**
		 * Block (Busy-Wait) until operation completed
		 */
		void waitUntilOperationCompleted(opid_t operationId);

		/**
		 * Check if operation completed
		 */
		bool checkIfOperationCompleted(opid_t operationId);

		/**
		 * Post a new buffer for receiving messages
		 */
		void postReceiveBuffer(infinityverbs::memory::RdmaBuffer *buffer);

		/**
		 * Returns true if there has been a message. In that case, buffer and bytesWritten are set.
		 */
		bool receive(infinityverbs::memory::RdmaBuffer **buffer, uint32_t *bytesWritten);

	protected:

		/**
		 * Initializes the context
		 */
		void initialize(uint16_t ibDevice, uint16_t ibDevicePort, uint32_t sendQueueLength, uint32_t receiveQueueLength);

		/**
		 * IB verbs context and protection domain
		 */
		ibv_context *ibvContext;
		ibv_pd *ibvProtectionDomain;

		/**
		 * IB verbs send and receive completion queues
		 */
		ibv_cq *ibvSendCompletionQueue;
		ibv_cq *ibvReceiveCompletionQueue;
		ibv_srq *ibvSharedReceiveQueue;

		/**
		 * Loop-back queue pair
		 */
		infinityverbs::queues::QueuePair *loopBackQueuePair;

		/**
		 * Local device
		 */
		ibv_device *ibvDevice;
		uint16_t ibvDeviceId;
		uint16_t ibvDevicePort;

		/**
		 * Queue lengths
		 */
		uint32_t sendQueueLength;
		uint32_t receiveQueueLength;

		/**
		 * Key-Value store for memory region meta data
		 */
		infinityverbs::core::MemoryTokenStore *memoryTokenStore;

		/**
		 * Operation id counter
		 */
		std::atomic<opid_t> operationIdCounter;


		opid_t completedBaseCounter;
		std::unordered_set<opid_t> requestCompletionSet;

		/**
		 * Register that operation has completed
		 */
		void registerNextComplitionElement();

		/**
		 * Check next element on the completion queue
		 */
		opid_t pollNextComplitionElement();

		/**
		 * Lock for registering completed elements
		 */
		infinityverbs::tools::Lock completedSetLock;

};

} /* namespace core */
} /* namespace infiniverbs */

#endif /* INFINITYVERBS_SRC_CORE_CONTEXT_HPP_ */
