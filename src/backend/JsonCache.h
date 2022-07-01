#ifndef CLIO_JSONCACHE_H_INCLUDED
#define CLIO_JSONCACHE_H_INCLUDED

#include <ripple/basics/base_uint.h>
#include <ripple/basics/hardened_hash.h>
#include <gtl/lru_cache.hpp>
#include <boost/json.hpp>

template <>
struct std::hash<ripple::uint256> {
    size_t operator()(const ripple::uint256 &key) const {
        return ripple::hardened_hash<>()(key);
    }
};

namespace Backend {
class JsonCache
{
    struct CacheEntry
    {
        uint32_t seq = 0;
        boost::json::object obj;
    };
    mutable std::shared_mutex mtx_;
    gtl::lru_cache<ripple::uint256, CacheEntry> map_;

public:
    std::optional<boost::json::object>
    get(ripple::uint256 const& key, uint32_t seq);

    void
    insert(
        ripple::uint256 const& key,
        boost::json::object const& value,
        uint32_t seq);
};
}  // namespace Backend
#endif
