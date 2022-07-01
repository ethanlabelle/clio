#include <backend/JsonCache.h>
namespace Backend {

std::optional<boost::json::object>
JsonCache::get(ripple::uint256 const& key, uint32_t seq)
{
    std::shared_lock lk(mtx_);
    auto cached = map_.get(key);
    if (cached == nullptr || cached->seq != seq)
    {
        return {};
    }
    else
    {
        return {cached->obj};
    }
}

void
JsonCache::insert(
    ripple::uint256 const& key,
    boost::json::object const& value,
    uint32_t seq)
{
    std::unique_lock lk(mtx_);
    CacheEntry e = {seq, value};
    map_.insert(key, e);
}

}  // namespace Backend
