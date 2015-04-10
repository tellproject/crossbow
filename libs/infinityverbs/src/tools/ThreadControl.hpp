/*
 * ThreadControl.hpp
 *
 *  Created on: Feb 17, 2015
 *      Author: claudeb
 */

#ifndef TOOLS_THREADCONTROL_HPP_
#define TOOLS_THREADCONTROL_HPP_

#include <stdint.h>

namespace infinityverbs {
namespace tools {

class ThreadControl {
	public:

		static void pinThread(uint32_t coreId);

};

} /* namespace tools */
} /* namespace infinityverbs */

#endif /* TOOLS_THREADCONTROL_HPP_ */
