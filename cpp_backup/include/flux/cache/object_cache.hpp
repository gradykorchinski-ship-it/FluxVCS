#pragma once

#include "flux/core/types.hpp"
#include "flux/core/object_id.hpp"
#include <memory>
#include <mutex>
#include <list>
#include <unordered_map>
#include <atomic>

namespace flux {

/**
 * LRU cache for objects
 */
template<typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : capacity_(capacity) {}
    
    std::optional<V> get(const K& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it == cache_.end()) {
            return std::nullopt;
        }
        
        // Move to front (most recently used)
        access_list_.splice(access_list_.begin(), access_list_, it->second.list_it);
        
        return it->second.value;
    }
    
    void put(const K& key, V value) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            // Update existing
            it->second.value = std::move(value);
            access_list_.splice(access_list_.begin(), access_list_, it->second.list_it);
            return;
        }
        
        // Add new
        if (cache_.size() >= capacity_) {
            // Evict least recently used
            auto lru_key = access_list_.back();
            access_list_.pop_back();
            cache_.erase(lru_key);
        }
        
        access_list_.push_front(key);
        cache_[key] = CacheEntry{std::move(value), access_list_.begin()};
    }
    
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.clear();
        access_list_.clear();
    }
    
    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }
    
private:
    struct CacheEntry {
        V value;
        typename std::list<K>::iterator list_it;
    };
    
    size_t capacity_;
    std::list<K> access_list_;
    std::unordered_map<K, CacheEntry> cache_;
    mutable std::mutex mutex_;
};

/**
 * Object cache for repository
 */
class ObjectCache {
public:
    explicit ObjectCache(size_t capacity = 1000);
    
    std::optional<Bytes> get(const ObjectId& id);
    void put(const ObjectId& id, Bytes data);
    void clear();
    
    struct Stats {
        size_t hits;
        size_t misses;
        size_t size;
        double hit_rate;
    };
    
    Stats get_stats() const;
    
private:
    LRUCache<ObjectId, Bytes> cache_;
    mutable std::atomic<size_t> hits_{0};
    mutable std::atomic<size_t> misses_{0};
};

} // namespace flux
