#include "flux/cache/object_cache.hpp"

namespace flux {

ObjectCache::ObjectCache(size_t capacity)
    : cache_(capacity) {}

std::optional<Bytes> ObjectCache::get(const ObjectId& id) {
    auto result = cache_.get(id);
    if (result) {
        hits_++;
    } else {
        misses_++;
    }
    return result;
}

void ObjectCache::put(const ObjectId& id, Bytes data) {
    cache_.put(id, std::move(data));
}

void ObjectCache::clear() {
    cache_.clear();
    hits_ = 0;
    misses_ = 0;
}

ObjectCache::Stats ObjectCache::get_stats() const {
    Stats stats{};
    stats.hits = hits_.load();
    stats.misses = misses_.load();
    stats.size = cache_.size();
    
    size_t total = stats.hits + stats.misses;
    if (total > 0) {
        stats.hit_rate = static_cast<double>(stats.hits) / total;
    } else {
        stats.hit_rate = 0.0;
    }
    
    return stats;
}

} // namespace flux
