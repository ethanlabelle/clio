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

#include <etl/LoadBalancer.h>
#include <etl/Source.h>
#include <log/Logger.h>
#include <rpc/Counters.h>
#include <rpc/RPCHelpers.h>
#include <rpc/common/Types.h>
#include <webserver/Context.h>

#include <memory>
#include <string>

namespace RPC::detail {

template <typename LoadBalancerType, typename CountersType, typename HandlerProviderType>
class ForwardingProxy
{
    clio::Logger log_{"RPC"};

    std::shared_ptr<LoadBalancerType> balancer_;
    std::reference_wrapper<CountersType> counters_;
    std::shared_ptr<HandlerProviderType const> handlerProvider_;

public:
    ForwardingProxy(
        std::shared_ptr<LoadBalancerType> const& balancer,
        CountersType& counters,
        std::shared_ptr<HandlerProviderType const> const& handlerProvider)
        : balancer_{balancer}, counters_{std::ref(counters)}, handlerProvider_{handlerProvider}
    {
    }

    bool
    shouldForward(Web::Context const& ctx) const
    {
        if (ctx.method == "subscribe" || ctx.method == "unsubscribe")
            return false;

        // TODO: if needed, make configurable with json config option
        if (ctx.apiVersion == 1)
            return true;

        if (handlerProvider_->isClioOnly(ctx.method))
            return false;

        if (isProxied(ctx.method))
            return true;

        auto const& request = ctx.params;

        if (specifiesCurrentOrClosedLedger(request))
            return true;

        if (ctx.method == "account_info" && request.contains("queue") && request.at("queue").is_bool() &&
            request.at("queue").as_bool())
            return true;

        return false;
    }

    Result
    forward(Web::Context const& ctx)
    {
        auto toForward = ctx.params;
        toForward["command"] = ctx.method;

        if (auto const res = balancer_->forwardToRippled(toForward, ctx.clientIp, ctx.yield); not res)
        {
            notifyFailedToForward(ctx.method);
            return Status{RippledError::rpcFAILED_TO_FORWARD};
        }
        else
        {
            notifyForwarded(ctx.method);
            return *res;
        }
    }

    bool
    isProxied(std::string const& method) const
    {
        static std::unordered_set<std::string> const proxiedCommands{
            "submit",
            "submit_multisigned",
            "fee",
            "ledger_closed",
            "ledger_current",
            "ripple_path_find",
            "manifest",
            "channel_authorize",
            "channel_verify",
        };

        return proxiedCommands.contains(method);
    }

private:
    void
    notifyForwarded(std::string const& method)
    {
        if (validHandler(method))
            counters_.get().rpcForwarded(method);
    }

    void
    notifyFailedToForward(std::string const& method)
    {
        if (validHandler(method))
            counters_.get().rpcFailedToForward(method);
    }

    bool
    validHandler(std::string const& method) const
    {
        return handlerProvider_->contains(method) || isProxied(method);
    }
};

}  // namespace RPC::detail
