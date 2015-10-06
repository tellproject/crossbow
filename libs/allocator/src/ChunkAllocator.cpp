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
#include <crossbow/ChunkAllocator.hpp>
#include <cstdlib>

namespace crossbow {

ChunkMemoryPool::ChunkMemoryPool()
		: m_start{nullptr}, m_end{nullptr}, m_current{nullptr} {
}

ChunkMemoryPool::~ChunkMemoryPool() {
	delete m_start;
}

void ChunkMemoryPool::init() {
	if (!m_start) {
		m_start = static_cast<char*>(std::malloc(pool_size));
	}
	m_current = m_start;
	m_end = m_start + pool_size;
}

void* ChunkMemoryPool::allocate(std::size_t size) {
	if ((m_current + size) > m_end) {
		throw std::bad_alloc{};
	}
	auto p = m_current;
	m_current += size;
	return p;
}

ChunkObject::~ChunkObject() = default;

void* ChunkObject::operator new(size_t size, ChunkMemoryPool* pool) {
	return pool->allocate(size);
}

void ChunkObject::operator delete(void *a) {
	// Do nothing
}

} // namespace crossbow

