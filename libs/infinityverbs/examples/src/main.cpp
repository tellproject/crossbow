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

#include "InfinityVerbs.hpp"

#include <unistd.h>
#include <cassert>
#include <stdlib.h>

int main(int argc, char **argv) {

	bool isServer = false;

	while (argc > 1) {
		if (argv[1][0] == '-') {
			switch (argv[1][1]) {

				case 's': {
					isServer = true;
					break;
				}

			}
		}
		++argv;
		--argc;
	}

	infinityverbs::core::Context *context = new infinityverbs::core::Context();
	infinityverbs::queues::QueuePairFactory *qpFactory = new  infinityverbs::queues::QueuePairFactory(context);
	infinityverbs::queues::QueuePair *qp;

	if(isServer) {

		infinityverbs::memory::RdmaBuffer *bufferToRead = new infinityverbs::memory::RdmaBuffer(context, 128 * sizeof(char));
		infinityverbs::memory::RdmaBuffer *bufferToWrite = new infinityverbs::memory::RdmaBuffer(context, 128 * sizeof(char));
		infinityverbs::memory::RdmaBuffer *bufferToReceive = new infinityverbs::memory::RdmaBuffer(context, 128 * sizeof(char));

		infinityverbs::memory::MemoryToken *readToken = bufferToRead->getMemoryToken(1000);
		infinityverbs::memory::MemoryToken *writeToken = bufferToWrite->getMemoryToken(2000);

		context->getMemoryTokenStore()->publishTokenData(readToken);
		context->getMemoryTokenStore()->publishTokenData(writeToken);

		context->postReceiveBuffer(bufferToReceive);

		qpFactory->bindToPort(8010);
		qp = qpFactory->acceptIncomingConnection();

		infinityverbs::memory::RdmaBuffer *message;
		uint32_t messageSize;

		while(!context->receive(&message, &messageSize));

		delete bufferToRead;
		delete bufferToWrite;
		delete bufferToReceive;

	} else {

		qp = qpFactory->connectToRemoteHost("192.168.1.61", 8010);

		infinityverbs::memory::RdmaBuffer *buffer1Sided = new infinityverbs::memory::RdmaBuffer(context, 128 * sizeof(char));
		infinityverbs::memory::RdmaBuffer *buffer2Sided = new infinityverbs::memory::RdmaBuffer(context, 128 * sizeof(char));

		opid_t syncOp = qp->synchronizeRemoteMemoryTokenStore();
		context->waitUntilOperationCompleted(syncOp);

		infinityverbs::memory::MemoryToken *readBufferToken = qp->getRemoteMemoryToken(1000);
		infinityverbs::memory::MemoryToken *writeBufferToken = qp->getRemoteMemoryToken(2000);

		assert(readBufferToken != NULL);
		assert(writeBufferToken != NULL);

		opid_t readOp = qp->read(buffer1Sided, readBufferToken);
		context->waitUntilOperationCompleted(readOp);

		opid_t writeOp = qp->write(buffer1Sided, writeBufferToken);
		context->waitUntilOperationCompleted(writeOp);

		opid_t sendOp = qp->send(buffer2Sided);
		context->waitUntilOperationCompleted(sendOp);

		delete buffer1Sided;
		delete buffer2Sided;

	}

	delete qp;
	delete qpFactory;
	delete context;

	return 0;

}
