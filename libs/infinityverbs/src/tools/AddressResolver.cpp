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
 * AddressResolver.cpp
 *
 *  Created on: Dec 3, 2014
 *      Author: claudeb
 */

#include "AddressResolver.hpp"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <strings.h>

#include "../core/Configuration.hpp"

namespace infinityverbs {
namespace tools {

char* AddressResolver::getIpAddressOfInterface(const char* interfaceName) {

	struct ifaddrs *ifAddr;
	struct ifaddrs *ifa;
	char *ipAddress = (char*) calloc(16, sizeof(char));

	int returnValue = getifaddrs(&ifAddr);
	INFINITY_VERBS_ASSERT(returnValue != -1, "[INFINITYVERBS][TOOLS][ADDRESSRESOLVER] Cannot read interface list.\n");

	for(ifa = ifAddr; ifa != NULL; ifa = ifa->ifa_next) {
		if(ifa->ifa_addr == NULL) {
			continue;
		}
		if((ifa->ifa_addr->sa_family == AF_INET) && (strcasecmp(interfaceName, ifa->ifa_name) == 0)) {
			sprintf(ipAddress, "%s", inet_ntoa(((struct sockaddr_in *) ifa->ifa_addr)->sin_addr));
			break;
		}
	}
	INFINITY_VERBS_ASSERT(ifa != NULL, "[INFINITYVERBS][TOOLS][ADDRESSRESOLVER] Cannot find interface named %s.\n", interfaceName);

	freeifaddrs(ifAddr);

	return ipAddress;

}

uint32_t AddressResolver::getIpAddressAsUint32(const char* ipAddress) {
	uint32_t ipAddressNumbers[4];
	sscanf(ipAddress, "%d.%d.%d.%d", &ipAddressNumbers[3], &ipAddressNumbers[2], &ipAddressNumbers[1], &ipAddressNumbers[0]);
	uint32_t ipAddressNumber (ipAddressNumbers[0] | ipAddressNumbers[1] << 8 | ipAddressNumbers[2] << 16 | ipAddressNumbers[3] << 24);
	return ipAddressNumber;
}

} /* namespace tools */
} /* namespace infinityverbs */

