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

#include <crossbow/alignment.hpp>

#include <cstdlib>

namespace crossbow {

ChunkMemoryPool::ChunkMemoryPool(size_t chunkSize)
    : mChunkSize(chunkSize)
    , mCurrent{new char[mChunkSize]}
    , mEnd{mCurrent + mChunkSize}
    , mChunks({mCurrent})
{
}

ChunkMemoryPool::~ChunkMemoryPool() {
    for (auto c : mChunks) {
        delete[] c;
    }
}

void* ChunkMemoryPool::allocate(std::size_t size) {
    if ((mCurrent + size) > mEnd) {
        if (size > mChunkSize) { 
            auto res = new char[size];
            mChunks.push_back(res);
            return res;
        }
        appendNewChunk();
    }
    auto p = mCurrent;
    mCurrent = crossbow::align(mCurrent + size, 8u);
    return p;
}

void ChunkMemoryPool::appendNewChunk() {
    mCurrent = new char[mChunkSize];
    mEnd = mCurrent + mChunkSize;
    mChunks.push_back(mCurrent);
}

ChunkObject::~ChunkObject() = default;

} // namespace crossbow
