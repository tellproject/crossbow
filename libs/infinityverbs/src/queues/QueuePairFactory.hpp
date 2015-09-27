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
 * QueuePairFactory.hpp
 *
 *  Created on: Dec 3, 2014
 *      Author: claudeb
 */

#ifndef QUEUES_QUEUEPAIRFACTORY_HPP_
#define QUEUES_QUEUEPAIRFACTORY_HPP_

#include <stdlib.h>
#include <stdint.h>
#include <infiniband/verbs.h>

#include "../core/Context.hpp"
#include "QueuePair.hpp"

namespace infinityverbs {
namespace queues {

class QueuePairFactory {

	public:

		QueuePairFactory(infinityverbs::core::Context *context);
		~QueuePairFactory();

		/**
		 * Bind to port for listening to incoming connections
		 */
		void bindToPort(uint16_t port);

		/**
		 * Accept incoming connection request (passive side)
		 */
		QueuePair * acceptIncomingConnection();

		/**
		 * Accept incoming connection request (passive side) with expected user-defined token
		 */
		QueuePair * acceptIncomingConnection(int32_t userToken);

		/**
		 * Connect to remote machine (active side)
		 */
		QueuePair * connectToRemoteHost(const char *hostAddress, uint16_t port);

		/**
		 * Connect to remote machine (active side) with user-defined token.
		 */
		QueuePair * connectToRemoteHost(const char* hostAddress, uint16_t port, int32_t userToken);

		/**
		 * Create loopback queue pair
		 */
		QueuePair * createLoopback();

		/**
		 * Create loopback queue pair with user-defined token
		 */
		QueuePair * createLoopback(int32_t userToken);

		/**
		 * Convenience function for all-to-all communication
		 */
		QueuePair ** connectAllToAll(const char **hostAddresses, uint16_t *ports, uint32_t numberOfMachines, uint32_t localMachineOffset);



	protected:

		infinityverbs::core::Context * const context;

		int32_t serverSocket;

		QueuePair * acceptIncomingConnection(int32_t userToken, bool userTokenSet);
		QueuePair * connectToRemoteHost(const char* hostAddress, uint16_t port, int32_t userToken, bool userTokenSet);
		QueuePair * createLoopback(int32_t userToken, bool userTokenSet);

};

} /* namespace queues */
} /* namespace infinityverbs */

#endif /* QUEUES_QUEUEPAIRFACTORY_HPP_ */
