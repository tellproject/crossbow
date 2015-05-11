/*
 * Lock.cpp
 *
 *  Created on: Mar 4, 2015
 *      Author: claudeb
 */

#include "Lock.hpp"

namespace infinityverbs {
namespace tools {

Lock::Lock() : lockFlag(0) {

}

void Lock::lock() {
	while(tas(&lockFlag)) {
		#if defined(__i386__) || defined(__x86_64__)
			__asm__ __volatile__ ("pause\n");
		#endif
	}
}

void Lock::unlock() {
	lockFlag = 0;
}

inline int Lock::tas(volatile char* lockFlag) {

	char res = 1;

	#if defined(__i386__) || defined(__x86_64__)
		__asm__ __volatile__ (
	          "lock xchgb %0, %1\n"
	          : "+q"(res), "+m"(*lockFlag)
	          :
	          : "memory", "cc");
	#else
		#error TAS not defined for this architecture.
	#endif

	return res;

}

} /* namespace tools */
} /* namespace infinityverbs */
