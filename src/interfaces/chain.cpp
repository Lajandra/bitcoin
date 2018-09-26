// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <interfaces/chain.h>

#include <chain.h>
#include <chainparams.h>
#include <interfaces/handler.h>
#include <net.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <policy/rbf.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <protocol.h>
#include <rpc/protocol.h>
#include <rpc/server.h>
#include <sync.h>
#include <threadsafety.h>
#include <timedata.h>
#include <txmempool.h>
#include <ui_interface.h>
#include <uint256.h>
#include <univalue.h>
#include <util.h>
#include <validation.h>
#include <validationinterface.h>

#include <algorithm>
#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace interfaces {
namespace {

//! Extension of std::reference_wrapper with operator< for use as map key.
template <typename T>
class Ref : public std::reference_wrapper<T>
{
public:
    using std::reference_wrapper<T>::reference_wrapper;
    bool operator<(const Ref<T>& other) const { return this->get() < other.get(); }
};

class LockImpl : public Chain::Lock
{
    Optional<int> getHeight() override
    {
        int height = ::chainActive.Height();
        if (height >= 0) {
            return height;
        }
        return nullopt;
    }
    Optional<int> getBlockHeight(const uint256& hash) override
    {
        auto it = ::mapBlockIndex.find(hash);
        if (it != ::mapBlockIndex.end() && it->second) {
            if (::chainActive.Contains(it->second)) {
                return it->second->nHeight;
            }
        }
        return nullopt;
    }
    int getBlockDepth(const uint256& hash) override
    {
        const Optional<int> tip_height = getHeight();
        const Optional<int> height = getBlockHeight(hash);
        return tip_height && height ? *tip_height - *height + 1 : 0;
    }
    uint256 getBlockHash(int height) override { return ::chainActive[height]->GetBlockHash(); }
    int64_t getBlockTime(int height) override { return ::chainActive[height]->GetBlockTime(); }
    int64_t getBlockMedianTimePast(int height) override { return ::chainActive[height]->GetMedianTimePast(); }
    bool haveBlockOnDisk(int height) override
    {
        CBlockIndex* block = ::chainActive[height];
        return block && ((block->nStatus & BLOCK_HAVE_DATA) != 0) && block->nTx > 0;
    }
    Optional<int> findFirstBlockWithTime(int64_t time) override
    {
        CBlockIndex* block = ::chainActive.FindEarliestAtLeast(time);
        if (block) {
            return block->nHeight;
        }
        return nullopt;
    }
    Optional<int> findFirstBlockWithTimeAndHeight(int64_t time, int height) override
    {
        for (CBlockIndex* block = ::chainActive[height]; block; block = ::chainActive.Next(block)) {
            if (block->GetBlockTime() >= time) {
                return block->nHeight;
            }
        }
        return nullopt;
    }
    Optional<int> findPruned(int start_height, Optional<int> stop_height) override
    {
        if (::fPruneMode) {
            CBlockIndex* block = stop_height ? ::chainActive[*stop_height] : ::chainActive.Tip();
            while (block && block->nHeight >= start_height) {
                if ((block->nStatus & BLOCK_HAVE_DATA) == 0) {
                    return block->nHeight;
                }
                block = block->pprev;
            }
        }
        return nullopt;
    }
    Optional<int> findFork(const uint256& hash, Optional<int>* height) override
    {
        const CBlockIndex *block{nullptr}, *fork{nullptr};
        auto it = ::mapBlockIndex.find(hash);
        if (it != ::mapBlockIndex.end()) {
            block = it->second;
            fork = ::chainActive.FindFork(block);
        }
        if (height != nullptr) {
            if (block) {
                *height = block->nHeight;
            } else {
                height->reset();
            }
        }
        if (fork) {
            return fork->nHeight;
        }
        return nullopt;
    }
    bool isPotentialTip(const uint256& hash) override
    {
        if (::chainActive.Tip()->GetBlockHash() == hash) return true;
        auto it = ::mapBlockIndex.find(hash);
        return it != ::mapBlockIndex.end() && it->second->GetAncestor(::chainActive.Height()) == ::chainActive.Tip();
    }
    CBlockLocator getLocator() override { return ::chainActive.GetLocator(); }
    Optional<int> findLocatorFork(const CBlockLocator& locator) override
    {
        if (CBlockIndex* fork = FindForkInGlobalIndex(::chainActive, locator)) {
            return fork->nHeight;
        }
        return nullopt;
    }
    bool checkFinalTx(const CTransaction& tx) override
    {
        LockAnnotation lock(::cs_main);
        return CheckFinalTx(tx);
    }
    bool acceptToMemoryPool(CTransactionRef tx, CValidationState& state) override
    {
        LockAnnotation lock(::cs_main);
        return AcceptToMemoryPool(::mempool, state, tx, nullptr /* missing inputs */, nullptr /* txn replaced */,
            false /* bypass limits */, ::maxTxFee /* absurd fee */);
    }
};

class LockingStateImpl : public LockImpl, public UniqueLock<CCriticalSection>
{
    using UniqueLock::UniqueLock;
};

class HandlerImpl : public Handler
{
public:
    explicit HandlerImpl(Chain::Notifications& notifications) : m_forwarder(notifications) {}
    ~HandlerImpl() override
    {
        // It is important to disconnect here in the HandlerImpl destructor,
        // instead of in the HandleImpl::Forwarder destructor, because
        // UnregisterAllValidationInterfaces() internally accesses Forwarder
        // virtual methods which are no longer available after Forwarder object
        // has begun to be being destroyed.
        m_forwarder.disconnect();
    }
    void disconnect() override { m_forwarder.disconnect(); }

    struct Forwarder : CValidationInterface
    {
        explicit Forwarder(Chain::Notifications& notifications) : m_notifications(&notifications)
        {
            RegisterValidationInterface(this);
        }
        void disconnect()
        {
            if (m_notifications) {
                m_notifications = nullptr;
                UnregisterValidationInterface(this);
            }
        }
        void TransactionAddedToMempool(const CTransactionRef& tx) override
        {
            m_notifications->TransactionAddedToMempool(tx);
        }
        void TransactionRemovedFromMempool(const CTransactionRef& tx) override
        {
            m_notifications->TransactionRemovedFromMempool(tx);
        }
        void BlockConnected(const std::shared_ptr<const CBlock>& block,
            const CBlockIndex* index,
            const std::vector<CTransactionRef>& tx_conflicted) override
        {
            m_notifications->BlockConnected(*block, index->GetBlockHash(), tx_conflicted);
        }
        void BlockDisconnected(const std::shared_ptr<const CBlock>& block) override
        {
            m_notifications->BlockDisconnected(*block);
        }
        void ChainStateFlushed(const CBlockLocator& locator) override { m_notifications->ChainStateFlushed(locator); }
        void ResendWalletTransactions(int64_t best_block_time, CConnman*) override
        {
            m_notifications->ResendWalletTransactions(best_block_time);
        }

        Chain::Notifications* m_notifications;
    };

    Forwarder m_forwarder;
};

class ChainImpl : public Chain
{
public:
    std::unique_ptr<Chain::Lock> lock(bool try_lock) override
    {
        auto result = MakeUnique<LockingStateImpl>(::cs_main, "cs_main", __FILE__, __LINE__, try_lock);
        if (try_lock && result && !*result) return {};
        // std::move necessary on some compilers due to conversion from
        // LockingStateImpl to Lock pointer
        return std::move(result);
    }
    std::unique_ptr<Chain::Lock> assumeLocked() override { return MakeUnique<LockImpl>(); }
    bool findBlock(const uint256& hash, CBlock* block, int64_t* time, int64_t* time_max) override
    {
        CBlockIndex* index;
        {
            LOCK(cs_main);
            auto it = ::mapBlockIndex.find(hash);
            if (it == ::mapBlockIndex.end()) {
                return false;
            }
            index = it->second;
            if (time) {
                *time = index->GetBlockTime();
            }
            if (time_max) {
                *time_max = index->GetBlockTimeMax();
            }
        }
        if (block && !ReadBlockFromDisk(*block, index, Params().GetConsensus())) {
            block->SetNull();
        }
        return true;
    }
    double guessVerificationProgress(const uint256& block_hash) override
    {
        LOCK(cs_main);
        auto it = ::mapBlockIndex.find(block_hash);
        return GuessVerificationProgress(Params().TxData(), it != ::mapBlockIndex.end() ? it->second : nullptr);
    }
    int64_t getVirtualTransactionSize(const CTransaction& tx) override { return GetVirtualTransactionSize(tx); }
    RBFTransactionState isRBFOptIn(const CTransaction& tx) override
    {
        LOCK(::mempool.cs);
        return IsRBFOptIn(tx, ::mempool);
    }
    bool hasDescendantsInMempool(const uint256& txid) override
    {
        LOCK(::mempool.cs);
        auto it_mp = ::mempool.mapTx.find(txid);
        return it_mp != ::mempool.mapTx.end() && it_mp->GetCountWithDescendants() > 1;
    }
    bool relayTransaction(const uint256& txid) override
    {
        if (g_connman) {
            CInv inv(MSG_TX, txid);
            g_connman->ForEachNode([&inv](CNode* node) { node->PushInventory(inv); });
            return true;
        }
        return false;
    }
    void getTransactionAncestry(const uint256& txid, size_t& ancestors, size_t& descendants) override
    {
        ::mempool.GetTransactionAncestry(txid, ancestors, descendants);
    }
    bool checkChainLimits(CTransactionRef tx) override
    {
        LockPoints lp;
        CTxMemPoolEntry entry(tx, 0, 0, 0, false, 0, lp);
        CTxMemPool::setEntries setAncestors;
        size_t nLimitAncestors = gArgs.GetArg("-limitancestorcount", DEFAULT_ANCESTOR_LIMIT);
        size_t nLimitAncestorSize = gArgs.GetArg("-limitancestorsize", DEFAULT_ANCESTOR_SIZE_LIMIT) * 1000;
        size_t nLimitDescendants = gArgs.GetArg("-limitdescendantcount", DEFAULT_DESCENDANT_LIMIT);
        size_t nLimitDescendantSize = gArgs.GetArg("-limitdescendantsize", DEFAULT_DESCENDANT_SIZE_LIMIT) * 1000;
        std::string errString;
        LOCK(::mempool.cs);
        return ::mempool.CalculateMemPoolAncestors(entry, setAncestors, nLimitAncestors, nLimitAncestorSize,
            nLimitDescendants, nLimitDescendantSize, errString);
    }
    CFeeRate estimateSmartFee(int num_blocks, bool conservative, FeeCalculation* calc) override
    {
        return ::feeEstimator.estimateSmartFee(num_blocks, calc, conservative);
    }
    int estimateMaxBlocks() override { return ::feeEstimator.HighestTargetTracked(FeeEstimateHorizon::LONG_HALFLIFE); }
    CFeeRate poolMinFee() override
    {
        return ::mempool.GetMinFee(gArgs.GetArg("-maxmempool", DEFAULT_MAX_MEMPOOL_SIZE) * 1000000);
    }
    bool getPruneMode() override { return ::fPruneMode; }
    bool p2pEnabled() override { return g_connman != nullptr; }
    int64_t getAdjustedTime() override { return GetAdjustedTime(); }
    void initMessage(const std::string& message) override { ::uiInterface.InitMessage(message); }
    void initWarning(const std::string& message) override { InitWarning(message); }
    void initError(const std::string& message) override { InitError(message); }
    std::unique_ptr<Handler> handleNotifications(Notifications& notifications) override
    {
        return MakeUnique<HandlerImpl>(notifications);
    }
    void waitForNotifications() override { SyncWithValidationInterfaceQueue(); }
    std::unique_ptr<Handler> handleRpc(const CRPCCommand& command) override;

    //! Map of RPC method name to forwarder. If multiple clients provide
    //! implementations of the same RPC method, the RPC forwarder can dispatch
    //! between them.
    class RpcForwarder;
    std::map<Ref<const std::string>, RpcForwarder> m_rpc_forwarders;
};

//! Forwarder for one RPC method.
class ChainImpl::RpcForwarder
{
public:
    explicit RpcForwarder(const CRPCCommand& command) : m_command(command)
    {
        m_command.actor = [this](const JSONRPCRequest& request) { return forwardRequest(request); };
    }

    bool registerForwarder() { return ::tableRPC.appendCommand(m_command.name, &m_command); }

    void addCommand(const CRPCCommand& command) { m_commands.emplace_back(&command); }

    void removeCommand(const CRPCCommand& command)
    {
        m_commands.erase(std::remove(m_commands.begin(), m_commands.end(), &command), m_commands.end());
    }

    UniValue forwardRequest(const JSONRPCRequest& request) const
    {
        // Simple forwarding of RPC requests. This just sends the request to the
        // first client that registered a handler for the RPC method. If the
        // handler throws a wallet not found exception, this will retry
        // forwarding to the next handler (if any).
        //
        // This forwarding mechanism could be made more efficient (peeking
        // inside the RPC request for wallet name and sending it directly to the
        // right handler), but right now all wallets are in-process so there is
        // only ever a single handler, and in the future it isn't clear if we
        // will want we want to keep forwarding RPCs at all (clients could just
        // listen for their own RPCs).
        for (auto it = m_commands.begin(); it != m_commands.end();) {
            const CRPCCommand& command = **it++;
            try {
                return command.actor(request);
            } catch (const UniValue& e) {
                if (it != m_commands.end()) {
                    const UniValue& code = e["code"];
                    if (code.isNum() && code.get_int() == RPC_WALLET_NOT_FOUND) {
                        continue;
                    }
                }
                throw;
            }
        }

        // This will only be reached if m_commands is empty. (Because the RPC
        // server provides an appendCommand, but no removeCommand method, it
        // will keep sending requests here even if there are no clients left to
        // forward to.)
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");
    };

    CRPCCommand m_command;
    std::vector<const CRPCCommand*> m_commands;
};

class RpcHandler : public Handler
{
public:
    RpcHandler(ChainImpl::RpcForwarder& forwarder, const CRPCCommand& command)
        : m_forwarder(&forwarder), m_command(command)
    {
        m_forwarder->addCommand(m_command);
    }

    void disconnect() override final
    {
        if (m_forwarder) {
            m_forwarder->removeCommand(m_command);
            m_forwarder = nullptr;
        }
    }

    ~RpcHandler() override { disconnect(); }

    ChainImpl::RpcForwarder* m_forwarder;
    const CRPCCommand& m_command;
};

std::unique_ptr<Handler> ChainImpl::handleRpc(const CRPCCommand& command)
{
    auto inserted = m_rpc_forwarders.emplace(
        std::piecewise_construct, std::forward_as_tuple(command.name), std::forward_as_tuple(command));
    if (inserted.second && !inserted.first->second.registerForwarder()) {
        m_rpc_forwarders.erase(inserted.first);
        return {};
    }
    return MakeUnique<RpcHandler>(inserted.first->second, command);
}

} // namespace

std::unique_ptr<Chain> MakeChain() { return MakeUnique<ChainImpl>(); }

} // namespace interfaces
