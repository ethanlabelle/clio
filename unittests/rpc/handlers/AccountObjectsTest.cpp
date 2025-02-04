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

#include <rpc/common/AnyHandler.h>
#include <rpc/handlers/AccountObjects.h>
#include <util/Fixtures.h>
#include <util/TestObject.h>

#include <fmt/core.h>

using namespace RPC;
namespace json = boost::json;
using namespace testing;

constexpr static auto ACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr static auto ISSUER = "rsA2LpzuawewSBQXkiju3YQTMzW13pAAdW";
constexpr static auto ACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr static auto LEDGERHASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr static auto INDEX1 = "1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC";
constexpr static auto TXNID = "E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879";
constexpr static auto MAXSEQ = 30;
constexpr static auto MINSEQ = 10;

class RPCAccountObjectsHandlerTest : public HandlerBaseTest
{
};

struct AccountObjectsParamTestCaseBundle
{
    std::string testName;
    std::string testJson;
    std::string expectedError;
    std::string expectedErrorMessage;
};

// parameterized test cases for parameters check
struct AccountObjectsParameterTest : public RPCAccountObjectsHandlerTest,
                                     public WithParamInterface<AccountObjectsParamTestCaseBundle>
{
    struct NameGenerator
    {
        template <class ParamType>
        std::string
        operator()(const testing::TestParamInfo<ParamType>& info) const
        {
            auto bundle = static_cast<AccountObjectsParamTestCaseBundle>(info.param);
            return bundle.testName;
        }
    };
};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<AccountObjectsParamTestCaseBundle>{
        AccountObjectsParamTestCaseBundle{
            "MissingAccount", R"({})", "invalidParams", "Required field 'account' missing"},
        AccountObjectsParamTestCaseBundle{"AccountNotString", R"({"account":1})", "invalidParams", "accountNotString"},
        AccountObjectsParamTestCaseBundle{"AccountInvalid", R"({"account":"xxx"})", "actMalformed", "accountMalformed"},
        AccountObjectsParamTestCaseBundle{
            "TypeNotString",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "type":1})",
            "invalidParams",
            "Invalid parameters."},
        AccountObjectsParamTestCaseBundle{
            "TypeInvalid",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "type":"wrong"})",
            "invalidParams",
            "Invalid parameters."},
        AccountObjectsParamTestCaseBundle{
            "LedgerHashInvalid",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_hash":"1"})",
            "invalidParams",
            "ledger_hashMalformed"},
        AccountObjectsParamTestCaseBundle{
            "LedgerHashNotString",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_hash":1})",
            "invalidParams",
            "ledger_hashNotString"},
        AccountObjectsParamTestCaseBundle{
            "LedgerIndexInvalid",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "ledger_index":"a"})",
            "invalidParams",
            "ledgerIndexMalformed"},
        AccountObjectsParamTestCaseBundle{
            "LimitNotInt",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "limit":"1"})",
            "invalidParams",
            "Invalid parameters."},
        AccountObjectsParamTestCaseBundle{
            "LimitLargerThanMax",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "limit":401})",
            "invalidParams",
            "Invalid parameters."},
        AccountObjectsParamTestCaseBundle{
            "LimitLessThanMin",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "limit":9})",
            "invalidParams",
            "Invalid parameters."},
        AccountObjectsParamTestCaseBundle{
            "MarkerNotString",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "marker":9})",
            "invalidParams",
            "markerNotString"},
        AccountObjectsParamTestCaseBundle{
            "MarkerInvalid",
            R"({"account":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun", "marker":"xxxx"})",
            "invalidParams",
            "Malformed cursor"},
    };
}

INSTANTIATE_TEST_CASE_P(
    RPCAccountObjectsGroup1,
    AccountObjectsParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    AccountObjectsParameterTest::NameGenerator{});

TEST_P(AccountObjectsParameterTest, InvalidParams)
{
    auto const testBundle = GetParam();
    runSpawn([&, this](auto& yield) {
        auto const handler = AnyHandler{AccountObjectsHandler{mockBackendPtr}};
        auto const req = json::parse(testBundle.testJson);
        auto const output = handler.process(req, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), testBundle.expectedError);
        EXPECT_EQ(err.at("error_message").as_string(), testBundle.expectedErrorMessage);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, LedgerNonExistViaIntSequence)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(MAXSEQ, _))
        .WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":30
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountObjectsHandler{mockBackendPtr}};
    runSpawn([&](auto& yield) {
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, LedgerNonExistViaStringSequence)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence(MAXSEQ, _)).WillByDefault(Return(std::nullopt));

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_index":"30"
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountObjectsHandler{mockBackendPtr}};
    runSpawn([&](auto& yield) {
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, LedgerNonExistViaHash)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    EXPECT_CALL(*rawBackendPtr, fetchLedgerByHash).Times(1);
    // return empty ledgerinfo
    ON_CALL(*rawBackendPtr, fetchLedgerByHash(ripple::uint256{LEDGERHASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerInfo>{}));

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "ledger_hash":"{}"
        }})",
        ACCOUNT,
        LEDGERHASH));
    auto const handler = AnyHandler{AccountObjectsHandler{mockBackendPtr}};
    runSpawn([&](auto& yield) {
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, AccountNotExsit)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);

    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject).WillByDefault(Return(std::optional<Blob>{}));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT));
    auto const handler = AnyHandler{AccountObjectsHandler{mockBackendPtr}};
    runSpawn([&](auto& yield) {
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_FALSE(output);
        auto const err = RPC::makeError(output.error());
        EXPECT_EQ(err.at("error").as_string(), "actNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "accountNotFound");
    });
}

TEST_F(RPCAccountObjectsHandlerTest, DefaultParameter)
{
    static auto constexpr expectedOut = R"({
                                            "ledger_hash":"4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652",
                                            "ledger_index":30,
                                            "validated":true,
                                            "account": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                            "limit": 200,
                                            "account_objects":[
                                                {
                                                    "Balance":{
                                                        "currency":"USD",
                                                        "issuer":"rsA2LpzuawewSBQXkiju3YQTMzW13pAAdW",
                                                        "value":"100"
                                                    },
                                                    "Flags":0,
                                                    "HighLimit":{
                                                        "currency":"USD",
                                                        "issuer":"rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                                        "value":"20"
                                                    },
                                                    "LedgerEntryType":"RippleState",
                                                    "LowLimit":{
                                                        "currency":"USD",
                                                        "issuer":"rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                                                        "value":"10"
                                                    },
                                                    "PreviousTxnID":"E3FE6EA3D48F0C2B639448020EA4F03D4F4F8FFDB243A852A0F59177921B4879",
                                                    "PreviousTxnLgrSeq":123,
                                                    "index":"1B8590C01B0006EDFA9ED60296DD052DC5E90F99659B25014D08E1BC983515BC"
                                                }
                                            ]
                                        })";

    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));

    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);
    std::vector<Blob> bbs;
    auto const line1 =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    bbs.push_back(line1.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}"
        }})",
        ACCOUNT));

    auto const handler = AnyHandler{AccountObjectsHandler{mockBackendPtr}};
    runSpawn([&](auto& yield) {
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(*output, json::parse(expectedOut));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, Limit)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));

    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    static auto constexpr limit = 10;
    auto count = limit * 2;
    // put 20 items in owner dir, but only return 10
    auto const ownerDir = CreateOwnerDirLedgerObject(std::vector(count, ripple::uint256{INDEX1}), INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    while (count-- != 0)
    {
        auto const line1 =
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
        bbs.push_back(line1.getSerializer().peekData());
    }
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        ACCOUNT,
        limit));

    auto const handler = AnyHandler{AccountObjectsHandler{mockBackendPtr}};
    runSpawn([&](auto& yield) {
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), limit);
        EXPECT_EQ(output->as_object().at("marker").as_string(), fmt::format("{},{}", INDEX1, 0));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, Marker)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));

    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    static auto constexpr limit = 20;
    static auto constexpr page = 2;
    auto count = limit;
    auto const ownerDir = CreateOwnerDirLedgerObject(std::vector(count, ripple::uint256{INDEX1}), INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    auto const hintIndex = ripple::keylet::page(ownerDirKk, page).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(hintIndex, 30, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(3);

    std::vector<Blob> bbs;
    while (count-- != 0)
    {
        auto const line1 =
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
        bbs.push_back(line1.getSerializer().peekData());
    }
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "marker":"{},{}"
        }})",
        ACCOUNT,
        INDEX1,
        page));

    auto const handler = AnyHandler{AccountObjectsHandler{mockBackendPtr}};
    runSpawn([&](auto& yield) {
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), limit - 1);
        EXPECT_FALSE(output->as_object().contains("marker"));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, MultipleDir)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));

    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    static auto constexpr count = 10;
    static auto constexpr nextpage = 1;
    auto cc = count;
    auto ownerDir = CreateOwnerDirLedgerObject(std::vector(cc, ripple::uint256{INDEX1}), INDEX1);
    // set next page
    ownerDir.setFieldU64(ripple::sfIndexNext, nextpage);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    auto const page1 = ripple::keylet::page(ownerDirKk, nextpage).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(page1, 30, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(3);

    std::vector<Blob> bbs;
    // 10 items per page, 2 pages
    cc = count * 2;
    while (cc-- != 0)
    {
        auto const line1 =
            CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
        bbs.push_back(line1.getSerializer().peekData());
    }
    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "limit":{}
        }})",
        ACCOUNT,
        2 * count));

    auto const handler = AnyHandler{AccountObjectsHandler{mockBackendPtr}};
    runSpawn([&](auto& yield) {
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), count * 2);
        EXPECT_EQ(output->as_object().at("marker").as_string(), fmt::format("{},{}", INDEX1, nextpage));
    });
}

TEST_F(RPCAccountObjectsHandlerTest, TypeFilter)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));

    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;
    // put 1 state and 1 offer
    auto const line1 =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    auto const offer = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1);
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "type":"offer"
        }})",
        ACCOUNT));

    auto const handler = AnyHandler{AccountObjectsHandler{mockBackendPtr}};
    runSpawn([&](auto& yield) {
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), 1);
    });
}

TEST_F(RPCAccountObjectsHandlerTest, TypeFilterReturnEmpty)
{
    auto const rawBackendPtr = static_cast<MockBackend*>(mockBackendPtr.get());
    mockBackendPtr->updateRange(MINSEQ);  // min
    mockBackendPtr->updateRange(MAXSEQ);  // max
    auto const ledgerinfo = CreateLedgerInfo(LEDGERHASH, MAXSEQ);
    EXPECT_CALL(*rawBackendPtr, fetchLedgerBySequence).Times(1);
    ON_CALL(*rawBackendPtr, fetchLedgerBySequence).WillByDefault(Return(ledgerinfo));

    auto const accountKk = ripple::keylet::account(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(accountKk, MAXSEQ, _)).WillByDefault(Return(Blob{'f', 'a', 'k', 'e'}));

    auto const ownerDir = CreateOwnerDirLedgerObject({ripple::uint256{INDEX1}, ripple::uint256{INDEX1}}, INDEX1);
    auto const ownerDirKk = ripple::keylet::ownerDir(GetAccountIDWithString(ACCOUNT)).key;
    ON_CALL(*rawBackendPtr, doFetchLedgerObject(ownerDirKk, 30, _))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObject).Times(2);
    std::vector<Blob> bbs;
    auto const line1 =
        CreateRippleStateLedgerObject(ACCOUNT, "USD", ISSUER, 100, ACCOUNT, 10, ACCOUNT2, 20, TXNID, 123, 0);
    auto const offer = CreateOfferLedgerObject(
        ACCOUNT,
        10,
        20,
        ripple::to_string(ripple::to_currency("USD")),
        ripple::to_string(ripple::xrpCurrency()),
        ACCOUNT2,
        toBase58(ripple::xrpAccount()),
        INDEX1);
    bbs.push_back(line1.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());

    ON_CALL(*rawBackendPtr, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*rawBackendPtr, doFetchLedgerObjects).Times(1);

    auto const static input = boost::json::parse(fmt::format(
        R"({{
            "account":"{}",
            "type": "check"
        }})",
        ACCOUNT));

    auto const handler = AnyHandler{AccountObjectsHandler{mockBackendPtr}};
    runSpawn([&](auto& yield) {
        auto const output = handler.process(input, Context{std::ref(yield)});
        ASSERT_TRUE(output);
        EXPECT_EQ(output->as_object().at("account_objects").as_array().size(), 0);
    });
}
