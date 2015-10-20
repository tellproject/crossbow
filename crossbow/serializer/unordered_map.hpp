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
#include <unordered_map>

namespace crossbow {

template<class Archiver, class Key, class Value, class Hash, class Predicate, class Allocator>
struct serialize_policy<Archiver, std::unordered_map<Key, Value, Hash, Predicate, Allocator>>
{
    using type = std::unordered_map<Key, Value, Hash, Predicate, Allocator>;
    uint8_t* operator() (Archiver& ar, const type& map, uint8_t* pos) const {
        std::size_t s = map.size();
        ar & s;
        for (const auto& e : map) {
            ar & e.first;
            ar & e.second;
        }
        return ar.pos;
    }
};

template<class Archiver, class Key, class Value, class Hash, class Predicate, class Allocator>
struct deserialize_policy<Archiver, std::unordered_map<Key, Value, Hash, Predicate, Allocator>>
{
    using type = std::unordered_map<Key, Value, Hash, Predicate, Allocator>;
    const uint8_t* operator() (Archiver& ar, type& out, const uint8_t* ptr) const {
        size_t sz;
        ar & sz;
        for (size_t i = 0; i < sz; ++i) {
            Key f;
            Value s;
            ar & f;
            ar & s;
            out.emplace(std::move(f), std::move(s));
        }
        return ar.pos;
    }
};

template<class Archiver, class Key, class Value, class Hash, class Predicate, class Allocator>
struct size_policy<Archiver, std::unordered_map<Key, Value, Hash, Predicate, Allocator>>
{
    using type = std::unordered_map<Key, Value, Hash, Predicate, Allocator>;
    std::size_t operator() (Archiver& ar, const type& map) const {
        std::size_t s;
        ar & s;
        for (auto& e : map) {
            ar & e.first;
            ar & e.second;
        }
        return 0;
    }
};

} // namespace crossbow
