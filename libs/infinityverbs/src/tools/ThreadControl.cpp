/*
 * ThreadControl.cpp
 *
 *  Created on: Feb 17, 2015
 *      Author: claudeb
 */

#include "ThreadControl.hpp"

#include <pthread.h>

namespace infinityverbs {
namespace tools {

void ThreadControl::pinThread(uint32_t coreId) {

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(coreId, &cpuset);

	pthread_t current_thread = pthread_self();
	pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);

}


} /* namespace tools */
} /* namespace infinityverbs */

