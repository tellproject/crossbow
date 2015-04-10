/*
 * Lock.hpp
 *
 *  Created on: Mar 4, 2015
 *      Author: claudeb
 */

#ifndef TOOLS_LOCK_HPP_
#define TOOLS_LOCK_HPP_

namespace infinityverbs {
namespace tools {

class Lock {
	public:

		Lock();

		void lock();
		void unlock();

	protected:

		volatile char lockFlag;
		inline int tas(volatile char* lockFlag);

};

} /* namespace tools */
} /* namespace infinityverbs */

#endif /* TOOLS_LOCK_HPP_ */
