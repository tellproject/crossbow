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
#include <iostream>
#include <atomic>
#include <future>
#include <thread>
#include <vector>
#include <array>
#include <mutex>
#include <stdlib.h>
#include <unistd.h>
#include <limits>

namespace crossbow {

template <typename T, size_t QueueSize>
class SingleConsumerQueue {
public:
    struct Item {
        Item() {
            is_valid = false;
        }
        T value;
        /*std::atomic_*/
        bool is_valid;
        //        size_t padding[8 -1 - (sizeof(T)+7)/8];
    };

    SingleConsumerQueue(): _consumed(std::numeric_limits<std::size_t>::max()), _insert_place(0) {
    }

    template<class ...Args>
    bool write(Args && ... recordArgs) {
        //1016 seems to give the best performance for the first-gen i7
        //        static_assert(sizeof(Item) == 16,"SingleConsumerQueue::Item does not have the right size");

        auto pos = _insert_place++;
        while (isFull(pos)) {
            usleep(1);
        }
        writeItem(pos, std::forward<Args>(recordArgs)...);
        return true;
    }

    template<class ...Args>
    bool tryWrite(Args && ... recordArgs) {
        auto pos = _insert_place.load();
        do {
            auto consume = _consumed.load();
            if (consume < pos && (pos - consume) >= QueueSize) {
                return false;
            }
        } while (!_insert_place.compare_exchange_strong(pos, pos + 1));
        writeItem(pos, std::forward<Args>(recordArgs)...);
        return true;
    }

    bool read(T &out) {
        size_t consume = _consumed.load();
        if (consume + 1 >= _insert_place.load() || !_data[(consume + 1) % QueueSize].is_valid)
            return false;
        ++consume;
        Item &item = _data[consume % QueueSize];
        item.is_valid = false;
        out = std::move(item.value);
        (item.value).~T();
        _consumed.store(consume);
        return true;
    }


    template<typename Iter>
    std::size_t readMultiple(Iter out, Iter end) {
        size_t consumed = _consumed.load();
        std::size_t count = 0;
        for (size_t i = 0; true; ++i) {
            if (!(_data[(consumed + 1 + i) % _data.size()].is_valid) || out == end) {
                count = i;
                break;
            }
            Item &item = _data[(consumed + i + 1) % _data.size()];
            item.is_valid = false;
            *out = std::move(item.value);
            (item.value).~T();
            ++out;
        }
        consumed += count;
        _consumed.store(consumed);
        return count;
    }

private:
    bool isFull(size_t pos) {
        auto size = pos - _consumed.load();
        return size >= QueueSize;
    }

    template<class ...Args>
    void writeItem(size_t pos, Args&&... recordArgs) {
        auto real_pos = pos % QueueSize;
        new(&_data[real_pos].value) T(std::forward<Args>(recordArgs)...);
        _data[real_pos].is_valid = true;
    }

    //    uint32_t startpadding[11];
    std::array<Item, QueueSize> _data;
    std::atomic_size_t _consumed;
    char padding[128];//64 performs badly
    std::atomic_size_t _insert_place;
};

} // namespace crossbow
