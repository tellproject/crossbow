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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace crossbow {

/**
 * @brief Align the integral type to be a multiple of alignment
 *
 * Alignment must be a power of two.
 */
template<typename T, typename A>
inline typename std::enable_if<std::is_integral<T>::value, T>::type align(T value, A alignment) {
    assert((alignment != 0) && !(alignment & (alignment - 1)));
    return (value - 1u + alignment) & -(static_cast<T>(alignment));
}

/**
 * @brief Align the pointer to be a multiple of alignment
 *
 * Alignment must be a power of two.
 */
template<typename T>
inline typename std::enable_if<std::is_pointer<T>::value, T>::type align(T value, uintptr_t alignment) {
    assert((alignment != 0) && !(alignment & (alignment - 1)));
    return reinterpret_cast<T>((reinterpret_cast<uintptr_t>(value) - 1u + alignment) & -alignment);
}

} // namespace crossbow
