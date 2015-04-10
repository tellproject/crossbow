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
