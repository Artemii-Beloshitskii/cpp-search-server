#pragma once

#include <algorithm>
#include <execution>
#include <future>
#include <iostream>
#include <iterator>
#include <list>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace std::string_literals;


template <typename Key, typename Value>
class ConcurrentMap {
private:
    struct Bucket {
        std::mutex mutex;
        std::map<Key, Value> map;
    };

public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        std::lock_guard<std::mutex> guard;
        Value& ref_to_value;

        Access(const Key& key, Bucket& bucket)
            : guard(bucket.mutex)
            , ref_to_value(bucket.map[key]) {
        }
    };

    explicit ConcurrentMap(size_t bucket_count)
        : buckets_(bucket_count) {
    }

    Access operator[](const Key& key) {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        return { key, bucket };
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> result;
        for (auto& [mutex, map] : buckets_) {
            std::lock_guard g(mutex);
            result.insert(map.begin(), map.end());
        }
        return result;
    }

    void Erase(const Key& key) {
        auto& bucket = buckets_[static_cast<uint64_t>(key) % buckets_.size()];
        std::lock_guard guard(bucket.mutex);
        bucket.map.erase(key);
    }

private:
    std::vector<Bucket> buckets_;
};
