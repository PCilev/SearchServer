#pragma once

#include <cstdlib>
#include <map>
#include <mutex>
#include <string>
#include <vector>
 
#include "log_duration.h"
#include "test_framework.h"
 
using namespace std::string_literals;
 
template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);
 
    struct Bucket {
        std::mutex mut_;
        std::map<Key, Value> bucket_;
    };
    
    struct Access {
        Access(const Key& key, Bucket& bucket) : guard(bucket.mut_), ref_to_value(bucket.bucket_[key]) {}
    
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;
    };
 
    explicit ConcurrentMap(size_t bucket_count) : buckets_(bucket_count) {}
 
    Access operator[](const Key& key) {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        return {key, bucket};
    }
    
    void Erase(const Key& key) {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        bucket.bucket_.erase(key);
    }
 
    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [mut, map] : buckets_) {
            std::lock_guard guard(mut);
            result.insert(map.begin(), map.end());
        }
        return result;
    }
 
private:
    std::vector<Bucket> buckets_;
};