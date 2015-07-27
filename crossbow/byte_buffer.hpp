#pragma once

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
        mPos = reinterpret_cast<const char*>((reinterpret_cast<const uintptr_t>(mPos) - 0x1u + alignment) & -alignment);
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
        mPos = reinterpret_cast<char*>((reinterpret_cast<uintptr_t>(mPos) - 0x1u + alignment) & -alignment);
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
