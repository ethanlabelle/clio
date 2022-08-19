#include <backend/TxCache.h>
namespace Backend {

uint32_t
TxCache::latestLedgerSequence() const
{
    std::shared_lock lck{mtx_};
    return latestSeq_;
}

// adds an entire ledger's worth of transactions to the cache. Evicts oldest
// ledger.
void
TxCache::update(
    std::vector<ripple::uint256> const& hashes,
    std::vector<TransactionAndMetadata> const& transactions,
    uint32_t seq)
{
    std::unique_lock lck{mtx_};
    if (!cache_.size())
        return;
    cache_[tail_].clear();

    assert(seq == latestSeq_ + 1 || latestSeq_ == 0);
    latestSeq_ = seq;
    for (size_t i = 0; i < hashes.size(); i++)
    {
        cache_[tail_][hashes[i]] = transactions[i];
    }
    tail_ = (tail_ + 1) % cache_.size();
}

std::optional<TransactionAndMetadata>
TxCache::get(ripple::uint256 const& key) const
{
    std::shared_lock lck{mtx_};
    txReqCounter_++;

    for (auto& map : cache_)
    {
        auto e = map.find(key);
        if (e != map.end())
        {
            txHitCounter_++;
            return {e->second};
        }
    }
    return {};
}

float
TxCache::getHitRate() const
{
    if (!txReqCounter_)
        return 0;
    return ((float)txHitCounter_ / txReqCounter_);
}

std::optional<std::vector<TransactionAndMetadata>>
TxCache::getLedgerTransactions(uint32_t const ledgerSequence) const
{
    std::shared_lock lck{mtx_};
    txReqCounter_++;
    auto diff = latestSeq_ - ledgerSequence;
    if (diff < cache_.size())
    {
        auto head = (tail_ - 1) % cache_.size();
        auto ledgerCache = cache_[(head - diff) % cache_.size()];
        std::vector<TransactionAndMetadata> result;
        for (auto const& tx : ledgerCache)
        {
            result.push_back(tx.second);
        }
        if (result.size() > 0)
        {
            txHitCounter_++;
            return {result};
        }
    }
    return {};
}

std::optional<std::vector<ripple::uint256>>
TxCache::getLedgerTransactionHashes(uint32_t const ledgerSequence) const
{
    std::shared_lock lck{mtx_};
    txReqCounter_++;
    auto diff = latestSeq_ - ledgerSequence;
    if (diff < NUM_LEDGERS_CACHED)
    {
        auto head = (tail_ - 1) % cache_.size();
        auto ledgerCache = cache_[(head - diff) % cache_.size()];
        std::vector<ripple::uint256> result;
        for (auto const& tx : ledgerCache)
        {
            result.push_back(tx.first);
        }
        if (result.size() > 0)
        {
            txHitCounter_++;
            return {result};
        }
    }
    return {};
}

size_t
TxCache::size() const
{
    std::shared_lock lck{mtx_};
    return cache_.size();
}

// safe to call multiple times but should not be called multiple times
void
TxCache::setSize(uint32_t size)
{
    std::unique_lock lck{mtx_};
    tail_ = 0;
    cache_.resize(size);
}

}  // namespace Backend
