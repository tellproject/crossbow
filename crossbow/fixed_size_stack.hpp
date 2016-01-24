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

#include <crossbow/non_copyable.hpp>

#include <atomic>
#include <cassert>
#include <cstddef>
#include <vector>

namespace crossbow {

template<class T>
class fixed_size_stack : crossbow::non_copyable, crossbow::non_movable {
private:
    struct alignas(8) Head {
        unsigned readHead = 0u;
        unsigned writeHead = 0u;

        Head() noexcept = default;

        Head(unsigned readHead, unsigned writeHead)
            : readHead(readHead),
              writeHead(writeHead)
        {}
    };
    static_assert(sizeof(T) <= 8, "Only CAS with less than 8 bytes supported");
    std::vector<T> mVec;
    std::atomic<Head> mHead;

public:
    fixed_size_stack(size_t size, T nullValue)
        : mVec(size, nullValue)
    {
        mHead.store(Head(0u, 0u));
        assert(mHead.is_lock_free());
        assert(mVec.size() == size);
        assert(mHead.load().readHead == 0);
        assert(mHead.load().writeHead == 0);
    }

    /**
    * \returns true if pop succeeded - result will be set
    *          to the popped element on the stack
    */
    bool pop(T& result) {
        while (true) {
            auto head = mHead.load();
            if (head.writeHead != head.readHead) continue;
            if (head.readHead == 0) {
                return false;
            }
            result = mVec[head.readHead - 1];
            if (mHead.compare_exchange_strong(head, Head(head.readHead - 1, head.writeHead - 1)))
                return true;
        }
    }

    bool push(T element) {
        auto head = mHead.load();

        // Advance the write head by one
        do {
            if (head.writeHead == mVec.size()) {
                return false;
            }
        } while (!mHead.compare_exchange_strong(head, Head(head.readHead, head.writeHead + 1)));
        auto wHead = head.writeHead;

        // Store the element
        mVec[wHead] = element;

        // Wait until the read head points to our write position
        while (head.readHead != wHead) {
            head = mHead.load();
        }

        // Advance the read head by one
        while (!mHead.compare_exchange_strong(head, Head(wHead + 1, head.writeHead)));

        return true;
    }

    /**
     * @brief Number of elements in the stack
     */
    size_t size() const {
        return mHead.load().readHead;
    }

    /**
     * @brief Maximum capacity of the stack
     */
    size_t capacity() const {
        return mVec.size();
    }
};

} // namespace crossbow
