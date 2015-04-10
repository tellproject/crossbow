/*
 * QueuePairFactory.cpp
 *
 *  Created on: Dec 3, 2014
 *      Author: claudeb
 */

#include "QueuePairFactory.hpp"

#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include  <sys/socket.h>

#include "../core/Configuration.hpp"
#include "../core/MemoryTokenStore.hpp"
#include "../tools/AddressResolver.hpp"

#define MAX(a,b) ((a) > (b) ? (a) : (b))

namespace infinityverbs {
namespace queues {

typedef struct {

		uint16_t localDeviceId;
		uint32_t queuePairNumber;
		uint32_t sequenceNumber;
		int32_t userToken;
		bool userTokenSet;
		bool memoryTokenStoreActive;
		uint64_t memoryTokenStoreAddress;
		uint32_t memoryTokenStoreKey;
		uint32_t memoryTokenStoreSize;

} serializedQueuePair;

QueuePairFactory::QueuePairFactory(infinityverbs::core::Context *context) : context(context) {

	serverSocket = -1;

}

QueuePairFactory::~QueuePairFactory() {

	if(serverSocket >= 0) {
		close(serverSocket);
	}

}

void QueuePairFactory::bindToPort(uint16_t port) {

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	INFINITY_VERBS_ASSERT(serverSocket >= 0, "[INFINITYVERBS][QUEUES][FACTORY] Cannot open server socket.\n");

	sockaddr_in serverAddress;
	memset(&(serverAddress), 0, sizeof(sockaddr_in));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(port);

	int32_t enabled = 1;
	int32_t returnValue = setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
	INFINITY_VERBS_ASSERT(returnValue == 0, "[INFINITYVERBS][QUEUES][FACTORY] Cannot set socket option to reuse address.\n");

	returnValue = bind(serverSocket, (sockaddr *) &serverAddress, sizeof(sockaddr_in));
	INFINITY_VERBS_ASSERT(returnValue == 0, "[INFINITYVERBS][QUEUES][FACTORY] Cannot bind to local address and port.\n");

	returnValue = listen(serverSocket, 128);
	INFINITY_VERBS_ASSERT(returnValue == 0, "[INFINITYVERBS][QUEUES][FACTORY] Cannot listen on server socket.\n");

	char *ipAddressOfDevice = infinityverbs::tools::AddressResolver::getIpAddressOfInterface(INFINITY_VERBS_DEFAULT_DEVICE_NAME);
	INFINITY_VERBS_DEBUG_STATUS(0, "[INFINITYVERBS][QUEUES][FACTORY] Accepting connections on IP address %s and port %d.\n", ipAddressOfDevice, port);
	free(ipAddressOfDevice);

}

QueuePair* QueuePairFactory::acceptIncomingConnection() {

	return acceptIncomingConnection(0, false);

}

QueuePair* QueuePairFactory::acceptIncomingConnection(int32_t userToken) {

	return acceptIncomingConnection(userToken, true);

}

QueuePair * QueuePairFactory::acceptIncomingConnection(int32_t userToken, bool userTokenSet) {

	serializedQueuePair *receiveBuffer = (serializedQueuePair*) calloc(1, sizeof(serializedQueuePair));
	serializedQueuePair *sendBuffer = (serializedQueuePair*) calloc(1, sizeof(serializedQueuePair));

	int connectionSocket = accept(this->serverSocket, (sockaddr *) NULL, NULL);
	INFINITY_VERBS_ASSERT(connectionSocket >= 0, "[INFINITYVERBS][QUEUES][FACTORY] Cannot open connection socket.\n");

	int32_t returnValue = recv(connectionSocket, receiveBuffer, sizeof(serializedQueuePair), 0);
	INFINITY_VERBS_ASSERT(returnValue == sizeof(serializedQueuePair), "[INFINITYVERBS][QUEUES][FACTORY] Incorrect number of bytes received. Expected %lu. Received %d.\n", sizeof(serializedQueuePair), returnValue);

	QueuePair *queuePair = new QueuePair(this->context);
	if(userTokenSet) {
		queuePair->setLocalUserToken(userToken);
	}

	sendBuffer->localDeviceId = queuePair->getLocalDeviceId();
	sendBuffer->queuePairNumber = queuePair->getQueuePairNumber();
	sendBuffer->sequenceNumber = queuePair->getSequenceNumber();
	sendBuffer->userToken = queuePair->isLocalUserTokenSet();
	sendBuffer->userTokenSet = queuePair->getLocalUserToken();
	sendBuffer->memoryTokenStoreActive = (this->context->getMemoryTokenStore() != NULL);
	if(sendBuffer->memoryTokenStoreActive) {
		sendBuffer->memoryTokenStoreAddress = this->context->getMemoryTokenStore()->getStoreAddress();
		sendBuffer->memoryTokenStoreKey = this->context->getMemoryTokenStore()->getStoreKey();
		sendBuffer->memoryTokenStoreSize = this->context->getMemoryTokenStore()->getStoreSize();
	}

	returnValue = send(connectionSocket, sendBuffer, sizeof(serializedQueuePair), 0);
	INFINITY_VERBS_ASSERT(returnValue == sizeof(serializedQueuePair), "[INFINITYVERBS][QUEUES][FACTORY] Incorrect number of bytes transmitted. Expected %lu. Received %d.\n", sizeof(serializedQueuePair), returnValue);

	queuePair->activate(receiveBuffer->localDeviceId, receiveBuffer->queuePairNumber, receiveBuffer->sequenceNumber);
	if(receiveBuffer->userTokenSet) {
		queuePair->setRemoteUserToken(receiveBuffer->userToken);
	}

	if(receiveBuffer->memoryTokenStoreActive) {
		queuePair->activateRemoteMemoryTokenStore(receiveBuffer->memoryTokenStoreAddress, receiveBuffer->memoryTokenStoreKey, receiveBuffer->memoryTokenStoreSize);
	}

	close(connectionSocket);
	free(receiveBuffer);
	free(sendBuffer);

	return queuePair;

}

QueuePair * QueuePairFactory::connectToRemoteHost(const char* hostAddress, uint16_t port) {

	return connectToRemoteHost(hostAddress, port, 0, false);

}

QueuePair * QueuePairFactory::connectToRemoteHost(const char* hostAddress, uint16_t port, int32_t userToken) {

	return connectToRemoteHost(hostAddress, port, userToken, true);

}

QueuePair * QueuePairFactory::connectToRemoteHost(const char* hostAddress, uint16_t port, int32_t userToken, bool userTokenSet) {

	serializedQueuePair *receiveBuffer = (serializedQueuePair*) calloc(1, sizeof(serializedQueuePair));
	serializedQueuePair *sendBuffer = (serializedQueuePair*) calloc(1, sizeof(serializedQueuePair));

	sockaddr_in remoteAddress;
	memset(&(remoteAddress), 0, sizeof(sockaddr_in));
	remoteAddress.sin_family = AF_INET;
	inet_pton(AF_INET, hostAddress, &(remoteAddress.sin_addr));
	remoteAddress.sin_port = htons(port);

	int connectionSocket = socket(AF_INET, SOCK_STREAM, 0);
	INFINITY_VERBS_ASSERT(connectionSocket >= 0, "[INFINITYVERBS][QUEUES][FACTORY] Cannot open connection socket.\n");

	int returnValue = connect(connectionSocket, (sockaddr *) &(remoteAddress), sizeof(sockaddr_in));
	INFINITY_VERBS_ASSERT(returnValue == 0, "[INFINITYVERBS][QUEUES][FACTORY] Could not connect to server.\n");

	QueuePair *queuePair = new QueuePair(this->context);
	if(userTokenSet) {
		queuePair->setLocalUserToken(userToken);
	}

	sendBuffer->localDeviceId = queuePair->getLocalDeviceId();
	sendBuffer->queuePairNumber = queuePair->getQueuePairNumber();
	sendBuffer->sequenceNumber = queuePair->getSequenceNumber();
	sendBuffer->userToken = queuePair->isLocalUserTokenSet();
	sendBuffer->userTokenSet = queuePair->getLocalUserToken();
	sendBuffer->memoryTokenStoreActive = (this->context->getMemoryTokenStore() != NULL);
	if(sendBuffer->memoryTokenStoreActive) {
		sendBuffer->memoryTokenStoreAddress = this->context->getMemoryTokenStore()->getStoreAddress();
		sendBuffer->memoryTokenStoreKey = this->context->getMemoryTokenStore()->getStoreKey();
		sendBuffer->memoryTokenStoreSize = this->context->getMemoryTokenStore()->getStoreSize();
	}

	returnValue = send(connectionSocket, sendBuffer, sizeof(serializedQueuePair), 0);
	INFINITY_VERBS_ASSERT(returnValue == sizeof(serializedQueuePair), "[INFINITYVERBS][QUEUES][FACTORY] Incorrect number of bytes transmitted. Expected %lu. Received %d.\n", sizeof(serializedQueuePair), returnValue);

	returnValue = recv(connectionSocket, receiveBuffer, sizeof(serializedQueuePair), 0);
	INFINITY_VERBS_ASSERT(returnValue == sizeof(serializedQueuePair), "[INFINITYVERBS][QUEUES][FACTORY] Incorrect number of bytes received. Expected %lu. Received %d.\n", sizeof(serializedQueuePair), returnValue);

	queuePair->activate(receiveBuffer->localDeviceId, receiveBuffer->queuePairNumber, receiveBuffer->sequenceNumber);
	if(receiveBuffer->userTokenSet) {
		queuePair->setRemoteUserToken(receiveBuffer->userToken);
	}

	if(receiveBuffer->memoryTokenStoreActive) {
		queuePair->activateRemoteMemoryTokenStore(receiveBuffer->memoryTokenStoreAddress, receiveBuffer->memoryTokenStoreKey, receiveBuffer->memoryTokenStoreSize);
	}

	close(connectionSocket);
	free(receiveBuffer);
	free(sendBuffer);

	return queuePair;

}

QueuePair* QueuePairFactory::createLoopback() {

	return createLoopback(0, false);

}

QueuePair* QueuePairFactory::createLoopback(int32_t userToken) {

	return createLoopback(userToken, true);

}

QueuePair* QueuePairFactory::createLoopback(int32_t userToken, bool userTokenSet) {

	QueuePair *queuePair = new QueuePair(this->context);
	queuePair->activate(queuePair->getLocalDeviceId(), queuePair->getQueuePairNumber(), queuePair->getSequenceNumber());
	if(userTokenSet) {
		queuePair->setLocalUserToken(userToken);
		queuePair->setRemoteUserToken(userToken);
	}
	if(this->context->getMemoryTokenStore() != NULL) {
		queuePair->activateRemoteMemoryTokenStore(this->context->getMemoryTokenStore()->getStoreAddress(), this->context->getMemoryTokenStore()->getStoreKey(), this->context->getMemoryTokenStore()->getStoreSize());
	}

	return queuePair;

}


QueuePair** QueuePairFactory::connectAllToAll(const char** hostAddresses, uint16_t* ports,
		uint32_t numberOfMachines, uint32_t localMachineOffset) {

	QueuePair** queuePairs = new QueuePair* [numberOfMachines];

	// Wait for connections from lower ids
	for(uint32_t i=0; i<localMachineOffset; ++i) {
		queuePairs[i] = acceptIncomingConnection(localMachineOffset);
	}

	// Connect to higher ids
	for(uint32_t i=numberOfMachines-1; i>localMachineOffset; --i) {
		queuePairs[i] = connectToRemoteHost(hostAddresses[i], ports[i], localMachineOffset);
	}

	// Local queue pair is a loopback
	queuePairs[localMachineOffset] = this->context->getLoopBackQueuePair();

	for(uint32_t i=0; i<numberOfMachines; ++i) {
		INFINITY_VERBS_ASSERT(queuePairs[i]->isRemoteUserTokenSet(), "[INFINITYVERBS][QUEUES][FACTORY] Queue pair %d has no user-defined token set.\n", i);
		INFINITY_VERBS_ASSERT(queuePairs[i]->getRemoteUserToken() == i, "[INFINITYVERBS][QUEUES][FACTORY] Queue pair %d has incorrect user-defined token. Token is %d. Expected %d.\n", i, queuePairs[i]->getRemoteUserToken(), i);
	}

	return queuePairs;

}

} /* namespace queues */
} /* namespace infinityverbs */


