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
