#pragma once

#include <cstddef>
#include <cstdint>

namespace crossbow {
namespace infinio {

/**
 * @brief The BufferReader class used to read values from a buffer
 */
class BufferReader {
public:
    BufferReader(const char* pos, size_t length)
            : mPos(pos),
              mEnd(pos + length) {
    }

    bool exhausted() const {
        return (mPos >= mEnd);
    }

    bool canRead(size_t length) {
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
        mPos += ((reinterpret_cast<const uintptr_t>(mPos) % alignment != 0)
                 ? (alignment - (reinterpret_cast<const uintptr_t>(mPos) % alignment))
                 :  0);
    }

    BufferReader extract(size_t length) {
        auto value = BufferReader(mPos, length);
        mPos += length;
        return value;
    }

private:
    const char* mPos;
    const char* mEnd;
};

/**
 * @brief The BufferWriter class used to write values to a buffer
 */
class BufferWriter {
public:
    BufferWriter(char* pos, size_t length)
            : mPos(pos),
              mEnd(pos + length) {
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

    char* data() {
        return mPos;
    }

    void advance(size_t length) {
        mPos += length;
    }

    void align(size_t alignment) {
        mPos += ((reinterpret_cast<uintptr_t>(mPos) % alignment != 0)
                 ? (alignment - (reinterpret_cast<uintptr_t>(mPos) % alignment))
                 :  0);
    }

    BufferWriter extract(size_t length) {
        auto value = BufferWriter(mPos, length);
        mPos += length;
        return value;
    }

private:
    char* mPos;
    char* mEnd;
};

} // namespace infinio
} // namespace crossbow
