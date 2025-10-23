#include <iostream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <mutex>
#include <optional>
#include <string>

// ---------------- Cache Entry Struct ----------------
struct CacheEntry {
    std::string value;
    std::chrono::steady_clock::time_point expiry;

    bool isExpired() const {
        return std::chrono::steady_clock::now() > expiry;
    }
};

// ---------------- Distributed Cache (Simulated Shared Cache) ----------------
class DistributedCache {
    static std::mutex mtx;
    static std::unordered_map<std::string, CacheEntry> cache;

public:
    static std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cache.find(key);
        if (it != cache.end() && !it->second.isExpired())
            return it->second.value;
        return std::nullopt;
    }

    static void set(const std::string& key, const std::string& value, int ttlSec) {
        std::lock_guard<std::mutex> lock(mtx);
        cache[key] = {value, std::chrono::steady_clock::now() + std::chrono::seconds(ttlSec)};
    }
};

std::mutex DistributedCache::mtx;
std::unordered_map<std::string, CacheEntry> DistributedCache::cache;

// ---------------- Local Cache (Per Application Instance) ----------------
class LocalCache {
    std::unordered_map<std::string, CacheEntry> cache;
    std::mutex mtx;

public:
    std::optional<std::string> get(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = cache.find(key);
        if (it != cache.end() && !it->second.isExpired())
            return it->second.value;
        return std::nullopt;
    }

    void set(const std::string& key, const std::string& value, int ttlSec) {
        std::lock_guard<std::mutex> lock(mtx);
        cache[key] = {value, std::chrono::steady_clock::now() + std::chrono::seconds(ttlSec)};
    }
};

// ---------------- Two-Tier Cache ----------------
class TwoTierCache {
    LocalCache localCache;
    int ttl = 5; // default 5 seconds

    // Simulated database fetch
    std::string loadFromDB(const std::string& key) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // simulate delay
        return "DB_VALUE_" + key;
    }

public:
    std::string get(const std::string& key) {
        // Step 1: Check local cache
        auto localVal = localCache.get(key);
        if (localVal.has_value()) {
            std::cout << "[Local Hit] ";
            return localVal.value();
        }

        // Step 2: Check distributed cache
        auto distVal = DistributedCache::get(key);
        if (distVal.has_value()) {
            std::cout << "[Distributed Hit] ";
            localCache.set(key, distVal.value(), ttl);
            return distVal.value();
        }

        // Step 3: Miss — Load from DB
        std::cout << "[DB Fetch] ";
        std::string value = loadFromDB(key);

        // Update both caches
        DistributedCache::set(key, value, ttl);
        localCache.set(key, value, ttl);

        return value;
    }
};

// ---------------- Demo ----------------
int main() {
    TwoTierCache cache;

    // First fetch (DB hit)
    std::cout << "Value: " << cache.get("user42") << "\n";

    // Second fetch (local hit)
    std::cout << "Value: " << cache.get("user42") << "\n";

    // Wait to let TTL expire (simulate cache expiry)
    std::cout << "Sleeping 6 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(6));

    // Third fetch (DB hit again after TTL expiry)
    std::cout << "Value: " << cache.get("user42") << "\n";

    return 0;
}
