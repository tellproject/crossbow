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
 * AddressResolver.hpp
 *
 *  Created on: Dec 3, 2014
 *      Author: claudeb
 */

#ifndef TOOLS_ADDRESSRESOLVER_HPP_
#define TOOLS_ADDRESSRESOLVER_HPP_

#include <stdint.h>

namespace infinityverbs {
namespace tools {

class AddressResolver {
	public:

		static char * getIpAddressOfInterface(const char *interfaceName);
		static uint32_t getIpAddressAsUint32(const char *ipAddress);

};

} /* namespace tools */
} /* namespace infinityverbs */

#endif /* TOOLS_ADDRESSRESOLVER_HPP_ */
