#pragma once

namespace crossbow {

/**
 * @brief Dummy interface to delete all copy constructor and assignment functions
 */
class non_copyable {
protected:
    non_copyable() = default;
    ~non_copyable() = default;

    // Delete Copy Constructor / Assignment
    non_copyable(const non_copyable&) = delete;
    non_copyable& operator=(const non_copyable&) = delete;
};

/**
 * @brief Dummy interface to delete all move constructor and assignment functions
 */
class non_movable {
protected:
    non_movable() = default;
    ~non_movable() = default;

    // Delete Move Constructor / Assignment
    non_movable(non_movable&&) = delete;
    non_movable& operator=(non_movable&&) = delete;
};

} // namespace crossbow
