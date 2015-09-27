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

#include <crossbow/alignment.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace crossbow {

/**
 * @brief The buffer_reader class used to read values from a buffer
 */
class buffer_reader {
public:
    buffer_reader(const char* pos, size_t length)
            : mPos(pos),
              mEnd(pos + length) {
    }

    ~buffer_reader() {
        assert(mPos <= mEnd);
    }

    bool exhausted() const {
        return (mPos >= mEnd);
    }

    bool canRead(size_t length) const {
        return (mPos + length <= mEnd);
    }

    template <typename T>
    T read() {
        auto value = (*reinterpret_cast<const T*>(mPos));
        mPos += sizeof(T);
        return value;
    }

    const char* read(size_t length) {
        auto value = mPos;
        mPos += length;
        return value;
    }

    const char* data() const {
        return mPos;
    }

    void advance(size_t length) {
        mPos += length;
    }

    void align(size_t alignment) {
        mPos = crossbow::align(mPos, alignment);
    }

    buffer_reader extract(size_t length) {
        auto value = buffer_reader(mPos, length);
        mPos += length;
        return value;
    }

private:
    const char* mPos;
    const char* mEnd;
};

/**
 * @brief The buffer_writer class used to write values to a buffer
 */
class buffer_writer {
public:
    buffer_writer(char* pos, size_t length)
            : mPos(pos),
              mEnd(pos + length) {
    }

    buffer_writer(void* pos, size_t length)
            : buffer_writer(reinterpret_cast<char*>(pos), length) {
    }

    ~buffer_writer() {
        assert(mPos <= mEnd);
    }

    bool exhausted() const {
        return (mPos >= mEnd);
    }

    bool canWrite(size_t length) const {
        return (mPos + length <= mEnd);
    }

    template <typename T>
    void write(T value) {
        (*reinterpret_cast<T*>(mPos)) = value;
        mPos += sizeof(T);
    }

    void write(const void* value, size_t length) {
        memcpy(mPos, value, length);
        mPos += length;
    }

    void set(int value, size_t length) {
        memset(mPos, value, length);
        mPos += length;
    }

    char* data() {
        return mPos;
    }

    void advance(size_t length) {
        mPos += length;
    }

    void align(size_t alignment) {
        mPos = crossbow::align(mPos, alignment);
    }

    buffer_writer extract(size_t length) {
        auto value = buffer_writer(mPos, length);
        mPos += length;
        return value;
    }

private:
    char* mPos;
    char* mEnd;
};

} // namespace crossbow
