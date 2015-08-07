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
