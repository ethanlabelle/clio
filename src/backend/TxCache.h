#ifndef CLIO_TXCACHE_H_INCLUDED
#define CLIO_TXCACHE_H_INCLUDED

#include <ripple/basics/base_uint.h>
#include <ripple/basics/hardened_hash.h>
#include <backend/Types.h>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <utility>
#include <vector>

#define NUM_LEDGERS_CACHED 10

namespace Backend {
class TxCache
{
    mutable std::shared_mutex mtx_;
    uint32_t latestSeq_ = 0;
    bool enabled_;
    size_t numLedgersCached_;
    std::atomic_int tail_;
    mutable std::atomic_int txReqCounter_;
    mutable std::atomic_int txHitCounter_;
    // ring buffer
    // vector of maps of transaction hashes -> TransactionAndMetadata objects
    // replaces the oldest ledger in cache when inserting a new ledger
    std::vector<std::map<ripple::uint256, TransactionAndMetadata>> cache_;

public:
    // Update the cache with new ledger objects
    void
    update(
        std::vector<ripple::uint256> const& hashes,
        std::vector<TransactionAndMetadata> const& transactions,
        uint32_t seq);

    std::optional<TransactionAndMetadata>
    get(ripple::uint256 const& key) const;

    std::optional<std::vector<TransactionAndMetadata>>
    getLedgerTransactions(std::uint32_t const ledgerSequence) const;

    std::optional<std::vector<ripple::uint256>>
    getLedgerTransactionHashes(uint32_t const ledgerSequence) const;

    uint32_t
    latestLedgerSequence() const;

    float
    getHitRate() const;

    size_t
    size() const;

    void
    setSize(std::uint32_t size);
};

}  // namespace Backend
#endif
