// Copyright (c) 2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <interfaces/chain.h>
#include <node/context.h>
#include <test/util/mining.h>
#include <test/util/setup_common.h>
#include <wallet/test/util.h>
#include <util/translation.h>
#include <validationinterface.h>
#include <wallet/context.h>
#include <wallet/receive.h>
#include <wallet/wallet.h>

#include <optional>

<<<<<<< HEAD
namespace wallet{
||||||| parent of f7d4451b9801 (refactor: Use util::Result class for wallet loading)
using wallet::CWallet;
using wallet::DatabaseFormat;
using wallet::DatabaseOptions;
using wallet::TxStateInactive;
using wallet::WALLET_FLAG_DESCRIPTORS;
using wallet::WalletContext;
using wallet::WalletDatabase;

static std::shared_ptr<CWallet> BenchLoadWallet(std::unique_ptr<WalletDatabase> database, WalletContext& context, DatabaseOptions& options)
{
    bilingual_str error;
    std::vector<bilingual_str> warnings;
    auto wallet = CWallet::Create(context, "", std::move(database), options.create_flags, error, warnings);
    NotifyWalletLoaded(context, wallet);
    if (context.chain) {
        wallet->postInitProcess();
    }
    return wallet;
}

static void BenchUnloadWallet(std::shared_ptr<CWallet>&& wallet)
{
    SyncWithValidationInterfaceQueue();
    wallet->m_chain_notifications_handler.reset();
    UnloadWallet(std::move(wallet));
}

=======
using wallet::CWallet;
using wallet::DatabaseFormat;
using wallet::DatabaseOptions;
using wallet::TxStateInactive;
using wallet::WALLET_FLAG_DESCRIPTORS;
using wallet::WalletContext;
using wallet::WalletDatabase;

static std::shared_ptr<CWallet> BenchLoadWallet(std::unique_ptr<WalletDatabase> database, WalletContext& context, DatabaseOptions& options)
{
    bilingual_str error;
    std::vector<bilingual_str> warnings;
    auto wallet = CWallet::Create(context, "", std::move(database), options.create_flags);
    NotifyWalletLoaded(context, *wallet);
    if (context.chain) {
        (*wallet)->postInitProcess();
    }
    return *wallet;
}

static void BenchUnloadWallet(std::shared_ptr<CWallet>&& wallet)
{
    SyncWithValidationInterfaceQueue();
    wallet->m_chain_notifications_handler.reset();
    UnloadWallet(std::move(wallet));
}

>>>>>>> f7d4451b9801 (refactor: Use util::Result class for wallet loading)
static void AddTx(CWallet& wallet)
{
    CMutableTransaction mtx;
    mtx.vout.push_back({COIN, GetScriptForDestination(*Assert(wallet.GetNewDestination(OutputType::BECH32, "")))});
    mtx.vin.push_back(CTxIn());

    wallet.AddToWallet(MakeTransactionRef(mtx), TxStateInactive{});
}

static void WalletLoading(benchmark::Bench& bench, bool legacy_wallet)
{
    const auto test_setup = MakeNoLogFileContext<TestingSetup>();

    WalletContext context;
    context.args = &test_setup->m_args;
    context.chain = test_setup->m_node.chain.get();

    // Setup the wallet
    // Loading the wallet will also create it
    uint64_t create_flags = 0;
    if (!legacy_wallet) {
        create_flags = WALLET_FLAG_DESCRIPTORS;
    }
    auto database = CreateMockableWalletDatabase();
    auto wallet = TestLoadWallet(std::move(database), context, create_flags);

    // Generate a bunch of transactions and addresses to put into the wallet
    for (int i = 0; i < 1000; ++i) {
        AddTx(*wallet);
    }

    database = DuplicateMockDatabase(wallet->GetDatabase());

    // reload the wallet for the actual benchmark
    TestUnloadWallet(std::move(wallet));

    bench.epochs(5).run([&] {
        wallet = TestLoadWallet(std::move(database), context, create_flags);

        // Cleanup
        database = DuplicateMockDatabase(wallet->GetDatabase());
        TestUnloadWallet(std::move(wallet));
    });
}

#ifdef USE_BDB
static void WalletLoadingLegacy(benchmark::Bench& bench) { WalletLoading(bench, /*legacy_wallet=*/true); }
BENCHMARK(WalletLoadingLegacy, benchmark::PriorityLevel::HIGH);
#endif

#ifdef USE_SQLITE
static void WalletLoadingDescriptors(benchmark::Bench& bench) { WalletLoading(bench, /*legacy_wallet=*/false); }
BENCHMARK(WalletLoadingDescriptors, benchmark::PriorityLevel::HIGH);
#endif
} // namespace wallet
