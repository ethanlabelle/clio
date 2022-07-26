#include <backend/SimpleCache.h>
namespace Backend {

uint32_t
SimpleCache::latestLedgerSequence() const
{
    std::shared_lock lck{mtx_};
    return latestSeq_;
}

void
SimpleCache::update(
    std::vector<LedgerObject> const& objs,
    uint32_t seq,
    bool isBackground)
{
    if (disabled_)
        return;

    {
        std::unique_lock lck{mtx_};
        if (seq > latestSeq_)
        {
            assert(seq == latestSeq_ + 1 || latestSeq_ == 0);
            latestSeq_ = seq;
        }
        for (auto const& obj : objs)
        {
            if (obj.blob.size())
            {
                if (isBackground && deletes_.count(obj.key))
                    continue;

                auto& e = map_[obj.key];
                if (seq > e.seq)
                {
                    e = {seq, obj.blob};
                }
            }
            else
            {
                map_.erase(obj.key);
                if (!full_ && !isBackground)
                    deletes_.insert(obj.key);
            }
        }
    }
}

std::optional<LedgerObject>
SimpleCache::getSuccessor(ripple::uint256 const& key, uint32_t seq) const
{
    if (!full_)
        return {};
    std::shared_lock{mtx_};
    if (seq != latestSeq_)
        return {};
    auto e = map_.upper_bound(key);
    if (e == map_.end())
        return {};
    return {{e->first, e->second.blob}};
}

std::optional<LedgerObject>
SimpleCache::getPredecessor(ripple::uint256 const& key, uint32_t seq) const
{
    if (!full_)
        return {};
    std::shared_lock lck{mtx_};
    if (seq != latestSeq_)
        return {};
    auto e = map_.lower_bound(key);
    if (e == map_.begin())
        return {};
    --e;
    return {{e->first, e->second.blob}};
}
std::optional<Blob>
SimpleCache::get(ripple::uint256 const& key, uint32_t seq) const
{
    if (seq > latestSeq_)
        return {};
    std::shared_lock lck{mtx_};
    auto e = map_.find(key);
    if (e == map_.end())
        return {};
    if (seq < e->second.seq)
        return {};
    return {e->second.blob};
}

void
SimpleCache::setDisabled()
{
    disabled_ = true;
}

void
SimpleCache::setFull()
{
    if (disabled_)
        return;

    full_ = true;
    std::unique_lock lck{mtx_};
    deletes_.clear();
}

bool
SimpleCache::isFull() const
{
    return full_;
}
size_t
SimpleCache::size() const
{
    std::shared_lock lck{mtx_};
    return map_.size();
}

// todo- probing, fine tune cache size
int
SimpleCache::hash(ripple::uint256 const& key) const
{
    auto total = 0;
    for (auto word : key)
    {
        total ^= word;
    }
    return total % JSON_CACHE_SIZE;
}

std::optional<boost::json::object>
SimpleCache::getJson(ripple::uint256 const& key, uint32_t seq) const
{
    std::shared_lock lck(jsonMtx_);
    auto obj = jsonCache_[hash(key)];
    if (obj.key == key && obj.seq == seq)
        return {obj.obj};
    return {};
}

void
SimpleCache::insertJson(
    boost::json::object const& json,
    ripple::uint256 const& key,
    uint32_t seq) const
{
    std::unique_lock lck(jsonMtx_);
    jsonCache_[hash(key)] = {json, key, seq};
}

float
SimpleCache::jsonHitRate() const
{
    if (!jsonCounter_)
        return 1;
    return ((float)jsonHitCounter_/jsonCounter_);
}
}  // namespace Backend
