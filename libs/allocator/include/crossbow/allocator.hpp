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

#include <atomic>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

namespace crossbow {

class allocator {
public:
    static void init();

    static void destroy();

    static void* malloc(std::size_t size);

    static void* malloc(std::size_t size, std::size_t align);

    static void free(void* ptr, std::function<void()> destruct = []() { });
    static void free_in_order(void* ptr, std::function<void()> destruct = []() { });

    static void free_now(void* ptr);

    static void invoke(std::function<void()> fun) {
        allocator::free(allocator::malloc(0), std::move(fun));
    }

    template <typename T, typename... Args>
    static typename std::enable_if<alignof(T) <= alignof(void*), T*>::type construct(Args&&... args) {
        return new (allocator::malloc(sizeof(T))) T(std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    static typename std::enable_if<(alignof(T) > alignof(void*)) && !(alignof(T) & (alignof(T) - 1)), T*>::type
            construct(Args&&... args) {
        return new (allocator::malloc(sizeof(T), alignof(T))) T(std::forward<Args>(args)...);
    }

    template <typename T>
    static void destroy(T* ptr) {
        if (!ptr) {
            return;
        }

        allocator::free(ptr, [ptr] () {
            ptr->~T();
        });
    }

    template <typename T>
    static void destroy_in_order(T* ptr) {
        if (!ptr) {
            return;
        }

        allocator::free_in_order(ptr, [ptr] () {
            ptr->~T();
        });
    }

    template <typename T>
    static void destroy_now(T* ptr) {
        if (!ptr) {
            return;
        }

        ptr->~T();
        allocator::free_now(ptr);
    }

    allocator();

    ~allocator();

private:
    std::atomic<uint64_t>* cnt_;
};

} // namespace crossbow
