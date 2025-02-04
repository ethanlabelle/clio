//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#pragma once

#include <backend/BackendInterface.h>
#include <backend/LedgerCache.h>
#include <etl/LoadBalancer.h>
#include <etl/Source.h>
#include <etl/SystemState.h>
#include <etl/impl/CacheLoader.h>
#include <etl/impl/ExtractionDataPipe.h>
#include <etl/impl/Extractor.h>
#include <etl/impl/LedgerFetcher.h>
#include <etl/impl/LedgerLoader.h>
#include <etl/impl/LedgerPublisher.h>
#include <etl/impl/Transformer.h>
#include <log/Logger.h>
#include <subscriptions/SubscriptionManager.h>

#include "org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h"
#include <grpcpp/grpcpp.h>

#include <memory>

struct AccountTransactionsData;
struct NFTTransactionsData;
struct NFTsData;
class SubscriptionManager;

/**
 * @brief This class is responsible for continuously extracting data from a p2p node, and writing that data to the
 * databases.
 *
 * Usually, multiple different processes share access to the same network accessible databases, in which case only one
 * such process is performing ETL and writing to the database. The other processes simply monitor the database for new
 * ledgers, and publish those ledgers to the various subscription streams. If a monitoring process determines that the
 * ETL writer has failed (no new ledgers written for some time), the process will attempt to become the ETL writer.
 *
 * If there are multiple monitoring processes that try to become the ETL writer at the same time, one will win out, and
 * the others will fall back to monitoring/publishing. In this sense, this class dynamically transitions from monitoring
 * to writing and from writing to monitoring, based on the activity of other processes running on different machines.
 */
class ETLService
{
    // TODO: make these template parameters in ETLService
    using SubscriptionManagerType = SubscriptionManager;
    using LoadBalancerType = LoadBalancer;
    using NetworkValidatedLedgersType = NetworkValidatedLedgers;
    using DataPipeType = clio::detail::ExtractionDataPipe<org::xrpl::rpc::v1::GetLedgerResponse>;
    using CacheLoaderType = clio::detail::CacheLoader<Backend::LedgerCache>;
    using LedgerFetcherType = clio::detail::LedgerFetcher<LoadBalancerType>;
    using ExtractorType = clio::detail::Extractor<DataPipeType, NetworkValidatedLedgersType, LedgerFetcherType>;
    using LedgerLoaderType = clio::detail::LedgerLoader<LoadBalancerType, LedgerFetcherType>;
    using LedgerPublisherType = clio::detail::LedgerPublisher<SubscriptionManagerType>;
    using TransformerType = clio::detail::Transformer<DataPipeType, LedgerLoaderType, LedgerPublisherType>;

    clio::Logger log_{"ETL"};

    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<LoadBalancerType> loadBalancer_;
    std::shared_ptr<NetworkValidatedLedgersType> networkValidatedLedgers_;

    std::uint32_t extractorThreads_ = 1;
    std::thread worker_;

    CacheLoaderType cacheLoader_;
    LedgerFetcherType ledgerFetcher_;
    LedgerLoaderType ledgerLoader_;
    LedgerPublisherType ledgerPublisher_;

    SystemState state_;

    size_t numMarkers_ = 2;
    std::optional<uint32_t> startSequence_;
    std::optional<uint32_t> finishSequence_;
    size_t txnThreshold_ = 0;

public:
    /**
     * @brief Create an instance of ETLService
     *
     * @param config The configuration to use
     * @param ioc io context to run on
     * @param backend BackendInterface implementation
     * @param subscriptions Subscription manager
     * @param balancer Load balancer to use
     * @param ledgers The network validated ledgers datastructure
     */
    ETLService(
        clio::Config const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManagerType> subscriptions,
        std::shared_ptr<LoadBalancerType> balancer,
        std::shared_ptr<NetworkValidatedLedgersType> ledgers);

    static std::shared_ptr<ETLService>
    make_ETLService(
        clio::Config const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<SubscriptionManagerType> subscriptions,
        std::shared_ptr<LoadBalancerType> balancer,
        std::shared_ptr<NetworkValidatedLedgersType> ledgers)
    {
        auto etl = std::make_shared<ETLService>(config, ioc, backend, subscriptions, balancer, ledgers);
        etl->run();

        return etl;
    }

    /**
     * @brief Stops components and joins worker thread
     */
    ~ETLService()
    {
        log_.info() << "onStop called";
        log_.debug() << "Stopping Reporting ETL";

        state_.isStopping = true;
        cacheLoader_.stop();

        if (worker_.joinable())
            worker_.join();

        log_.debug() << "Joined ETLService worker thread";
    }

    /**
     * @brief Get time passed since last ledger close, in seconds
     */
    std::uint32_t
    lastCloseAgeSeconds() const
    {
        return ledgerPublisher_.lastCloseAgeSeconds();
    }

    /**
     * @brief Get state of ETL as a JSON object
     */
    boost::json::object
    getInfo() const
    {
        boost::json::object result;

        result["etl_sources"] = loadBalancer_->toJson();
        result["is_writer"] = state_.isWriting.load();
        result["read_only"] = state_.isReadOnly;
        auto last = ledgerPublisher_.getLastPublish();
        if (last.time_since_epoch().count() != 0)
            result["last_publish_age_seconds"] = std::to_string(ledgerPublisher_.lastPublishAgeSeconds());
        return result;
    }

private:
    /**
     * @brief Run the ETL pipeline.
     *
     * Extracts ledgers and writes them to the database, until a write conflict occurs (or the server shuts down).
     * @note database must already be populated when this function is called
     *
     * @param startSequence the first ledger to extract
     * @return the last ledger written to the database, if any
     */
    std::optional<uint32_t>
    runETLPipeline(uint32_t startSequence, int offset);

    /**
     * @brief Monitor the network for newly validated ledgers.
     *
     * Also monitor the database to see if any process is writing those ledgers.
     * This function is called when the application starts, and will only return when the application is shutting down.
     * If the software detects the database is empty, this function will call loadInitialLedger(). If the software
     * detects ledgers are not being written, this function calls runETLPipeline(). Otherwise, this function publishes
     * ledgers as they are written to the database.
     */
    void
    monitor();

    /**
     * @brief Monitor the database for newly written ledgers.
     *
     * Similar to the monitor(), except this function will never call runETLPipeline() or loadInitialLedger().
     * This function only publishes ledgers as they are written to the database.
     */
    void
    monitorReadOnly();

    /**
     * @return true if stopping; false otherwise
     */
    bool
    isStopping()
    {
        return state_.isStopping;
    }

    /**
     * @brief Get the number of markers to use during the initial ledger download
     *
     * This is equivelent to the degree of parallelism during the initial ledger download.
     *
     * @return the number of markers
     */
    std::uint32_t
    getNumMarkers()
    {
        return numMarkers_;
    }

    /**
     * @brief Start all components to run ETL service
     */
    void
    run();

    /**
     * @brief Spawn the worker thread and start monitoring
     */
    void
    doWork();
};
