#include <unordered_map>
#include <mutex>
#include <chrono>

// Optimization: Cache virtual to physical translations for the target process.
// Physical addresses do change over time (swapping, etc.), but for ESP running at 60fps,
// caching for a few seconds is usually safe and greatly reduces IOCTLs / page walks.

struct CachedTranslation {
    uint64_t PhysicalAddress;
    uint32_t TimestampMs;
};

class TLBCache {
public:
    uint64_t Lookup(uint64_t DTB, uint64_t VirtualPage, bool& OutFound) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        auto it = m_Cache.find({DTB, VirtualPage});
        if (it != m_Cache.end()) {
            uint32_t now = GetTickCount();
            // Cache valid for 2000ms (2 seconds)
            if (now - it->second.TimestampMs < 2000) {
                OutFound = true;
                return it->second.PhysicalAddress;
            } else {
                // Expired
                m_Cache.erase(it);
            }
        }
        OutFound = false;
        return 0;
    }

    void Insert(uint64_t DTB, uint64_t VirtualPage, uint64_t PhysicalPage) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        CachedTranslation entry = { PhysicalPage, GetTickCount() };
        m_Cache[{DTB, VirtualPage}] = entry;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Cache.clear();
    }

private:
    struct std_hash {
        size_t operator()(const std::pair<uint64_t, uint64_t>& p) const {
            return std::hash<uint64_t>()(p.first) ^ (std::hash<uint64_t>()(p.second) << 1);
        }
    };
    std::unordered_map<std::pair<uint64_t, uint64_t>, CachedTranslation, std_hash> m_Cache;
    std::mutex m_Mutex;
};

TLBCache g_Cache;
