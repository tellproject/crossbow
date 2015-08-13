#pragma once
#include <string>
#include <array>
#include <limits>
#include <iostream>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <type_traits>
#include <memory>

namespace crossbow {

template<class Char, class Traits = std::char_traits<Char>, class Allocator = std::allocator<Char> >
class basic_string {
public: // Types
    using traits_type = Traits;
    using value_type = typename Traits::char_type;
    using allocator_type = Allocator;
    using size_type = typename std::allocator_traits<allocator_type>::size_type;
    using difference_type = typename std::allocator_traits<allocator_type>::difference_type;
    using reference = value_type &;
    using const_reference = const value_type &;
    using pointer = typename std::allocator_traits<allocator_type>::pointer;
    using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;
    using iterator = pointer;
    using const_iterator = const_pointer;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
public: // Constants
    static constexpr size_type npos = std::numeric_limits<size_type>::max();
private: // members
    static constexpr size_type _ARR_SIZE = 32;
    // data model:
    // First byte = size if whole string is in buffer
    // ELSE
    //  - first byte = 0
    //  - next 7 bytes: unused
    //  - next 8 bytes = size
    //  - next 8 bytes = capacity
    //  - next sizeof(pointer) bytes are the pointer to the heap objects
    static constexpr size_t SIZE_OFFSET = 8;
    static constexpr size_t CAPACITY_OFFSET = 8 + sizeof(size_type);
    static constexpr size_t POINTER_OFFSET = 8 + 2 * sizeof(size_type);
    static_assert(POINTER_OFFSET + sizeof(value_type*) <= _ARR_SIZE* sizeof(value_type), "array to small");
    std::array<value_type, _ARR_SIZE> arr;
    allocator_type alloc;
private: // Helpers
    static constexpr unsigned char nullchar = std::numeric_limits<value_type>::max();
    static constexpr size_type max_in_buffer_size() {
        return ((_ARR_SIZE - 1) / sizeof(value_type)) - 1;
    }

    inline void init(size_t count) {
        if (count <= max_in_buffer_size()) {
            arr[0] = (unsigned char)(count);
            (*end()) = '\0';
        } else {
            arr[0] = nullchar;
            set_size(count);
            auto ptr = alloc.allocate(count + 1);
            set_capacity(count);
            set_ptr(ptr);
            *(end()) = '\0';
        }
    }

    inline void set_capacity(size_type c) {
        std::memcpy(arr.data() + CAPACITY_OFFSET, &c, sizeof(size_type));
    }

    inline size_type get_capacity() const {
        if (arr[0] != nullchar)
            return max_in_buffer_size();
        size_type res;
        std::memcpy(&res, arr.data() + CAPACITY_OFFSET, sizeof(res));
        return res;
    }

    inline void set_size(size_type count) {
        if (arr[0] != nullchar)
            arr[0] = (unsigned char)(count);
        else
            std::memcpy(arr.data() + SIZE_OFFSET, &count, sizeof(size_type));
    }

    inline size_type get_size() const {
        if (arr[0] != nullchar)
            return size_type(arr[0]);
        size_type res;
        std::memcpy(&res, arr.data() + SIZE_OFFSET, sizeof(res));
        return res;
    }

    inline void set_ptr(pointer ptr) {
        std::memcpy(arr.data() + POINTER_OFFSET, &ptr, sizeof(ptr));
#ifndef NDEBUG
        auto nptr = get_ptr();
        assert(nptr == ptr);
#endif
    }

    inline pointer get_ptr() {
        using val_ptr = value_type*;
        if (arr[0] != nullchar) {
            return pointer(reinterpret_cast<val_ptr>(arr.data() + 1));
        }
        pointer res;
        std::memcpy(&res, arr.data() + POINTER_OFFSET, sizeof(res));
        return pointer(res);
    }
    inline const_pointer get_ptr() const {
        return const_cast<basic_string<Char, Traits, Allocator>*>(this)->get_ptr();
    }
public: // Helpers
    bool __invariants() const {
        return size() <= capacity();
    }
public: // Constructors
    explicit basic_string(const allocator_type &alloc = allocator_type())
        : alloc(alloc) {
for (auto & c : arr) c = '\0';
        arr[0] = 0;
    }
    basic_string(size_type count, Char ch, const allocator_type &alloc = allocator_type())
        : alloc(alloc) {
        init(count);
        auto ptr = get_ptr();
        for (size_type i = 0; i < count; ++i) {
            ptr[i] = ch;
        }
        ptr[count] = '\0';
    }
    basic_string(const basic_string<Char, Traits, Allocator> &other,
                 size_type pos,
                 size_type count = npos,
                 const allocator_type &alloc = allocator_type())
        : alloc(alloc) {
        auto osize = other.size();
        if (pos > osize)
            throw std::out_of_range("Trying to construct a string with pos >= size");
        if (pos == osize) {
for (auto & c : arr) c = '\0';
        }
        auto cnt = osize - pos < count ? osize - pos : count;
        init(cnt);
        auto ptr = get_ptr();
        auto optr = other.get_ptr() + pos;
        std::copy(optr, optr + cnt, ptr);
        ptr[cnt] = '\0';
    }
    basic_string(const_pointer s, size_type count, const allocator_type &alloc = allocator_type())
        : alloc(alloc) {
        init(count);
        auto ptr = get_ptr();
        std::copy(s, s + count, ptr);
        ptr[count] = '\0';
    }
    basic_string(const_pointer s, const allocator_type &alloc = allocator_type())
        : alloc(alloc) {
        size_type count = traits_type::length(s);
        init(count);
        auto ptr = get_ptr();
        std::copy(s, s + count, ptr);
    }
    template< class InputIt >
    basic_string(InputIt first, InputIt last,
                 const Allocator &alloc = Allocator())
        : alloc(alloc) {
        auto count = std::distance(first, last);
        init(count);
        auto ptr = get_ptr();
        for (decltype(count) i = 0; i < count; ++i) {
            auto adv = first;
            std::advance(adv, i);
            *(ptr + i) = *adv;
        }
        *(ptr + count) = '\0';
    }
    basic_string(const basic_string<Char, Traits, Allocator> &other) : alloc(other.alloc) {
        if (other.arr[0] != nullchar) {
            arr = other.arr;
        } else {
            auto count = other.size();
            init(count);
            std::copy(other.begin(), other.end(), begin());
            get_ptr()[count] = '\0';
        }
    }
    basic_string(const basic_string &other, const allocator_type &alloc) : alloc(alloc) {
        if (other.arr[0] != nullchar) {
            arr = other.arr;
        } else {
            auto count = other.size();
            init(count);
            std::copy(other.begin(), other.end(), begin());
            *(end()) = '\0';
        }
    }
    basic_string(basic_string && other) noexcept(std::is_nothrow_move_assignable<allocator_type>::value) : arr(other.arr), alloc(std::move(other.alloc)) {
for (auto & a : other.arr) {
            a = '\0';
        }
    }
    basic_string(basic_string && other, const allocator_type &alloc) noexcept(std::is_nothrow_assignable<allocator_type &, allocator_type>::value) : arr(other.arr), alloc(alloc) {
for (auto & a : other.arr) {
            a = '\0';
        }
    }
    basic_string(std::initializer_list<value_type> linit,
                 const allocator_type &alloc = allocator_type())
        : alloc(alloc) {
        init(linit.size());
        std::copy(linit.begin(), linit.end(), begin());
    }

    // Compatibility to std::string
    basic_string(const std::basic_string<Char, traits_type, allocator_type> &other)
        : basic_string(const_pointer(other.c_str()), other.size(), other.get_allocator()) {
    }

    ~basic_string() {
        if (arr[0] == nullchar) {
            alloc.deallocate(get_ptr(), capacity());
            arr[0] = 0;
        }
    }

    basic_string &operator=(const basic_string &str) {
        reserve(str.size());
        std::copy(str.begin(), str.end(), begin());
        set_size(str.size());
        (*end()) = '\0';
        return *this;
    }
    basic_string &operator=(basic_string && str) noexcept(std::is_nothrow_move_assignable<allocator_type>::value) {
        if (arr[0] == nullchar) {
            alloc.deallocate(get_ptr(), size());
        }
        alloc = std::move(str.alloc);
        arr = str.arr;
for (auto & a : str.arr) {
            a = '\0';
        }
        return *this;
    }
    basic_string &operator=(const_pointer s) {
        auto _size = traits_type::length(s);
        reserve(_size);
        std::copy(s, s + _size, begin());
        set_size(_size);
        (*end()) = '\0';
        return *this;
    }
    basic_string &operator=(value_type ch) {
        auto _size = 1;
        reserve(_size);
        set_size(_size);
        (*begin()) = ch;
        (*end()) = '\0';
        return *this;
    }
    basic_string &operator=(std::initializer_list<value_type> ilist) {
        reserve(ilist.size());
        std::copy(ilist.begin(), ilist.end(), begin());
        set_size(ilist.size());
        (*end()) = '\0';
        return *this;
    }

    // Compatibility with std::string
    basic_string &operator= (std::basic_string<Char, traits_type, allocator_type> &o) {
        alloc = o.get_allocator();
        return assign(o.c_str(), o.size());
    }

    basic_string &assign(size_type count, value_type ch) {
        reserve(count);
        auto iter = begin();
        for (size_type i = 0; i < count; ++i) {
            *(iter++) = ch;
        }
        set_size(count);
        *(end()) = '\0';
        return *this;
    }
    basic_string &assign(const basic_string &str) {
        (*this) = str;
    }
    basic_string &assign(const basic_string &str,
                         size_type pos,
                         size_type count) {
        auto os = str.size();
        if (pos > os)
            throw std::out_of_range("Tried to assign a string with a pos > size");
        auto cnt = (pos + count) > os ? os - pos : count;
        return assign(str.c_str() + pos, cnt);
    }
    basic_string &assign(basic_string && str) {
        *(this) = std::move(str);
        return *this;
    }
    basic_string &assign(const_pointer s,
                         size_type count) {
        reserve(count);
        std::copy(s, s + count, begin());
        set_size(count);
        *(end()) = '\0';
        return *this;
    }
    basic_string &assign(const_pointer s) {
        *this = s;
        return *this;
    }
    template< class InputIt >
    basic_string &assign(InputIt first, InputIt last) {
        auto s = std::distance(first, last);
        reserve(s);
        std::copy(first, last, begin());
        set_size(s);
        *(end()) = '\0';
        return *this;
    }
    basic_string &assign(std::initializer_list<value_type> ilist) {
        reserve(ilist.size());
        std::copy(ilist.begin(), ilist.end(), begin());
        set_size(ilist.size());
        *(end()) = '\0';
        return *this;
    }

    allocator_type get_allocator() const noexcept {
        return alloc;
    }
public: // iterators
    iterator begin() {
        return get_ptr();
    }
    const_iterator begin() const {
        return get_ptr();
    }
    const_iterator cbegin() const {
        return get_ptr();
    }
    iterator end() {
        return get_ptr() + size();
    }
    const_iterator end() const {
        return get_ptr() + size();
    }
    const_iterator cend() const {
        return get_ptr() + size();
    }
    reverse_iterator rbegin() {
        return reverse_iterator(end());
    }
    const_reverse_iterator rbegin() const {
        return const_reverse_iterator(end());
    }
    const_reverse_iterator crbegin() const {
        return const_reverse_iterator(end());
    }
    reverse_iterator rend() {
        return reverse_iterator(begin());
    }
    const_reverse_iterator rend() const {
        return const_reverse_iterator(begin());
    }
    const_reverse_iterator crend() const {
        return const_reverse_iterator(begin());
    }
public: // Capacity
    size_type size() const {
        return get_size();
    }
    size_type length() const {
        return size();
    }
    bool empty() const {
        return size() == 0;
    }
    size_type max_size() const {
        return alloc.max_size() - 1;
    }
    size_type capacity() const {
        if (arr[0] != nullchar)
            return max_in_buffer_size();
        return get_capacity();
    }
    void shrink_to_fit() {
        auto s = size();
        auto c = capacity();
        if (s == c) return;
        if (c <= max_in_buffer_size()) return;
        if (s <= max_in_buffer_size()) {
            auto ptr = get_ptr();
            arr[0] = (unsigned char)(s);
            std::copy(ptr, ptr + s, arr.begin() + 1);
            alloc.deallocate(ptr, c);
            arr[1 + s] = '\0';
        } else {
            auto nptr = alloc.allocate(s + 1);
            std::copy(begin(), end(), nptr);
            nptr[s] = '\0';
            alloc.deallocate(get_ptr(), c);
            set_ptr(nptr);
            set_capacity(s);
        }
    }
    void reserve(size_type new_cap = 0) {
        if (new_cap <= capacity()) return;
        if (new_cap > max_size()) throw std::length_error("Cannot make a bigger string than max_size");
        // we at least double the capacity
        auto sz = size();
        new_cap = std::max(capacity() * 2, new_cap);
        auto nptr = alloc.allocate(new_cap + 1);
        if (!nptr)
            throw std::bad_alloc();
        auto ptr = get_ptr();
        std::copy(ptr, ptr + size() + 1, nptr);
        if (arr[0] != nullchar) {
            arr[0] = nullchar;
            set_size(sz);
        } else {
            alloc.deallocate(ptr, get_capacity());
        }
        set_capacity(new_cap);
        set_ptr(nptr);
    }
public: // Operations
    void clear() {
        get_ptr()[0] = '\0';
        set_size(0);
    }
    basic_string &insert(size_type index, size_type count, value_type ch) {
        auto sz = size();
        if (index > sz) throw std::out_of_range("Could not insert at a position after string");
        auto nsize = size() + count;
        reserve(nsize);
        auto ptr = get_ptr();
        ptr[nsize] = '\0';
        auto chars_to_move = sz - index;
        for (size_type i = 0; i < chars_to_move; ++i) {
            ptr[nsize - 1 - i] = ptr[sz - 1 - i];
        }
        for (size_type i = 0; i < count; ++i) {
            ptr[index + i] = ch;
        }
        set_size(nsize);
        return *this;
    }
    basic_string &insert(size_type index, const_pointer s, size_type count) {
        auto sz = size();
        if (index > sz) throw std::out_of_range("Could not insert at a position after string");
        auto nsize = sz + count;
        reserve(nsize);
        auto ptr = get_ptr();
        ptr[nsize] = '\0';
        auto chars_to_move = sz - index;
        for (size_type i = 0; i < chars_to_move; ++i) {
            ptr[nsize - 1 - i] = ptr[sz - 1 - i];
        }
        for (size_type i = 0; i < count; ++i) {
            ptr[index + i] = s[i];
        }
        set_size(nsize);
        return *this;
    }
    basic_string &insert(size_type index, const_pointer s) {
        size_type count = traits_type::length(s);
        return insert(index, s, count);
    }
    basic_string &insert(size_type index, const basic_string &str) {
        return insert(index, str.get_ptr(), str.size());
    }
    basic_string &insert(size_type index, const basic_string &str,
                         size_type index_str, size_type count) {
        if (index_str > str.size()) throw std::out_of_range("Could not insert at a position after string");
        auto cnt = std::min(count, str.size() - index_str);
        return insert(index, str.get_ptr() + index_str, cnt);
    }
    iterator insert(const_iterator pos, value_type ch) {
        auto p = std::distance(cbegin(), pos);
        insert(p, 1, ch);
        return begin() + p;
    }
    iterator insert(const_iterator pos, size_type count, value_type ch) {
        auto p = std::distance(cbegin(), pos);
        insert(p, count, ch);
        return begin() + p;
    }
    template< class InputIt >
    iterator insert(const_iterator pos, InputIt first, InputIt last) {
        auto sz = size();
        auto index = pos - cbegin();
        auto count = std::distance(first, last);
        if (count < 0) {
            return begin() + index;
        }
        auto nsize = sz + count;
        reserve(nsize);
        auto ptr = get_ptr();
        ptr[nsize] = '\0';
        auto chars_to_move = sz - index;
        for (size_type i = 0; i < chars_to_move; ++i) {
            ptr[nsize - 1 - i] = ptr[sz - 1 - i];
        }
        for (size_type i = 0; i < static_cast<size_type>(count); ++i) {
            ptr[index + i] = *(first++);
        }
        set_size(nsize);
        return begin() + index;
    }
    iterator insert(const_iterator pos, std::initializer_list<value_type> ilist) {
        return insert(pos, ilist.begin(), ilist.end());
    }
    iterator erase(const_iterator first, const_iterator last) {
        auto count = std::distance(first, last);
        auto index = std::distance(cbegin(), first);
        auto f = begin() + index;
        auto res = f;
        auto l = f + count;
        while (l != end()) {
            *f = *l;
            ++f;
            ++l;
        }
        set_size(size() - count);
        (*this)[size()] = '\0';
        return res;
    }
    iterator erase(const_iterator position) {
        return erase(position, position + 1);
    }
    basic_string &erase(size_type index = 0, size_type count = npos) {
        if (index > size()) throw std::out_of_range("Could not erase at a position after string");
        const_iterator first = cbegin();
        std::advance(first, index);
        const_iterator last = (count == npos || count >= size() || index + count > size()) ? cend() : first + count;
        erase(first, last);
        return *this;
    }
    void push_back(value_type c) {
        if (size() == capacity())
            reserve(size() + 1);
        (*this)[size()] = c;
        set_size(size() + 1);
        (*this)[size()] = '\0';
    }
    void pop_back() {
        set_size(size() - 1);
        (*this)[size()] = '\0';
    }
    template< class InputIt >
    basic_string &append(InputIt first, InputIt last) {
        insert(end(), first, last);
        return *this;
    }
    basic_string &append(size_type count, value_type ch) {
        return insert(size(), count, ch);
    }
    basic_string &append(const basic_string &str) {
        return append(str.begin(), str.end());
    }
    basic_string &append(const basic_string &str,
                         size_type pos,
                         size_type count) {
        auto _begin = begin() + count;
        auto _end = pos + count > size() ? end() : _begin + count;
        return append(_begin, _end);
    }
    basic_string &append(const value_type* s,
                         size_type count) {
        return append(s, s + count);
    }
    basic_string &append(const value_type* s) {
        size_type count = traits_type::length(s);
        auto begin = s;
        return append(begin, begin + count);
    }
    basic_string &append(std::initializer_list<value_type> ilist) {
        return append(ilist.begin(), ilist.end());
    }
    basic_string &operator+=(const basic_string &str) {
        return append(str);
    }
    basic_string &operator+=(value_type ch) {
        return append(1, ch);
    }
    basic_string &operator+=(const_pointer s) {
        return append(s);
    }
    basic_string &operator+=(std::initializer_list<value_type> ilist) {
        return append(ilist);
    }
    int compare(size_type pos1, size_type count1,
                const_pointer s, size_type count2) const {
        auto ts = size();
        if (pos1 > ts) throw std::out_of_range("Can not compare with a substring positioned after string");
        if (pos1 + count1 > ts) {
            count1 = ts - pos1;
        }
        ts = count1;
        auto rlen = std::min(ts, count2);
        auto res = traits_type::compare(data() + pos1, s, rlen);
        return res == 0 ? count1 - count2 : res;
    }
    int compare(const basic_string &str) const {
        return compare(0, npos, str.c_str(), str.size());
    }
    int compare(size_type pos, size_type count,
                const basic_string &str) const {
        return compare(pos, count, str.c_str(), str.size());
    }
    int compare(size_type pos1, size_type count1,
                const basic_string &str,
                size_type pos2, size_type count2) const {
        auto sz = str.size();
        if (pos2 > sz) throw std::out_of_range("Can not compare with a substring positioned after string");
        auto cnt2 = pos2 + count2 > sz ? sz - pos2 : count2;
        return compare(pos1, count1, str.c_str() + pos2, cnt2);
    }
    int compare(const value_type* s) const {
        return compare(0, npos, s, traits_type::length(s));
    }
    int compare(size_type pos1, size_type count1,
                const_pointer s) const {
        return compare(pos1, count1, s, traits_type::length(s));
    }
    template< class InputIt >
    basic_string &replace(const_iterator first, const_iterator last,
                          InputIt first2, InputIt last2) {
        auto _begin = begin() + std::distance(cbegin(), first);
        auto _end = _begin + std::distance(first, last);
        while (_begin != _end && first2 != last2) {
            *(_begin++) = *(first2++);
        }
        if (_begin != _end) {
            erase(_begin, _end);
        } else if (first2 != last2) {
            insert(_end, first2, last2);
        }
        return *this;
    }
    basic_string &replace(size_type pos, size_type count,
                          const basic_string &str) {
        if (pos > size()) throw std::out_of_range("Replace called with pos out of range");
        auto _begin = cbegin() + pos;
        auto _end = cend();
        if (size() > pos + count) _end = _begin + count;
        return replace(_begin, _end, str.begin(), str.end());
    }
    basic_string &replace(const_iterator first, const_iterator last,
                          const basic_string &str) {
        return replace(first, last, str.begin(), str.end());
    }
    basic_string &replace(size_type pos, size_type count,
                          const basic_string &str,
                          size_type pos2, size_type count2) {
        if (pos > size() || pos2 > str.size()) throw std::out_of_range("Replace called with pos out of range");
        auto _begin = cbegin() + pos;
        auto _end = cend();
        if (size() > pos + count) _end = _begin + count;
        auto first2 = str.cbegin() + pos2;
        auto last2 = str.cend();
        if (str.size() > pos2 + count2) last2 = first2 + count2;
        return replace(_begin, _end, first2, last2);
    }
    basic_string &replace(size_type pos, size_type count,
                          const value_type* cstr, size_type count2) {
        auto _begin = cbegin() + pos;
        auto _end = cend();
        if (size() > pos + count) _end = _begin + count;
        return replace(_begin, _end, cstr, cstr + count2);
    }
    basic_string &replace(const_iterator first, const_iterator last,
                          const value_type* cstr, size_type count2) {
        return replace(first, last, cstr, cstr + count2);
    }
    basic_string &replace(size_type pos, size_type count,
                          const value_type* cstr) {
        size_type cnt = traits_type::length(cstr);
        return replace(pos, count, cstr, cnt);
    }
    basic_string &replace(const_iterator first, const_iterator last,
                          const value_type* cstr) {
        size_type cnt = traits_type::length(cstr);
        return replace(first, last, cstr, cnt);
    }
    basic_string &replace(const_iterator first, const_iterator last,
                          std::initializer_list<value_type> ilist) {
        return replace(first, last, ilist.begin(), ilist.end());
    }
    basic_string &replace(size_type pos, size_type count,
                          size_type count2, value_type ch) {
        if (pos > size()) throw std::out_of_range("Replace called with pos out of range");
        auto _begin = begin() + pos;
        auto _end = end();
        if (pos + count < size()) _end = _begin + count;
        while (count2 && _begin != _end) {
            *(_begin++) = ch;
            --count2;
        }
        if (count2) {
            insert(_begin, count2, ch);
        } else if (_begin != _end) {
            erase(_begin, _end);
        }
        return *this;
    }
    basic_string &replace(const_iterator first, const_iterator last,
                          size_type count2, value_type ch) {
        return replace(std::distance(cbegin(), first), std::distance(first, last), count2, ch);
    }
    basic_string substr(size_type pos = 0,
                        size_type count = npos) const {
        return basic_string<value_type, traits_type, allocator_type>(*this, pos, count, alloc);
    }
    size_type copy(value_type* dest,
                   size_type count,
                   size_type pos = 0) const {
        if (pos > size()) {
            throw std::out_of_range("tried to copy too much of a string");
        }
        size_type res = 0;
        auto _begin = begin() + pos;
        auto _end = end();
        while (_begin != _end && count) {
            *(dest++) = *(_begin++);
            --count;
            ++res;
        }
        return res;
    }
    void resize(size_type count, value_type ch) {
        auto old_size = size();
        reserve(count);
        auto _begin = begin();
        *(_begin + count) = '\0';
        _begin = end();
        set_size(count);
        if (old_size >= count) return;
        auto _end = end();
        while (_begin != _end) {
            *(_begin++) = ch;
        }
    }
    void resize(size_type count) {
        resize(count, value_type());
    }
    void swap(basic_string &other) {
        auto tarr = arr;
        arr = other.arr;
        other.arr = tarr;
    }
public: // Element access
    reference operator[](size_type pos) {
        return begin()[pos];
    }
    const_reference operator[](size_type pos) const {
        return begin()[pos];
    }
    const value_type* data() const {
        return begin();
    }
    const value_type* c_str() const {
        return begin();
    }
    reference at(size_type pos) {
        if (pos >= size())
            throw std::out_of_range("Out of range error in call to basic_string::at");
        return *(begin() + pos);
    }
    const_reference at(size_type pos) const {
        if (pos >= size())
            throw std::out_of_range("Out of range error in call to basic_string::at");
        return *(begin() + pos);
    }
    reference front() {
        return *(begin());
    }
    const_reference front() const {
        return *(begin());
    }
    reference back() {
        return *(end() - 1);
    }
    const_reference back() const {
        return *(end() - 1);
    }
public: // search
    size_type find(const_pointer s, size_type pos, size_type count) const {
        auto sz = size();
        if (pos >= sz) return npos;
        auto res = pos;
        auto _begin = begin();
        auto _end = end();
        while (_begin + count != _end) {
            auto iterA = s;
            auto iterB = _begin;
            auto matches = 0;
            while (*iterA == *iterB && matches < count) {
                ++iterA;
                ++iterB;
                ++matches;
            }
            if (matches == count) return res;
            ++res;
        }
        return npos;
    }
    size_type find(const_pointer s, size_type pos = 0) const {
        return find(s, pos, traits_type::length(s));
    }
    size_type find(const basic_string &str, size_type pos = 0) const {
        return find(str.c_str(), pos, str.size());
    }
    size_type find(value_type ch, size_type pos = 0) const {
        if (pos >= size()) return npos;
        auto ptr = c_str();
        auto res = traits_type::find(ptr + pos, size() - pos, ch);
        return res == nullptr ? npos : std::distance(ptr, res);
    }
    size_type rfind(const_pointer s, size_type pos, size_type count) const {
        auto sz = size();
        if (pos >= sz) pos = sz;
        auto last = begin() - 1;
        auto iter = begin() + pos;
        if (count > sz) return npos;
        for (; iter != last; --iter) {
            bool match = true;
            auto iter2 = iter;
            for (size_type i = 0; i < count; ++i) {
                if (*iter2 != s[i]) {
                    match = false;
                    break;
                }
                ++iter2;
            }
            if (match) return std::distance(begin(), iter);
        }
        return npos;
    }
    size_type rfind(const basic_string &str, size_type pos = npos) const {
        return rfind(str.c_str(), pos, str.size());
    }
    size_type rfind(const_pointer s, size_type pos = npos) const {
        return rfind(s, pos, traits_type::length(s));
    }
    size_type rfind(value_type ch, size_type pos = npos) const {
        return rfind(&ch, pos, 1);
    }
    size_type find_first_of(const_pointer s, size_type pos, size_type count) const {
        if (pos >= size()) return npos;
        auto _begin = cbegin() + pos;
        auto _end = end();
        while (_begin != _end) {
            for (size_type i = 0; i < count; ++i) {
                if (*_begin == s[i]) {
                    return std::distance(cbegin(), _begin);
                }
            }
            ++_begin;
        }
        return npos;
    }
    size_type find_first_of(const basic_string &str, size_type pos = 0) const {
        return find_first_of(str.c_str(), pos, str.size());
    }
    size_type find_first_of(const_pointer s, size_type pos = 0) const {
        return find_first_of(s, pos, traits_type::length(s));
    }
    size_type find_first_of(value_type ch, size_type pos = 0) const {
        if (pos >= size()) return npos;
        auto _begin = cbegin() + pos;
        auto _end = end();
        while (_begin != _end) {
            if (*_begin == ch) return std::distance(cbegin(), _begin);
            ++_begin;
        }
        return npos;
    }
    size_type find_first_not_of(const_pointer s, size_type pos, size_type count) const {
        if (pos >= size()) return npos;
        auto _begin = cbegin() + pos;
        auto _end = end();
        while (_begin != _end) {
            bool match = false;
            for (size_type i = 0; i < count; ++i) {
                if (*_begin == s[i]) {
                    match = true;
                    break;
                }
            }
            if (!match) return std::distance(begin(), _begin);
            ++_begin;
        }
        return npos;
    }
    size_type find_first_not_of(const basic_string &str, size_type pos = 0) const {
        return find_first_not_of(str.c_str(), pos, str.size());
    }
    size_type find_first_not_of(const_pointer s, size_type pos = 0) const {
        return find_first_not_of(s, pos, traits_type::length(pos));
    }
    size_type find_first_not_of(value_type ch, size_type pos = 0) const {
        if (pos >= size()) return npos;
        auto _begin = cbegin() + pos;
        auto _end = end();
        for (; _begin != _end; ++_begin) {
            if (*_begin != ch)
                return std::distance(begin(), _begin);
        }
        return npos;
    }
    size_type find_last_of(const_pointer s, size_type pos, size_type count) const {
        auto iter = end() - 1 - (pos >= size() ? 0 : pos);
        auto _end = begin() - 1;
        for (; iter != _end; --iter) {
            for (size_type i = 0; i < count; ++i) {
                if (*iter == s[i]) return std::distance(begin(), iter);
            }
        }
        return npos;
    }
    size_type find_last_of(const basic_string &str, size_type pos = npos) const {
        return find_last_of(str.c_str(), pos, str.size());
    }
    size_type find_last_of(const_pointer* s, size_type pos = npos) const {
        return find_last_of(s, pos, traits_type::length(pos));
    }
    size_type find_last_of(value_type ch, size_type pos = npos) const {
        return find_last_of(&ch, pos, 1);
    }
    size_type find_last_not_of(const_pointer s, size_type pos, size_type count) const {
        auto iter = end() - 1 - (pos >= size() ? 0 : pos);
        auto _end = begin() - 1;
        for (; iter != _end; --iter) {
            bool match = false;
            for (size_type i = 0; i < count; ++i) {
                if (*iter == s[i]) {
                    match = true;
                    break;
                }
            }
            if (!match) return std::distance(begin(), iter);
        }
        return npos;
    }
    size_type find_last_not_of(const basic_string &str, size_type pos = npos) const {
        return find_last_not_of(str.c_str(), pos, str.size());
    }
    size_type find_last_not_of(const_pointer s, size_type pos = npos) const {
        return find_last_not_of(s, pos, traits_type::length(s));
    }
    size_type find_last_not_of(value_type ch, size_type pos = npos) const {
        return find_last_not_of(&ch, pos, 1);
    }
};

template< class CharT, class Traits, class Alloc >
basic_string<CharT, Traits, Alloc>
operator+(const basic_string<CharT, Traits, Alloc> &lhs,
          const basic_string<CharT, Traits, Alloc> &rhs) {

    basic_string<CharT, Traits, Alloc> res;
    res.reserve(lhs.size() + rhs.size());
    return res.append(lhs).append(rhs);
}

template< class CharT, class Traits, class Alloc >
basic_string<CharT, Traits, Alloc>
operator+(const CharT* lhs,
          const basic_string<CharT, Traits, Alloc> &rhs) {
    auto s = Traits::length(lhs);
    basic_string<CharT, Traits, Alloc> res;
    res.reserve(s + rhs.size());
    return res.append(lhs, s).append(rhs);
}

template< class CharT, class Traits, class Alloc >
basic_string<CharT, Traits, Alloc>
operator+(CharT lhs,
          const basic_string<CharT, Traits, Alloc> &rhs) {
    basic_string<CharT, Traits, Alloc> res;
    res.reserve(1 + rhs.size());
    return res.append(1, lhs).append(rhs);
}

template< class CharT, class Traits, class Alloc >
basic_string<CharT, Traits, Alloc>
operator+(const basic_string<CharT, Traits, Alloc> &lhs,
          const CharT* rhs) {
    auto s = Traits::length(rhs);
    basic_string<CharT, Traits, Alloc> res;
    res.reserve(s + lhs.size());
    return res.append(lhs).append(rhs, s);
}

template<class CharT, class Traits, class Alloc>
basic_string<CharT, Traits, Alloc>
operator+(const basic_string<CharT, Traits, Alloc> &lhs,
          CharT rhs) {
    basic_string<CharT, Traits, Alloc> res;
    res.reserve(1 + lhs.size());
    return res.append(lhs).append(1, rhs);
}

template< class CharT, class Traits, class Alloc >
basic_string<CharT, Traits, Alloc>
operator+(basic_string<CharT, Traits, Alloc> && lhs,
          const basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.append(rhs);
}

template< class CharT, class Traits, class Alloc >
basic_string<CharT, Traits, Alloc>
operator+(const basic_string<CharT, Traits, Alloc> &lhs,
          basic_string<CharT, Traits, Alloc> && rhs) {
    basic_string<CharT, Traits, Alloc> res;
    res.reserve(lhs.size() + rhs.size());
    return res.append(lhs).append(std::move(rhs));
}

template< class CharT, class Traits, class Alloc >
basic_string<CharT, Traits, Alloc>
operator+(basic_string<CharT, Traits, Alloc> && lhs,
          basic_string<CharT, Traits, Alloc> && rhs) {
    return lhs.append(std::move(rhs));
}

template< class CharT, class Traits, class Alloc >
basic_string<CharT, Traits, Alloc>
operator+(const CharT* lhs,
          basic_string<CharT, Traits, Alloc> && rhs) {
    auto s = Traits::length(lhs);
    basic_string<CharT, Traits, Alloc> res;
    res.reserve(s + rhs.size());
    return res.append(lhs, s).append(std::move(rhs));
}

template< class CharT, class Traits, class Alloc >
basic_string<CharT, Traits, Alloc>
operator+(CharT lhs,
          basic_string<CharT, Traits, Alloc> && rhs) {
    basic_string<CharT, Traits, Alloc> res;
    res.reserve(1 + rhs.size());
    return res.append(1, lhs).append(std::move(rhs));
}

template< class CharT, class Traits, class Alloc >
basic_string<CharT, Traits, Alloc>
operator+(basic_string<CharT, Traits, Alloc> && lhs,
          const CharT* rhs) {
    return lhs.append(rhs);
}

template< class CharT, class Traits, class Alloc >
basic_string<CharT, Traits, Alloc>
operator+(basic_string<CharT, Traits, Alloc> && lhs,
          CharT rhs) {
    return lhs.append(1, rhs);
}

template< class CharT, class Traits, class Alloc >
bool operator==(const basic_string<CharT, Traits, Alloc> &lhs,
                const basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs) == 0;
}

template< class CharT, class Traits, class Alloc >
bool operator!=(const basic_string<CharT, Traits, Alloc> &lhs,
                const basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs) != 0;
}

template< class CharT, class Traits, class Alloc >
bool operator<(const basic_string<CharT, Traits, Alloc> &lhs,
               const basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs) < 0;
}

template< class CharT, class Traits, class Alloc >
bool operator<=(const basic_string<CharT, Traits, Alloc> &lhs,
                const basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs) <= 0;
}

template< class CharT, class Traits, class Alloc >
bool operator>(const basic_string<CharT, Traits, Alloc> &lhs,
               const basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs) > 0;
}

template< class CharT, class Traits, class Alloc >
bool operator>=(const basic_string<CharT, Traits, Alloc> &lhs,
                const basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs) >= 0;
}

template< class CharT, class Traits, class Alloc >
bool operator==(const CharT* lhs, const basic_string<CharT, Traits, Alloc> &rhs) {
    return rhs.compare(lhs) == 0;
}

template< class CharT, class Traits, class Alloc >
bool operator==(const basic_string<CharT, Traits, Alloc> &lhs, const CharT* rhs) {
    return lhs.compare(rhs) == 0;
}

template< class CharT, class Traits, class Alloc >
bool operator==(const basic_string<CharT, Traits, Alloc> &lhs, const std::basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs.c_str(), rhs.size()) == 0;
}

template< class CharT, class Traits, class Alloc >
bool operator==(const std::basic_string<CharT, Traits, Alloc> &lhs, const basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(0, rhs.size(), rhs.c_str(), rhs.size()) == 0;
}

template< class CharT, class Traits, class Alloc >
bool operator!=(const CharT* lhs, const basic_string<CharT, Traits, Alloc> &rhs) {
    return rhs.compare(lhs) != 0;
}

template< class CharT, class Traits, class Alloc >
bool operator!=(const basic_string<CharT, Traits, Alloc> &lhs, const CharT* rhs) {
    return lhs.compare(rhs) != 0;
}

template< class CharT, class Traits, class Alloc >
bool operator!=(const basic_string<CharT, Traits, Alloc> &lhs, const std::basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs.c_str(), rhs.size()) != 0;
}

template< class CharT, class Traits, class Alloc >
bool operator!=(const std::basic_string<CharT, Traits, Alloc> &lhs, const basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs.c_str(), rhs.size()) != 0;
}

template< class CharT, class Traits, class Alloc >
bool operator<(const CharT* lhs, const basic_string<CharT, Traits, Alloc> &rhs) {
    return rhs.compare(lhs) > 0;
}

template< class CharT, class Traits, class Alloc >
bool operator<(const basic_string<CharT, Traits, Alloc> &lhs,  const CharT* rhs) {
    return lhs.compare(rhs) < 0;
}

template< class CharT, class Traits, class Alloc >
bool operator<(const basic_string<CharT, Traits, Alloc> &lhs, const std::basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs.c_str(), rhs.size()) < 0;
}

template< class CharT, class Traits, class Alloc >
bool operator<(const std::basic_string<CharT, Traits, Alloc> &lhs, const basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs.c_str(), rhs.size()) < 0;
}

template< class CharT, class Traits, class Alloc >
bool operator<=(const CharT* lhs, const basic_string<CharT, Traits, Alloc> &rhs) {
    return rhs.compare(lhs) >= 0;
}

template< class CharT, class Traits, class Alloc >
bool operator<=(const basic_string<CharT, Traits, Alloc> &lhs, const CharT* rhs) {
    return lhs.compare(rhs) <= 0;
}

template< class CharT, class Traits, class Alloc >
bool operator<=(const basic_string<CharT, Traits, Alloc> &lhs, const std::basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs.c_str(), rhs.size()) <= 0;
}

template< class CharT, class Traits, class Alloc >
bool operator<=(const std::basic_string<CharT, Traits, Alloc> &lhs, const basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs.c_str(), rhs.size()) <= 0;
}

template< class CharT, class Traits, class Alloc >
bool operator>(const CharT* lhs, const basic_string<CharT, Traits, Alloc> &rhs) {
    return rhs.compare(lhs) < 0;
}

template< class CharT, class Traits, class Alloc >
bool operator>(const basic_string<CharT, Traits, Alloc> &lhs, const CharT* rhs) {
    return lhs.compare(rhs) > 0;
}

template< class CharT, class Traits, class Alloc >
bool operator>(const basic_string<CharT, Traits, Alloc> &lhs, const std::basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs.c_str(), rhs.size()) > 0;
}

template< class CharT, class Traits, class Alloc >
bool operator>(const std::basic_string<CharT, Traits, Alloc> &lhs, const basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs.c_str(), rhs.size()) > 0;
}

template< class CharT, class Traits, class Alloc >
bool operator>=(const CharT* lhs, const basic_string<CharT, Traits, Alloc> &rhs) {
    return rhs.compare(lhs) <= 0;
}

template< class CharT, class Traits, class Alloc >
bool operator>=(const basic_string<CharT, Traits, Alloc> &lhs, const CharT* rhs) {
    return lhs.compare(rhs) >= 0;
}

template< class CharT, class Traits, class Alloc >
bool operator>=(const basic_string<CharT, Traits, Alloc> &lhs, const std::basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs.c_str(), rhs.size()) >= 0;
}

template< class CharT, class Traits, class Alloc >
bool operator>=(const std::basic_string<CharT, Traits, Alloc> &lhs, const basic_string<CharT, Traits, Alloc> &rhs) {
    return lhs.compare(rhs.c_str(), rhs.size()) >= 0;
}

using string = basic_string<char>;
using wstring = basic_string<wchar_t>;

template<class CharT, class Traits, class Allocator>
size_t hash_value(const crossbow::basic_string<CharT, Traits, Allocator> &str) {
    constexpr size_t FNV_offset_basis = 14695981039346656037ul;
    auto hash = FNV_offset_basis;
    for (auto i = str.begin(); i != str.end(); ++i) {
        hash *= FNV_offset_basis;
        hash ^= size_t(*i);
    }
    return hash;
}

template<class _CharT, class _Traits, class _Allocator>
std::basic_ostream<_CharT, _Traits> &
operator<<(std::basic_ostream<_CharT, _Traits> &__os,
           const basic_string<_CharT, _Traits, _Allocator> &__str) {
    return __os << __str.c_str();
}

template <class CharT, class Traits, class Allocator>
std::basic_istream<CharT, Traits> &
operator>>(std::basic_istream<CharT, Traits> &is,
           basic_string<CharT, Traits, Allocator> &str) {
    typename std::basic_istream<CharT, Traits>::sentry s(is);
    if (s) {
        str.clear();
        std::streamsize n = is.width();
        if (n <= 0)
            n = str.max_size();
        if (n <= 0)
            n = std::numeric_limits<std::streamsize>::max();
        std::streamsize c = 0;
        const std::ctype<CharT> &ct = std::use_facet<std::ctype<CharT> >(is.getloc());
        std::ios_base::iostate err = std::ios_base::goodbit;
        while (c < n) {
            typename Traits::int_type i = is.rdbuf()->sgetc();
            if (Traits::eq_int_type(i, Traits::eof())) {
                err |= std::ios_base::eofbit;
                break;
            }
            CharT ch = Traits::to_char_type(i);
            if (ct.is(ct.space, ch))
                break;
            str.push_back(ch);
            ++c;
            is.rdbuf()->sbumpc();
        }
        is.width(0);
        if (c == 0)
            err |= std::ios_base::failbit;
        is.setstate(err);
    } else {
        is.setstate(std::ios_base::failbit);
    }
    return is;
}

namespace impl {
template<class S, class P, class V>
inline S as_string(P snprintf_like, S s, const typename S::value_type* fmt, V a) {
    using size_type = typename S::size_type;
    size_type available = s.size();
    while (true) {
        int status = snprintf_like(&s[0], available + 1, fmt, a);
        if (status >= 0) {
            size_type used = static_cast<size_type>(status);
            if (used <= available) {
                s.resize(used);
                break;
            }
            available = used;
        } else {
            available = available * 2 + 1;
        }
        s.resize(available);
    }
    return s;
}

template<class S, class V, bool = std::is_floating_point<V>::value>
struct initial_string;

template<class V, bool b>
struct initial_string<crossbow::string, V, b> {
    crossbow::string operator()() const {
        crossbow::string s;
        s.resize(s.capacity());
        return s;
    }
};

template<class V, bool b>
struct initial_string<crossbow::wstring, V, b> {
    crossbow::wstring operator()() const {
        crossbow::wstring s;
        s.resize(s.capacity());
        return s;
    }
};

}

template<class V>
string to_string(V value);

template<>
inline string to_string<short>(short value) {
    return impl::as_string(snprintf, impl::initial_string<string, short>()(), "%hd", value);
}

template<>
inline string to_string<int>(int value) {
    return impl::as_string(snprintf, impl::initial_string<string, int>()(), "%d", value);
}

template<>
inline string to_string<long>(long value) {
    return impl::as_string(snprintf, impl::initial_string<string, long>()(), "%ld", value);
}

template<>
inline string to_string<long long>(long long value) {
    return impl::as_string(snprintf, impl::initial_string<string, int>()(), "%lld", value);
}

template<>
inline string to_string<unsigned short>(unsigned short value) {
    return impl::as_string(snprintf, impl::initial_string<string, short>()(), "%hu", value);
}

template<>
inline string to_string<unsigned>(unsigned value) {
    return impl::as_string(snprintf, impl::initial_string<string, int>()(), "%u", value);
}

template<>
inline string to_string<unsigned long>(unsigned long value) {
    return impl::as_string(snprintf, impl::initial_string<string, int>()(), "%lu", value);
}

template<>
inline string to_string<unsigned long long>(unsigned long long value) {
    return impl::as_string(snprintf, impl::initial_string<string, int>()(), "%llu", value);
}

template<>
inline string to_string<float>(float value) {
    return impl::as_string(snprintf, impl::initial_string<string, int>()(), "%f", value);
}

template<>
inline string to_string<double>(double value) {
    return impl::as_string(snprintf, impl::initial_string<string, int>()(), "%f", value);
}

template<>
inline string to_string<long double>(long double value) {
    return impl::as_string(snprintf, impl::initial_string<string, int>()(), "%Lf", value);
}

}

namespace std {

template<class _CharT, class _Traits, class _Allocator>
basic_istream<_CharT, _Traits> &
getline(basic_istream<_CharT, _Traits> &__is,
        crossbow::basic_string<_CharT, _Traits, _Allocator> &__str, _CharT __dlm) {
    typename basic_istream<_CharT, _Traits>::sentry __sen(__is, true);
    if (__sen) {
        __str.clear();
        ios_base::iostate __err = ios_base::goodbit;
        streamsize __extr = 0;
        while (true) {
            typename _Traits::int_type __i = __is.rdbuf()->sbumpc();
            if (_Traits::eq_int_type(__i, _Traits::eof())) {
                __err |= ios_base::eofbit;
                break;
            }
            ++__extr;
            _CharT __ch = _Traits::to_char_type(__i);
            if (_Traits::eq(__ch, __dlm))
                break;
            __str.push_back(__ch);
            if (__str.size() == __str.max_size()) {
                __err |= ios_base::failbit;
                break;
            }
        }
        if (__extr == 0)
            __err |= ios_base::failbit;
        __is.setstate(__err);
    }
    return __is;
}

template<class _CharT, class _Traits, class _Allocator>
basic_istream<_CharT, _Traits> &
getline(basic_istream<_CharT, _Traits> &__is,
        crossbow::basic_string<_CharT, _Traits, _Allocator> &__str) {
    return getline(__is, __str, __is.widen('\n'));
}

template< class T, class Traits, class Alloc >
void swap(crossbow::basic_string<T, Traits, Alloc> &lhs, crossbow::basic_string<T, Traits, Alloc> &rhs) {
    lhs.swap(rhs);
}

template<class CharT, class Traits, class Allocator>
struct hash<crossbow::basic_string<CharT, Traits, Allocator> > {
    size_t operator()(const crossbow::basic_string<CharT, Traits, Allocator> &str) const {
        return crossbow::hash_value(str);
    }
};

}

