// Copyright (c) 2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/chainstate.h>

#include <consensus/params.h>
#include <node/blockstorage.h>
#include <node/caches.h>
#include <validation.h>

InitResult LoadChainstate(
    ChainstateManager& chainman,
    const Consensus::Params& consensus_params,
    const CacheSizes& cache_sizes,
    const InitOptions& options)
{
    auto is_coinsview_empty = [&](CChainState* chainstate) EXCLUSIVE_LOCKS_REQUIRED(::cs_main) {
        return options.reset || options.reindex || chainstate->CoinsTip().GetBestBlock().IsNull();
    };

    {
        LOCK(cs_main);
        chainman.InitializeChainstate(options.mempool);
        chainman.m_total_coinstip_cache = cache_sizes.coins;
        chainman.m_total_coinsdb_cache = cache_sizes.coins_db;

        UnloadBlockIndex(options.mempool, chainman);

        auto& pblocktree{chainman.m_blockman.m_block_tree_db};
        // new CBlockTreeDB tries to delete the existing file, which
        // fails if it's still open from the previous loop. Close it first:
        pblocktree.reset();
        pblocktree.reset(new CBlockTreeDB(cache_sizes.block_tree_db, options.block_tree_db_in_memory, options.reset));

        if (options.reset) {
            pblocktree->WriteReindexing(true);
            //If we're reindexing in prune mode, wipe away unusable block files and all undo data files
            if (fPruneMode)
                CleanupBlockRevFiles();
        }

        if (options.check_interrupt && options.check_interrupt()) return {InitStatus::INTERRUPTED, {}};

        // LoadBlockIndex will load fHavePruned if we've ever removed a
        // block file from disk.
        // Note that it also sets fReindex based on the disk flag!
        // From here on out fReindex and fReset mean something different!
        if (!chainman.LoadBlockIndex()) {
            if (options.check_interrupt && options.check_interrupt()) return {InitStatus::INTERRUPTED, {}};
            return {InitStatus::FAILURE, _("Error loading block database")};
        }

        if (!chainman.BlockIndex().empty() &&
                !chainman.m_blockman.LookupBlockIndex(consensus_params.hashGenesisBlock)) {
            // If the loaded chain has a wrong genesis, bail out immediately
            // (we're likely using a testnet datadir, or the other way around).
            return {InitStatus::FAILURE, _("Incorrect or no genesis block found. Wrong datadir for network?")};
        }

        // Check for changed -prune state.  What we are concerned about is a user who has pruned blocks
        // in the past, but is now trying to run unpruned.
        if (fHavePruned && !fPruneMode) {
            return {InitStatus::FAILURE, _("You need to rebuild the database using -reindex to go back to unpruned mode.  This will redownload the entire blockchain")};
        }

        // At this point blocktree args are consistent with what's on disk.
        // If we're not mid-reindex (based on disk + args), add a genesis block on disk
        // (otherwise we use the one already on disk).
        // This is called again in ThreadImport after the reindex completes.
        if (!fReindex && !chainman.ActiveChainstate().LoadGenesisBlock()) {
            return {InitStatus::FAILURE, _("Error initializing block database")};
        }

        // At this point we're either in reindex or we've loaded a useful
        // block tree into BlockIndex()!

        for (CChainState* chainstate : chainman.GetAll()) {
            chainstate->InitCoinsDB(
                /* cache_size_bytes */ cache_sizes.coins_db,
                /* in_memory */ options.coins_db_in_memory,
                /* should_wipe */ options.reset || options.reindex);

            if (options.coins_error_cb) {
                chainstate->CoinsErrorCatcher().AddReadErrCallback(options.coins_error_cb);
            }

            // If necessary, upgrade from older database format.
            // This is a no-op if we cleared the coinsviewdb with -reindex or -reindex-chainstate
            if (!chainstate->CoinsDB().Upgrade()) {
                return {InitStatus::FAILURE, _("Error upgrading chainstate database")};
            }

            // ReplayBlocks is a no-op if we cleared the coinsviewdb with -reindex or -reindex-chainstate
            if (!chainstate->ReplayBlocks()) {
                return {InitStatus::FAILURE, _("Unable to replay blocks. You will need to rebuild the database using -reindex-chainstate.")};
            }

            // The on-disk coinsdb is now in a good state, create the cache
            chainstate->InitCoinsCache(cache_sizes.coins);
            assert(chainstate->CanFlushToDisk());

            if (!is_coinsview_empty(chainstate)) {
                // LoadChainTip initializes the chain based on CoinsTip()'s best block
                if (!chainstate->LoadChainTip()) {
                    return {InitStatus::FAILURE, _("Error initializing block database")};
                }
                assert(chainstate->m_chain.Tip() != nullptr);
            }
        }

        if (!options.reset) {
            auto chainstates{chainman.GetAll()};
            if (std::any_of(chainstates.begin(), chainstates.end(),
                            [](const CChainState* cs) EXCLUSIVE_LOCKS_REQUIRED(cs_main) { return cs->NeedsRedownload(); })) {
                return {InitStatus::FAILURE, strprintf(_("Witness data for blocks after height %d requires validation. Please restart with -reindex."),
                                                       consensus_params.SegwitHeight)};
            }
        }
    }

    return {InitStatus::SUCCESS, {}};
}

InitResult VerifyLoadedChainstate(
    ChainstateManager& chainman,
    const Consensus::Params& consensus_params,
    const InitOptions& options)
{
    auto is_coinsview_empty = [&](CChainState* chainstate) EXCLUSIVE_LOCKS_REQUIRED(::cs_main) {
        return options.reset || options.reindex || chainstate->CoinsTip().GetBestBlock().IsNull();
    };

    {
        LOCK(cs_main);

        for (CChainState* chainstate : chainman.GetAll()) {
            if (!is_coinsview_empty(chainstate)) {
                const CBlockIndex* tip = chainstate->m_chain.Tip();
                if (tip && tip->nTime > options.get_unix_time_seconds() + 2 * 60 * 60) {
                    return {InitStatus::FAILURE, _("The block database contains a block which appears to be from the future. "
                                                   "This may be due to your computer's date and time being set incorrectly. "
                                                   "Only rebuild the block database if you are sure that your computer's date and time are correct")};
                }

                if (!CVerifyDB().VerifyDB(
                        *chainstate, consensus_params, chainstate->CoinsDB(),
                        options.check_level,
                        options.check_blocks)) {
                    return {InitStatus::FAILURE, _("Corrupted block database detected")};
                }
            }
        }
    }

    return {InitStatus::SUCCESS, {}};
}
