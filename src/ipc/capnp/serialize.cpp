#include "ipc/capnp/serialize.h"

#include "clientversion.h"
#include "key.h"
#include "net.h"
#include "net_processing.h"
#include "netbase.h"
#include "wallet/coincontrol.h"

#include <univalue.h>

namespace ipc {
namespace capnp {

void BuildProxy(messages::Proxy::Builder&& builder, const proxyType& proxy)
{
    builder.setProxy(ToArray(Serialize(proxy.proxy)));
    builder.setRandomizeCredentials(proxy.randomize_credentials);
}

void ReadProxy(proxyType& proxy, const messages::Proxy::Reader& reader)
{
    Unserialize(proxy.proxy, reader.getProxy());
    proxy.randomize_credentials = reader.getRandomizeCredentials();
}

void BuildNodeStats(messages::NodeStats::Builder&& builder, const CNodeStats& node_stats)
{
    builder.setNodeid(node_stats.nodeid);
    builder.setServices(node_stats.nServices);
    builder.setRelayTxes(node_stats.fRelayTxes);
    builder.setLastSend(node_stats.nLastSend);
    builder.setLastRecv(node_stats.nLastRecv);
    builder.setTimeConnected(node_stats.nTimeConnected);
    builder.setTimeOffset(node_stats.nTimeOffset);
    builder.setAddrName(node_stats.addrName);
    builder.setVersion(node_stats.nVersion);
    builder.setCleanSubVer(node_stats.cleanSubVer);
    builder.setInbound(node_stats.fInbound);
    builder.setAddnode(node_stats.fAddnode);
    builder.setStartingHeight(node_stats.nStartingHeight);
    builder.setSendBytes(node_stats.nSendBytes);
    auto builder_send = builder.initSendBytesPerMsgCmd().initEntries(node_stats.mapSendBytesPerMsgCmd.size());
    size_t i = 0;
    for (const auto& entry : node_stats.mapSendBytesPerMsgCmd) {
        builder_send[i].setKey(entry.first);
        builder_send[i].setValue(entry.second);
        ++i;
    }
    builder.setRecvBytes(node_stats.nRecvBytes);
    auto builder_recv = builder.initRecvBytesPerMsgCmd().initEntries(node_stats.mapRecvBytesPerMsgCmd.size());
    i = 0;
    for (const auto& entry : node_stats.mapRecvBytesPerMsgCmd) {
        builder_recv[i].setKey(entry.first);
        builder_recv[i].setValue(entry.second);
        ++i;
    }
    builder.setWhitelisted(node_stats.fWhitelisted);
    builder.setPingTime(node_stats.dPingTime);
    builder.setPingWait(node_stats.dPingWait);
    builder.setMinPing(node_stats.dMinPing);
    builder.setAddrLocal(node_stats.addrLocal);
    builder.setAddr(ToArray(Serialize(node_stats.addr)));
    builder.setAddrBind(ToArray(Serialize(node_stats.addrBind)));
}

void ReadNodeStats(CNodeStats& node_stats, const messages::NodeStats::Reader& reader)
{
    node_stats.nodeid = NodeId(reader.getNodeid());
    node_stats.nServices = ServiceFlags(reader.getServices());
    node_stats.fRelayTxes = reader.getRelayTxes();
    node_stats.nLastSend = reader.getLastSend();
    node_stats.nLastRecv = reader.getLastRecv();
    node_stats.nTimeConnected = reader.getTimeConnected();
    node_stats.nTimeOffset = reader.getTimeOffset();
    node_stats.addrName = ToString(reader.getAddrName());
    node_stats.nVersion = reader.getVersion();
    node_stats.cleanSubVer = ToString(reader.getCleanSubVer());
    node_stats.fInbound = reader.getInbound();
    node_stats.fAddnode = reader.getAddnode();
    node_stats.nStartingHeight = reader.getStartingHeight();
    node_stats.nSendBytes = reader.getSendBytes();
    for (const auto& entry : reader.getSendBytesPerMsgCmd().getEntries()) {
        node_stats.mapSendBytesPerMsgCmd[ToString(entry.getKey())] = entry.getValue();
    }
    node_stats.nRecvBytes = reader.getRecvBytes();
    for (const auto& entry : reader.getRecvBytesPerMsgCmd().getEntries()) {
        node_stats.mapRecvBytesPerMsgCmd[ToString(entry.getKey())] = entry.getValue();
    }
    node_stats.fWhitelisted = reader.getWhitelisted();
    node_stats.dPingTime = reader.getPingTime();
    node_stats.dPingWait = reader.getPingWait();
    node_stats.dMinPing = reader.getMinPing();
    node_stats.addrLocal = ToString(reader.getAddrLocal());
    Unserialize(node_stats.addr, reader.getAddr());
    Unserialize(node_stats.addrBind, reader.getAddrBind());
}

void BuildNodeStateStats(messages::NodeStateStats::Builder&& builder, const CNodeStateStats& node_state_stats)
{
    builder.setMisbehavior(node_state_stats.nMisbehavior);
    builder.setSyncHeight(node_state_stats.nSyncHeight);
    builder.setCommonHeight(node_state_stats.nCommonHeight);
    auto heights = builder.initHeightInFlight(node_state_stats.vHeightInFlight.size());
    size_t i = 0;
    for (int height : node_state_stats.vHeightInFlight) {
        heights.set(i, height);
        ++i;
    }
}

void ReadNodeStateStats(CNodeStateStats& node_state_stats, const messages::NodeStateStats::Reader& reader)
{
    node_state_stats.nMisbehavior = reader.getMisbehavior();
    node_state_stats.nSyncHeight = reader.getSyncHeight();
    node_state_stats.nCommonHeight = reader.getCommonHeight();
    for (int height : reader.getHeightInFlight()) {
        node_state_stats.vHeightInFlight.emplace_back(height);
    }
}

void BuildBanmap(messages::Banmap::Builder&& builder, const banmap_t& banmap)
{
    auto builder_entries = builder.initEntries(banmap.size());
    size_t i = 0;
    for (const auto& entry : banmap) {
        builder_entries[i].setSubnet(ToArray(Serialize(entry.first)));
        builder_entries[i].setBanEntry(ToArray(Serialize(entry.first)));
        ++i;
    }
}
void ReadBanmap(banmap_t& banmap, const messages::Banmap::Reader& reader)
{
    for (const auto& entry : reader.getEntries()) {
        banmap.emplace(Unserialize<CSubNet>(entry.getSubnet()), Unserialize<CBanEntry>(entry.getBanEntry()));
    }
}

void BuildUniValue(messages::UniValue::Builder&& builder, const UniValue& univalue)
{
    builder.setType(univalue.getType());
    if (univalue.getType() == UniValue::VARR || univalue.getType() == UniValue::VOBJ) {
        builder.setValue(univalue.write());
    } else {
        builder.setValue(univalue.getValStr());
    }
}

void ReadUniValue(UniValue& univalue, const messages::UniValue::Reader& reader)
{
    if (reader.getType() == UniValue::VARR || reader.getType() == UniValue::VOBJ) {
        if (!univalue.read(ToString(reader.getValue()))) {
            throw std::runtime_error("Could not parse UniValue");
        }
    } else {
        univalue = UniValue(UniValue::VType(reader.getType()), ToString(reader.getValue()));
    }
}

void BuildWalletValueMap(messages::WalletValueMap::Builder&& builder, const WalletValueMap& value_map)
{
    auto builder_entries = builder.initEntries(value_map.size());
    size_t i = 0;
    for (const auto& entry : value_map) {
        builder_entries[i].setKey(entry.first);
        builder_entries[i].setValue(entry.second);
        ++i;
    }
}

void ReadWalletValueMap(WalletValueMap& value_map, const messages::WalletValueMap::Reader& reader)
{
    value_map.clear();
    for (const auto& entry : reader.getEntries()) {
        value_map.emplace(ToString(entry.getKey()), ToString(entry.getValue()));
    }
}

void BuildWalletOrderForm(messages::WalletOrderForm::Builder&& builder, const WalletOrderForm& order_form)
{
    auto builder_entries = builder.initEntries(order_form.size());
    size_t i = 0;
    for (const auto& entry : order_form) {
        builder_entries[i].setKey(entry.first);
        builder_entries[i].setValue(entry.second);
        ++i;
    }
}

void ReadWalletOrderForm(WalletOrderForm& order_form, const messages::WalletOrderForm::Reader& reader)
{
    order_form.clear();
    for (const auto& entry : reader.getEntries()) {
        order_form.emplace_back(ToString(entry.getKey()), ToString(entry.getValue()));
    }
}

void BuildTxDestination(messages::TxDestination::Builder&& builder, const CTxDestination& dest)
{
    if (const CKeyID* keyId = boost::get<CKeyID>(&dest)) {
        builder.setKeyId(ToArray(Serialize(*keyId)));
    } else if (const CScriptID* scriptId = boost::get<CScriptID>(&dest)) {
        builder.setScriptId(ToArray(Serialize(*scriptId)));
    }
}

void ReadTxDestination(CTxDestination& dest, const messages::TxDestination::Reader& reader)
{
    if (reader.hasKeyId()) {
        dest = Unserialize<CKeyID>(reader.getKeyId());
    } else if (reader.hasScriptId()) {
        dest = Unserialize<CScriptID>(reader.getScriptId());
    }
}

void BuildKey(messages::Key::Builder&& builder, const CKey& key)
{
    builder.setSecret(FromBlob(key));
    builder.setIsCompressed(key.IsCompressed());
}

void ReadKey(CKey& key, const messages::Key::Reader& reader)
{
    auto secret = reader.getSecret();
    key.Set(secret.begin(), secret.end(), reader.getIsCompressed());
}

void BuildCoinControl(messages::CoinControl::Builder&& builder, const CCoinControl& coin_control)
{
    BuildTxDestination(builder.initDestChange(), coin_control.destChange);
    builder.setAllowOtherInputs(coin_control.fAllowOtherInputs);
    builder.setAllowWatchOnly(coin_control.fAllowWatchOnly);
    builder.setOverrideFeeRate(coin_control.fOverrideFeeRate);
    if (coin_control.m_feerate) {
        builder.setFeeRate(ToArray(Serialize(*coin_control.m_feerate)));
    }
    if (coin_control.m_confirm_target) {
        builder.setHasConfirmTarget(true);
        builder.setConfirmTarget(*coin_control.m_confirm_target);
    }
    builder.setSignalRbf(coin_control.signalRbf);
    builder.setFeeMode(int32_t(coin_control.m_fee_mode));
    std::vector<COutPoint> selected;
    coin_control.ListSelected(selected);
    auto builder_selected = builder.initSetSelected(selected.size());
    size_t i = 0;
    for (const COutPoint& output : selected) {
        builder_selected.set(i, ToArray(Serialize(output)));
        ++i;
    }
}

void ReadCoinControl(CCoinControl& coin_control, const messages::CoinControl::Reader& reader)
{
    ReadTxDestination(coin_control.destChange, reader.getDestChange());
    coin_control.fAllowOtherInputs = reader.getAllowOtherInputs();
    coin_control.fAllowWatchOnly = reader.getAllowWatchOnly();
    coin_control.fOverrideFeeRate = reader.getOverrideFeeRate();
    if (reader.hasFeeRate()) {
        coin_control.m_feerate = Unserialize<CFeeRate>(reader.getFeeRate());
    }
    if (reader.getHasConfirmTarget()) {
        coin_control.m_confirm_target = reader.getConfirmTarget();
    }
    coin_control.signalRbf = reader.getSignalRbf();
    coin_control.m_fee_mode = FeeEstimateMode(reader.getFeeMode());
    for (const auto output : reader.getSetSelected()) {
        coin_control.Select(Unserialize<COutPoint>(output));
    }
}

void BuildCoinsList(messages::CoinsList::Builder&& builder, const Wallet::CoinsList& coins_list)
{
    auto builder_entries = builder.initEntries(coins_list.size());
    size_t i = 0;
    for (const auto& entry : coins_list) {
        BuildTxDestination(builder_entries[i].initDest(), entry.first);
        auto builder_coins = builder_entries[i].initCoins(entry.second.size());
        size_t j = 0;
        for (const auto& coin : entry.second) {
            builder_coins[i].setOutput(ToArray(Serialize(std::get<0>(coin))));
            BuildWalletTxOut(builder_coins[i].initTxout(), std::get<1>(coin));
            ++j;
        }
        ++i;
    }
}

void ReadCoinsList(Wallet::CoinsList& coins_list, const messages::CoinsList::Reader& reader)
{
    coins_list.clear();
    for (const auto& entry : reader.getEntries()) {
        CTxDestination dest;
        ReadTxDestination(dest, entry.getDest());
        auto& coins = coins_list[dest];
        coins.reserve(entry.getCoins().size());
        for (const auto& coin : entry.getCoins()) {
            coins.emplace_back();
            Unserialize(std::get<0>(coins.back()), coin.getOutput());
            ReadWalletTxOut(std::get<1>(coins.back()), coin.getTxout());
        }
    }
}

void BuildRecipient(messages::Recipient::Builder&& builder, const CRecipient& recipient)
{
    builder.setScriptPubKey(ToArray(recipient.scriptPubKey));
    builder.setAmount(recipient.nAmount);
    builder.setSubtractFeeFromAmount(recipient.fSubtractFeeFromAmount);
}

void ReadRecipient(CRecipient& recipient, const messages::Recipient::Reader& reader)
{
    recipient.scriptPubKey = ToBlob<CScript>(reader.getScriptPubKey());
    recipient.nAmount = reader.getAmount();
    recipient.fSubtractFeeFromAmount = reader.getSubtractFeeFromAmount();
}

void BuildWalletAddress(messages::WalletAddress::Builder&& builder, const WalletAddress& address)
{
    BuildTxDestination(builder.initDest(), address.dest);
    builder.setIsMine(address.is_mine);
    builder.setName(address.name);
    builder.setPurpose(address.purpose);
}

void ReadWalletAddress(WalletAddress& address, const messages::WalletAddress::Reader& reader)
{
    ReadTxDestination(address.dest, reader.getDest());
    address.is_mine = isminetype(reader.getIsMine());
    address.name = reader.getName();
    address.purpose = reader.getPurpose();
}

void BuildWalletBalances(messages::WalletBalances::Builder&& builder, const WalletBalances& balances)
{
    builder.setBalance(balances.balance);
    builder.setUnconfirmedBalance(balances.unconfirmed_balance);
    builder.setImmatureBalance(balances.immature_balance);
    builder.setHaveWatchOnly(balances.have_watch_only);
    builder.setWatchOnlyBalance(balances.watch_only_balance);
    builder.setUnconfirmedWatchOnlyBalance(balances.unconfirmed_watch_only_balance);
    builder.setImmatureWatchOnlyBalance(balances.immature_watch_only_balance);
}

void ReadWalletBalances(WalletBalances& balances, const messages::WalletBalances::Reader& reader)
{
    balances.balance = reader.getBalance();
    balances.unconfirmed_balance = reader.getUnconfirmedBalance();
    balances.immature_balance = reader.getImmatureBalance();
    balances.have_watch_only = reader.getHaveWatchOnly();
    balances.watch_only_balance = reader.getWatchOnlyBalance();
    balances.unconfirmed_watch_only_balance = reader.getUnconfirmedWatchOnlyBalance();
    balances.immature_watch_only_balance = reader.getImmatureWatchOnlyBalance();
}

void BuildWalletTx(messages::WalletTx::Builder&& builder, const WalletTx& wallet_tx)
{
    if (wallet_tx.tx) {
        builder.setTx(ToArray(Serialize(*wallet_tx.tx)));
    }

    size_t i = 0;
    auto builder_txin_is_mine = builder.initTxinIsMine(wallet_tx.txin_is_mine.size());
    for (const auto& ismine : wallet_tx.txin_is_mine) {
        builder_txin_is_mine.set(i, ismine);
        ++i;
    }

    i = 0;
    auto builder_txout_is_mine = builder.initTxoutIsMine(wallet_tx.txout_is_mine.size());
    for (const auto& ismine : wallet_tx.txout_is_mine) {
        builder_txout_is_mine.set(i, ismine);
        ++i;
    }

    i = 0;
    auto builder_txout_address = builder.initTxoutAddress(wallet_tx.txout_address.size());
    for (const auto& address : wallet_tx.txout_address) {
        BuildTxDestination(builder_txout_address[i], address);
        ++i;
    }

    i = 0;
    auto builder_txout_address_is_mine = builder.initTxoutAddressIsMine(wallet_tx.txout_address_is_mine.size());
    for (const auto& ismine : wallet_tx.txout_address_is_mine) {
        builder_txout_address_is_mine.set(i, ismine);
        ++i;
    }

    builder.setCredit(wallet_tx.credit);
    builder.setDebit(wallet_tx.debit);
    builder.setChange(wallet_tx.change);
    builder.setTime(wallet_tx.time);
    BuildWalletValueMap(builder.initValueMap(), wallet_tx.value_map);
    builder.setIsCoinBase(wallet_tx.is_coinbase);
}

void ReadWalletTx(WalletTx& wallet_tx, const messages::WalletTx::Reader& reader)
{
    if (reader.hasTx()) {
        // TODO dedup
        auto data = reader.getTx();
        CDataStream stream(reinterpret_cast<const char*>(data.begin()), reinterpret_cast<const char*>(data.end()),
            SER_NETWORK, CLIENT_VERSION);
        wallet_tx.tx = std::make_shared<CTransaction>(deserialize, stream);
    }

    wallet_tx.txin_is_mine.clear();
    wallet_tx.txin_is_mine.reserve(reader.getTxinIsMine().size());
    for (const auto& ismine : reader.getTxinIsMine()) {
        wallet_tx.txin_is_mine.emplace_back(isminetype(ismine));
    }

    wallet_tx.txout_is_mine.clear();
    wallet_tx.txout_is_mine.reserve(reader.getTxoutIsMine().size());
    for (const auto& ismine : reader.getTxoutIsMine()) {
        wallet_tx.txout_is_mine.emplace_back(isminetype(ismine));
    }

    wallet_tx.txout_address.clear();
    wallet_tx.txout_address.reserve(reader.getTxoutAddress().size());
    for (const auto& address : reader.getTxoutAddress()) {
        wallet_tx.txout_address.emplace_back();
        ReadTxDestination(wallet_tx.txout_address.back(), address);
    }

    wallet_tx.txout_address_is_mine.clear();
    wallet_tx.txout_address_is_mine.reserve(reader.getTxoutAddressIsMine().size());
    for (const auto& ismine : reader.getTxoutAddressIsMine()) {
        wallet_tx.txout_address_is_mine.emplace_back(isminetype(ismine));
    }

    wallet_tx.credit = reader.getCredit();
    wallet_tx.debit = reader.getDebit();
    wallet_tx.change = reader.getChange();
    wallet_tx.time = reader.getTime();
    ReadWalletValueMap(wallet_tx.value_map, reader.getValueMap());
    wallet_tx.is_coinbase = reader.getIsCoinBase();
}

void BuildWalletTxOut(messages::WalletTxOut::Builder&& builder, const WalletTxOut& txout)
{
    builder.setTxout(ToArray(Serialize(txout.txout)));
    builder.setTime(txout.time);
    builder.setDepthInMainChain(txout.depth_in_main_chain);
    builder.setIsSpent(txout.is_spent);
}

void ReadWalletTxOut(WalletTxOut& txout, const messages::WalletTxOut::Reader& reader)
{
    Unserialize(txout.txout, reader.getTxout());
    txout.time = reader.getTime();
    txout.depth_in_main_chain = reader.getDepthInMainChain();
    txout.is_spent = reader.getIsSpent();
}

void BuildWalletTxStatus(messages::WalletTxStatus::Builder&& builder, const WalletTxStatus& status)
{
    builder.setBlockHeight(status.block_height);
    builder.setBlocksToMaturity(status.blocks_to_maturity);
    builder.setDepthInMainChain(status.depth_in_main_chain);
    builder.setRequestCount(status.request_count);
    builder.setTimeReceived(status.time_received);
    builder.setLockTime(status.lock_time);
    builder.setIsFinal(status.is_final);
    builder.setIsTrusted(status.is_trusted);
    builder.setIsAbandoned(status.is_abandoned);
    builder.setIsCoinBase(status.is_coinbase);
    builder.setIsInMainChain(status.is_in_main_chain);
}

void ReadWalletTxStatus(WalletTxStatus& status, const messages::WalletTxStatus::Reader& reader)
{
    status.block_height = reader.getBlockHeight();
    status.blocks_to_maturity = reader.getBlocksToMaturity();
    status.depth_in_main_chain = reader.getDepthInMainChain();
    status.request_count = reader.getRequestCount();
    status.time_received = reader.getTimeReceived();
    status.lock_time = reader.getLockTime();
    status.is_final = reader.getIsFinal();
    status.is_trusted = reader.getIsTrusted();
    status.is_abandoned = reader.getIsAbandoned();
    status.is_coinbase = reader.getIsCoinBase();
    status.is_in_main_chain = reader.getIsInMainChain();
}

} // namespace capnp
} // namespace ipc
