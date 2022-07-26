#ifndef CLIO_SIMPLECACHE_H_INCLUDED
#define CLIO_SIMPLECACHE_H_INCLUDED

#include <ripple/basics/base_uint.h>
#include <ripple/basics/hardened_hash.h>
#include <boost/json.hpp>
#include <backend/Types.h>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

#define JSON_CACHE_SIZE 50000

namespace Backend {
class SimpleCache
{
    struct CacheEntry
    {
        uint32_t seq = 0;
        Blob blob;
    };

    struct JsonCacheEntry
    {
        boost::json::object obj;
        ripple::uint256 key;
        uint32_t seq;
    };

    mutable std::array<JsonCacheEntry, JSON_CACHE_SIZE> jsonCache_;
    mutable std::shared_mutex jsonMtx_;
    std::map<ripple::uint256, CacheEntry> map_;
    mutable std::shared_mutex mtx_;
    uint32_t latestSeq_ = 0;
    std::atomic_bool full_ = false;
    std::atomic_bool disabled_ = false;
    // temporary set to prevent background thread from writing already deleted
    // data. not used when cache is full
    std::unordered_set<ripple::uint256, ripple::hardened_hash<>> deletes_;

public:
    // Update the cache with new ledger objects
    // set isBackground to true when writing old data from a background thread
    void
    update(
        std::vector<LedgerObject> const& blobs,
        uint32_t seq,
        bool isBackground = false);

    std::optional<Blob>
    get(ripple::uint256 const& key, uint32_t seq) const;

    // always returns empty optional if isFull() is false
    std::optional<LedgerObject>
    getSuccessor(ripple::uint256 const& key, uint32_t seq) const;

    // always returns empty optional if isFull() is false
    std::optional<LedgerObject>
    getPredecessor(ripple::uint256 const& key, uint32_t seq) const;

    void
    setDisabled();

    void
    setFull();

    uint32_t
    latestLedgerSequence() const;

    // whether the cache has all data for the most recent ledger
    bool
    isFull() const;

    size_t
    size() const;

    int
    hash(ripple::uint256 const& key) const;

    std::optional<boost::json::object>
    getJson(ripple::uint256 const& key, uint32_t seq) const;

    void
    insertJson(
        boost::json::object const& json,
        ripple::uint256 const& key,
        uint32_t seq) const;
};

}  // namespace Backend
#endif
