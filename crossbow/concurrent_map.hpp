#pragma once

#include <functional>
#include <memory>
#include <utility>
#include <vector>
#include <initializer_list>
#include <array>
#include <atomic>
#include <mutex>
#include <type_traits>
#include <limits>
#include <stdint.h>

namespace crossbow {

template<
    typename Key,
    typename T,
    typename Hash = std::hash<Key>,
    typename KeyEqual = std::equal_to<Key>,
    typename Allocator = std::allocator<std::pair<const Key, T> >,
    typename MutexType = std::mutex,
    size_t ConcurrencyLevel = 32,
    size_t InitialCapacity = 32,
    size_t LoadFactor = 75
>
class concurrent_map
{
public:
    typedef Key key_type;
    typedef T mapped_type;
    typedef const T const_mapped_type;
    typedef size_t size_type;
    typedef std::ptrdiff_t difference_type;
    typedef Hash hasher;
    typedef KeyEqual key_equal;
    typedef Allocator allocator_type;
    typedef typename allocator_type::pointer pointer;
    typedef typename allocator_type::const_pointer const_pointer;
    typedef MutexType mutex_type;
private:
     enum class ElemState{
            UNASSIGNED,
            DELETED,
            VALID
    };

    struct KeyValueElement{
        template <typename K, typename V>
        KeyValueElement(K && key, V && value) : state(ElemState::VALID), key(std::forward<K>(key)), value(std::forward<V>(value)) {}
        KeyValueElement():state(ElemState::UNASSIGNED){}
        KeyValueElement(const KeyValueElement &) = default;
        KeyValueElement(KeyValueElement &&) = default;
        KeyValueElement & operator= (const KeyValueElement &) = default;
        KeyValueElement & operator= (KeyValueElement && other){
            state = other.state;
            key = std::move(other.key);
            value = std::move(other.value);
            return *this;
        }

        ElemState state;
        key_type key;
        mapped_type value;
    };

public: // private types

    struct Bucket {
        template <typename K, typename V>
        std::pair<bool, mapped_type> insert(K&& key, V&& value) {
            for (auto & el : arr){
                if (el.state == ElemState::UNASSIGNED) {
                    el.state = ElemState::VALID;
                    el.value = value;//TODO: forward this
                    el.key = key;
                    return std::make_pair(true, mapped_type());
                }
                else if (el.state == ElemState::VALID && el.key == key) {
                    mapped_type old_value = std::move(el.value);
                    el.value = value;//TODO: forward this
                    return std::make_pair(false, std::move(old_value));
                }
            }
            for (auto & el : overflow){
                //no need to check for validity, all entries in the vector are valid
                if (el.key == key) {
                    mapped_type old_value = std::move(el.value);
                    el.value = value;//TODO: forward this
                    return std::make_pair(false, std::move(old_value));
                }
            }
            //nothing with this key exists, array is full -> insert in vector
            overflow.push_back(KeyValueElement(std::forward<K>(key), std::forward<V>(value)));
            return std::make_pair(true, mapped_type());
        }

        void insertNoDuplicateCheck(KeyValueElement && element){
            for (auto & el : arr){
                if (el.state == ElemState::UNASSIGNED){
                    el = std::move(element);
                    return;
                }
            }
            overflow.push_back(std::move(element));
        }

        std::pair<bool, mapped_type> erase(const key_type &key){
            for (auto el = arr.begin(); el != arr.end(); ++el){
                if (el->state == ElemState::VALID && el->key == key) {
                    return erase(el);
                }
            }
            for (auto iter = overflow.begin(); iter != overflow.end(); ++iter){
                auto & el = *iter;
                //no need to check for validity, all entries in the vector are valid
                if (el.key == key) {
                    return erase(iter);
                }
            }
            //not found
            return std::make_pair(false, mapped_type());
        }
        std::pair<bool, mapped_type> at(const key_type &key){
            auto is_valid_and_equal = [&](KeyValueElement & el){
                return el.state == ElemState::VALID && el.key == key;
            };
            for (auto & el : arr){
                if (is_valid_and_equal(el)) {
                    return std::make_pair(true, el.value);
                }
            }
            for (auto & el : overflow){
                if (is_valid_and_equal(el)) {
                    return std::make_pair(true, el.value);
                }
            }
            return std::make_pair(false, mapped_type());
        }
        
        template<typename Fun>
        void for_each(const Fun& fun) {
            for (auto i = arr.begin(); i < arr.end(); ++i) {
                if (i->state == ElemState::VALID) {
                    fun(i->key, i->value);
                }
            }
            for (auto i = overflow.begin(); i < overflow.end(); ++i) {
                if (i->state == ElemState::VALID)
                    fun(i->key, i->value);
            }
        }
        
        template<typename Fun>
        void exec_on(size_t hash, const key_type& key, const Fun& fun, std::atomic_size_t& global_count) {
            auto is_valid_and_equal = [&](KeyValueElement & el){
                return el.state == ElemState::VALID && el.key == key;
            };
            for (auto i = arr.begin(); i != arr.end(); ++i){
                if (is_valid_and_equal(*i)) {
                    if (fun(i->value)) {
                        erase(i);
                        global_count.fetch_sub(1);
                    }
                    return;
                }
            }
            for (auto i = overflow.begin(); i != overflow.end(); ++i){
                if (is_valid_and_equal(*i)) {
                    if (fun(i->value)) {
                        erase(i);
                        global_count.fetch_sub(1);
                    }
                    return;
                }
            }
            auto p = KeyValueElement(key,mapped_type());
            if (!fun(p.value)){
                insertNoDuplicateCheck(std::move(p));
                global_count.fetch_add(1);
            }

        }
        
        void clear() {
            for (auto i = arr.begin(); i < arr.end(); ++i) {
                if (i->state == ElemState::VALID) {
                    erase(i);
                }
            }
            for (auto i = overflow.begin(); i < overflow.end(); ++i) {
                if (i->state == ElemState::VALID)
                    erase(i);
            }
        }

        typedef typename allocator_type::template rebind<KeyValueElement>::other  key_value_alloc;
        std::array<KeyValueElement,1> arr;
        std::vector<KeyValueElement, key_value_alloc> overflow;
    private:
        std::pair<bool, mapped_type> erase(decltype(arr.begin()) i)
        {
            auto res = std::move(i->value);
            {
                auto garbage = std::move(i->key);
                (void) garbage;
            }
            i->state = ElemState::UNASSIGNED;
            if (!overflow.empty()) {
                *i = std::move(overflow.back());
                overflow.pop_back();
            }
            return std::make_pair(true, res);
        }

        std::pair<bool, mapped_type> erase(decltype(overflow.begin()) i)
        {
            auto res = std::move(i->value);
            i->state = ElemState::UNASSIGNED;
            overflow.erase(i);
            return std::make_pair(true, res);
        }
    };

private: // data members
    hasher hash_;
    key_equal equal_;
    allocator_type allocator_;
    std::vector<Bucket, typename allocator_type::template rebind<Bucket>::other> _buckets;
    std::array<mutex_type, ConcurrencyLevel> _locks;
    std::atomic_size_t _count;
    std::atomic_size_t _upper_bound;
    size_t bucket_flag;

public: // construction and destruction
    explicit concurrent_map(const hasher& hash = hasher(),
                            const key_equal& equal = key_equal(),
                            const allocator_type& allocator = allocator_type())
        : hash_(hash),
          equal_(equal),
          allocator_(allocator),
          _buckets(InitialCapacity),
          _count(0),
          _upper_bound(InitialCapacity*LoadFactor/100),
          bucket_flag(((size_t)-1) % _buckets.size())
    {
    }
    
    concurrent_map(const concurrent_map<Key, T, Hash, KeyEqual, Allocator, MutexType, ConcurrencyLevel, InitialCapacity, LoadFactor>&) = default;
    concurrent_map(concurrent_map<Key, T, Hash, KeyEqual, Allocator, MutexType, ConcurrencyLevel, InitialCapacity, LoadFactor>&&) = default;

    
    concurrent_map<Key, T, Hash, KeyEqual, Allocator, MutexType, ConcurrencyLevel, InitialCapacity, LoadFactor>&
        operator= (const concurrent_map<Key, T, Hash, KeyEqual, Allocator, MutexType, ConcurrencyLevel, InitialCapacity, LoadFactor>&) = default;
    concurrent_map<Key, T, Hash, KeyEqual, Allocator, MutexType, ConcurrencyLevel, InitialCapacity, LoadFactor>&
        operator= (concurrent_map<Key, T, Hash, KeyEqual, Allocator, MutexType, ConcurrencyLevel, InitialCapacity, LoadFactor>&&) = default;
    
    allocator_type get_allocator() const { return allocator_; }

public: 
    size_t size() { return _count;}
    template <typename K, typename V>
    std::pair<bool, mapped_type> insert(K&& key, V&& value)
    {
        size_t hash = hash_(key);
        reserve(_count);
        std::lock_guard<mutex_type> l(getMutex(hash));
        auto ret = getBucket(hash).insert(std::forward<K>(key), std::forward<V>(value));
        if (ret.first)//new entry was inserted
            _count.fetch_add(1);
        return ret;
    }

    std::pair<bool, mapped_type> erase(const key_type& key) 
    {
        size_t hash = hash_(key);
        std::lock_guard<mutex_type> l(getMutex(hash));
        auto ret = getBucket(hash).erase(key);
        if (ret.first)//an entry was removed
            _count.fetch_sub(1);
        return ret;

    }

    std::pair<bool, mapped_type> at(const key_type& key)
    {
        size_t hash = hash_(key);
        std::lock_guard<mutex_type> l(getMutex(hash));
        return getBucket(hash).at(key);
    }
   
    void clear() {
        clear(0);
    }

    template<typename Fun>
    void exec_on(const key_type& key, const Fun& fun)
    {
        size_t hash = hash_(key);
        reserve(_count);
        std::lock_guard<mutex_type> l(getMutex(hash));
        getBucket(hash).exec_on(hash, key, fun, _count);
    }

    template<typename Fun>
    void for_each(const Fun& fun) {
        for_each(fun, 0);
    }
    
    //allocates space to fit count elements
    void reserve(size_t count){
        if (count < _upper_bound.load(std::memory_order_acquire))
            return;
        std::lock_guard<mutex_type> l(_locks[0]);
        if (count < _upper_bound.load(std::memory_order_acquire))
            return;
        auto new_size = _buckets.size()*2;
        while (new_size *  LoadFactor / 100 < count){
            new_size *= 2;
        }
        return resize(new_size, 1);
    }

private:
    template<typename Fun>
    void for_each(const Fun& fun, size_t lock) {
        if (lock < ConcurrencyLevel) {
            std::lock_guard<mutex_type> l(_locks[lock]);
            for_each(fun, lock + 1);
            return;
        }
        for (Bucket& b : _buckets) {
            b.for_each(fun);
        }
    }
    
    void clear(size_t lock) {
        if (lock < ConcurrencyLevel) {
            std::lock_guard<mutex_type> l(_locks[lock]);
            clear(lock + 1);
            return;
        }
        for (Bucket& b : _buckets) {
            b.clear();
        }
    }

    mutex_type & getMutex(size_t hash){
        return _locks[hash % ConcurrencyLevel];
    }

    Bucket & getBucket(size_t hash){
        return _buckets[hash & bucket_flag];
    }

    void resize(size_t new_size, size_t lock_index){
        if (lock_index < ConcurrencyLevel){
            std::lock_guard<mutex_type> l(_locks[lock_index]);
            return resize(new_size, lock_index + 1);
        }
        //everything is locked
        //move old buckets
        std::vector<Bucket, typename allocator_type::template rebind<Bucket>::other> old_buckets(std::move(_buckets));
        bucket_flag = ((size_t)-1) % new_size;
        _upper_bound.store(new_size *  LoadFactor / 100, std::memory_order_release);
        _buckets.resize(new_size);
        for (Bucket & buk : old_buckets){
            bool done = false;
            for (auto & el : buk.arr){
                if (el.state == ElemState::UNASSIGNED){
                    done = true;
                    break;
                }
                getBucket(hash_(el.key)).insertNoDuplicateCheck(std::move(el));
            }
            if (done)
                continue;
            for (auto & el : buk.overflow){
                getBucket(hash_(el.key)).insertNoDuplicateCheck(std::move(el));
            }
        }
    }
};

} // namespace crossbow
