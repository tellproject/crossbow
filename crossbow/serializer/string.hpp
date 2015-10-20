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
#include <string>

namespace crossbow {

template<typename Archiver, class Char, class Traits, class Allocator>
struct serialize_policy<Archiver, std::basic_string<Char, Traits, Allocator>>
{
    using type = std::basic_string<Char, Traits, Allocator>;
    uint8_t* operator() (Archiver&, const type& obj, uint8_t* pos) const
    {
        uint32_t len = uint32_t(obj.size());
        memcpy(pos, &len, sizeof(std::uint32_t));
        pos += sizeof(std::uint32_t);
        memcpy(pos, obj.data(), len);
        return pos + len;
    }
};

template<typename Archiver, class Char, class Traits, class Allocator>
struct deserialize_policy<Archiver, std::basic_string<Char, Traits, Allocator>>
{
    using type = std::basic_string<Char, Traits, Allocator>;
    const uint8_t* operator() (Archiver&, type& out, const uint8_t* ptr) const
    {
        const std::uint32_t s = *reinterpret_cast<const std::uint32_t*>(ptr);
        out = std::string(reinterpret_cast<const char*>(ptr + sizeof(std::uint32_t)), s);
        return ptr + sizeof(s) + s;
    }
};

template<typename Archiver, class Char, class Traits, class Allocator>
struct size_policy<Archiver, std::basic_string<Char, Traits, Allocator>>
{
    using type = std::basic_string<Char, Traits, Allocator>;
    std::size_t operator() (Archiver& ar, const type& obj) const
    {
        return sizeof(uint32_t) + obj.size();
    }
};

} // namespace crossbow

