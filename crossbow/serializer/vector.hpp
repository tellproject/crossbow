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
#include "Serializer.hpp"
#include <vector>

namespace crossbow {

template<typename Archiver, typename T, typename Allocator>
struct serialize_policy<Archiver, std::vector<T, Allocator>>
{
    uint8_t* operator() (Archiver& ar, const std::vector<T, Allocator>& v, uint8_t* pos) const {
        std::size_t s = v.size();
        ar & s;
        for (auto& e : v) {
            ar & e;
        }
        return ar.pos;
    }
};

template<typename Archiver, typename T, typename Allocator>
struct deserialize_policy<Archiver, std::vector<T, Allocator>>
{
    const uint8_t* operator() (Archiver& ar, std::vector<T, Allocator>& out, const uint8_t* ptr) const
    {
        const std::size_t s = *reinterpret_cast<const std::size_t*>(ptr);
        ar.pos = ptr + sizeof(s);
        out.reserve(s);
        for (std::size_t i = 0; i < s; ++i) {
            T obj;
            ar & obj;
            out.push_back(obj);
        }
        return ar.pos;
    }
};

template<typename Archiver, typename T, typename Allocator>
struct size_policy<Archiver, std::vector<T, Allocator>>
{
    std::size_t operator() (Archiver& ar, const std::vector<T, Allocator>& obj) const
    {
        std::size_t s;
        ar & s;
        for (auto& e : obj) {
            ar & e;
        }
        return 0;
    }
};

} // namespace crossbow
