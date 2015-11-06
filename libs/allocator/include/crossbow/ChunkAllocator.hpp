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
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <limits>

namespace crossbow {

/*!
 * \brief Memory allocator used by the parser stage to allocate temporary objects
 */
class ChunkMemoryPool {
public:
    static constexpr std::size_t DEFAULT_SIZE = 1 * 1024 * 1024; // 1MB

    ChunkMemoryPool(size_t chunkSize = DEFAULT_SIZE);

    // Disable copy constructor and assignment
    ChunkMemoryPool(const ChunkMemoryPool&) = delete;
    void operator =(const ChunkMemoryPool&) = delete;

    ~ChunkMemoryPool();

    void* allocate(std::size_t size);

    size_t chunkSize() const { return mChunkSize; }

private:
    void appendNewChunk();

private:
    std::size_t mChunkSize;

    char* mCurrent;
    char* mEnd;

    std::vector<char*> mChunks;
};

/*!
 * \brief Base class for classes used in the parser stage
 *
 * All member variables within a ChunkObject class have to be allocated with the same ChunkMemoryPool!
 */
class ChunkObject {
public:
    virtual ~ChunkObject();

    void* operator new(size_t size, ChunkMemoryPool* pool);

    void operator delete(void* p);
};

/*!
 * \brief Allocator backed by the ChunkMemoryPool for use with STL classes
 */
template <class T>
class ChunkAllocator {
public:
    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;

    template <class U>
        struct rebind {
            typedef ChunkAllocator<U> other;
        };

    ChunkAllocator(ChunkMemoryPool* pool);

    template <class U>
        ChunkAllocator(const ChunkAllocator<U>& other);

    T* allocate(std::size_t n);

    size_t max_size() const {
        return std::numeric_limits<size_t>::max();
    }

    void deallocate(T* p, std::size_t n);

    template <class U, class... Args>
        void construct(U* p, Args&&... args);

    template <class U>
        void destroy(U* p);

private:
    template <class T1, class U1>
        friend bool operator ==(const ChunkAllocator<T1>&, const ChunkAllocator<U1>&);

    template <class U1>
        friend class ChunkAllocator;

    ChunkMemoryPool* m_pool;
};

template <class T>
ChunkAllocator<T>::ChunkAllocator(ChunkMemoryPool* pool)
    : m_pool{pool} {
    }

template <class T>
template <class U>
ChunkAllocator<T>::ChunkAllocator(const ChunkAllocator<U>& other) {
    m_pool = other.m_pool;
}

template <class T>
T* ChunkAllocator<T>::allocate(std::size_t n) {
    auto p = m_pool->allocate(sizeof(T) * n);
    return static_cast<T*>(p);
}

template <class T>
void ChunkAllocator<T>::deallocate(T*, std::size_t) {
    // Do nothing
}

template <class T>
template <class U, class... Args>
void ChunkAllocator<T>::construct(U* p, Args&&... args) {
    ::new((void*) p) U(std::forward<Args>(args)...);
}

template <class T>
template <class U>
void ChunkAllocator<T>::destroy(U* p) {
    p->~U();
}

template <class T, class U>
inline bool operator ==(const ChunkAllocator<T>& lhs, const ChunkAllocator<U>& rhs) {
    return lhs.m_pool == rhs.m_pool;
}

template <class T, class U>
inline bool operator !=(const ChunkAllocator<T>& lhs, const ChunkAllocator<U>& rhs) {
    return !operator ==(lhs, rhs);
}

} // namespace crossbow

