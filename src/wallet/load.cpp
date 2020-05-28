// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2020 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/load.h>

#include <interfaces/chain.h>
#include <scheduler.h>
#include <util/string.h>
#include <util/system.h>
#include <util/translation.h>
#include <wallet/context.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>

bool VerifyWallets(WalletContext& context, const std::vector<std::string>& wallet_files)
{
    interfaces::Chain& chain = *context.chain;
    if (gArgs.IsArgSet("-walletdir")) {
        fs::path wallet_dir = gArgs.GetArg("-walletdir", "");
        boost::system::error_code error;
        // The canonical path cleans the path, preventing >1 Berkeley environment instances for the same directory
        fs::path canonical_wallet_dir = fs::canonical(wallet_dir, error);
        if (error || !fs::exists(wallet_dir)) {
            chain.initError(strprintf(_("Specified -walletdir \"%s\" does not exist"), wallet_dir.string()));
            return false;
        } else if (!fs::is_directory(wallet_dir)) {
            chain.initError(strprintf(_("Specified -walletdir \"%s\" is not a directory"), wallet_dir.string()));
            return false;
        // The canonical path transforms relative paths into absolute ones, so we check the non-canonical version
        } else if (!wallet_dir.is_absolute()) {
            chain.initError(strprintf(_("Specified -walletdir \"%s\" is a relative path"), wallet_dir.string()));
            return false;
        }
        gArgs.ForceSetArg("-walletdir", canonical_wallet_dir.string());
    }

    LogPrintf("Using wallet directory %s\n", GetWalletDir().string());

    chain.initMessage(_("Verifying wallet(s)...").translated);

    // Keep track of each wallet absolute path to detect duplicates.
    std::set<fs::path> wallet_paths;

    for (const auto& wallet_file : wallet_files) {
        WalletLocation location(wallet_file);

        if (!wallet_paths.insert(location.GetPath()).second) {
            chain.initError(strprintf(_("Error loading wallet %s. Duplicate -wallet filename specified."), wallet_file));
            return false;
        }

        bilingual_str error_string;
        std::vector<bilingual_str> warnings;
        bool verify_success = CWallet::Verify(context, location, error_string, warnings);
        if (!warnings.empty()) chain.initWarning(Join(warnings, Untranslated("\n")));
        if (!verify_success) {
            chain.initError(error_string);
            return false;
        }
    }

    return true;
}

bool LoadWallets(WalletContext& context, const std::vector<std::string>& wallet_files)
{
    interfaces::Chain& chain = *context.chain;
    try {
        for (const std::string& walletFile : wallet_files) {
            bilingual_str error;
            std::vector<bilingual_str> warnings;
            std::shared_ptr<CWallet> pwallet = CWallet::CreateWalletFromFile(context, WalletLocation(walletFile), error, warnings);
            if (!warnings.empty()) chain.initWarning(Join(warnings, Untranslated("\n")));
            if (!pwallet) {
                chain.initError(error);
                return false;
            }
            AddWallet(context, pwallet);
        }
        return true;
    } catch (const std::runtime_error& e) {
        chain.initError(Untranslated(e.what()));
        return false;
    }
}

void StartWallets(WalletContext& context, CScheduler& scheduler)
{
    for (const std::shared_ptr<CWallet>& pwallet : GetWallets(context)) {
        pwallet->postInitProcess();
    }

    // Schedule periodic wallet flushes and tx rebroadcasts
    if (context.args->GetBoolArg("-flushwallet", DEFAULT_FLUSHWALLET)) {
        scheduler.scheduleEvery([&context] { MaybeCompactWalletDB(context); }, std::chrono::milliseconds{500});
    }
    scheduler.scheduleEvery([&context] { MaybeResendWalletTxs(context); }, std::chrono::milliseconds{1000});
}

void FlushWallets(WalletContext& context)
{
<<<<<<< HEAD
    for (const std::shared_ptr<CWallet>& pwallet : GetWallets()) {
        pwallet->Flush();
||||||| merged common ancestors
    for (const std::shared_ptr<CWallet>& pwallet : GetWallets()) {
        pwallet->Flush(false);
=======
    for (const std::shared_ptr<CWallet>& pwallet : GetWallets(context)) {
        pwallet->Flush(false);
>>>>>>> refactor: remove ::vpwallets and related global variables
    }
}

void StopWallets(WalletContext& context)
{
<<<<<<< HEAD
    for (const std::shared_ptr<CWallet>& pwallet : GetWallets()) {
        pwallet->Close();
||||||| merged common ancestors
    for (const std::shared_ptr<CWallet>& pwallet : GetWallets()) {
        pwallet->Flush(true);
=======
    for (const std::shared_ptr<CWallet>& pwallet : GetWallets(context)) {
        pwallet->Flush(true);
>>>>>>> refactor: remove ::vpwallets and related global variables
    }
}

void UnloadWallets(WalletContext& context)
{
    auto wallets = GetWallets(context);
    while (!wallets.empty()) {
        auto wallet = wallets.back();
        wallets.pop_back();
        RemoveWallet(context, wallet);
        UnloadWallet(std::move(wallet));
    }
}
