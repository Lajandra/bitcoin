# Copyright (c) 2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

@0x92546c47dc734b2e;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interfaces::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
using Common = import "common.capnp";
using Wallet = import "wallet.capnp";
using Handler = import "handler.capnp";

interface Node $Proxy.wrap("interfaces::Node") {
    destroy @0 (context :Proxy.Context) -> ();
    initError @1 (context :Proxy.Context, message :Text) -> ();
    customSetupServerArgs @2 (context :Proxy.Context) -> () $Proxy.name("setupServerArgs");
    customParseParameters @3 (context :Proxy.Context, argv :List(Text) $Proxy.count(2)) -> (errorStr :Text, result :Bool) $Proxy.name("parseParameters");
    customForceSetArg @4 (context :Proxy.Context, arg :Text, value :Text) -> () $Proxy.name("forceSetArg");
    customSoftSetArg @5 (context :Proxy.Context, arg :Text, value :Text) -> (result :Bool) $Proxy.name("softSetArg");
    customSoftSetBoolArg @6 (context :Proxy.Context, arg :Text, value :Bool) -> (result :Bool) $Proxy.name("softSetBoolArg");
    customReadConfigFiles @7 (context :Proxy.Context) -> (errorStr :Text, result: Bool) $Proxy.name("readConfigFiles");
    customSelectParams @8 (context :Proxy.Context, network :Text) -> (error :Text $Proxy.exception("std::exception")) $Proxy.name("selectParams");
    customBaseInitialize @9 (context :Proxy.Context) -> (error :Text $Proxy.exception("std::exception"), result :Bool) $Proxy.name("baseInitialize");
    getAssumedBlockchainSize @10 (context :Proxy.Context) -> (result :UInt64);
    getAssumedChainStateSize @11 (context :Proxy.Context) -> (result :UInt64);
    getNetwork @12 (context :Proxy.Context) -> (result :Text);
    initLogging @13 (context :Proxy.Context) -> ();
    initParameterInteraction @14 (context :Proxy.Context) -> ();
    getWarnings @15 (context :Proxy.Context, type :Text) -> (result :Text);
    getLogCategories @16 (context :Proxy.Context) -> (result :UInt32);
    appInitMain @17 (context :Proxy.Context) -> (error :Text $Proxy.exception("std::exception"), result :Bool);
    appShutdown @18 (context :Proxy.Context) -> ();
    startShutdown @19 (context :Proxy.Context) -> ();
    shutdownRequested @20 (context :Proxy.Context) -> (result :Bool);
    mapPort @21 (context :Proxy.Context, useUPnP :Bool) -> ();
    getProxy @22 (context :Proxy.Context, net :Int32) -> (proxyInfo :ProxyType, result :Bool);
    getNodeCount @23 (context :Proxy.Context, flags :Int32) -> (result :UInt64);
    getNodesStats @24 (context :Proxy.Context) -> (stats :List(NodeStats), result :Bool);
    getBanned @25 (context :Proxy.Context) -> (banmap :List(Common.Pair(Data, Data)), result :Bool);
    ban @26 (context :Proxy.Context, netAddr :Data, reason :Int32, banTimeOffset :Int64) -> (result :Bool);
    unban @27 (context :Proxy.Context, ip :Data) -> (result :Bool);
    disconnectByAddress @28 (context :Proxy.Context, address :Data) -> (result :Bool);
    disconnectById @29 (context :Proxy.Context, id :Int64) -> (result :Bool);
    getTotalBytesRecv @30 (context :Proxy.Context) -> (result :Int64);
    getTotalBytesSent @31 (context :Proxy.Context) -> (result :Int64);
    getMempoolSize @32 (context :Proxy.Context) -> (result :UInt64);
    getMempoolDynamicUsage @33 (context :Proxy.Context) -> (result :UInt64);
    getHeaderTip @34 (context :Proxy.Context) -> (height :Int32, blockTime :Int64, result :Bool);
    getNumBlocks @35 (context :Proxy.Context) -> (result :Int32);
    getLastBlockTime @36 (context :Proxy.Context) -> (result :Int64);
    getVerificationProgress @37 (context :Proxy.Context) -> (result :Float64);
    isInitialBlockDownload @38 (context :Proxy.Context) -> (result :Bool);
    isAddressTypeSet @39 (context :Proxy.Context) -> (result :Bool);
    getReindex @40 (context :Proxy.Context) -> (result :Bool);
    getImporting @41 (context :Proxy.Context) -> (result :Bool);
    setNetworkActive @42 (context :Proxy.Context, active :Bool) -> ();
    getNetworkActive @43 (context :Proxy.Context) -> (result :Bool);
    estimateSmartFee @44 (context :Proxy.Context, numBlocks :Int32, conservative :Bool, wantReturnedTarget :Bool) -> (returnedTarget :Int32, result :Data);
    getDustRelayFee @45 (context :Proxy.Context) -> (result :Data);
    executeRpc @46 (context :Proxy.Context, command :Text, params :Common.UniValue, uri :Text) -> (error :Text $Proxy.exception("std::exception"), rpcError :Common.UniValue $Proxy.exception("UniValue"), result :Common.UniValue);
    listRpcCommands @47 (context :Proxy.Context) -> (result :List(Text));
    rpcSetTimerInterfaceIfUnset @48 (context :Proxy.Context, iface :Void) -> ();
    rpcUnsetTimerInterface @49 (context :Proxy.Context, iface :Void) -> ();
    getUnspentOutput @50 (context :Proxy.Context, output :Data) -> (coin :Data, result :Bool);
    getWalletDir @51 (context :Proxy.Context) -> (result :Text);
    listWalletDir @52 (context :Proxy.Context) -> (result :List(Text));
    getWallets @53 (context :Proxy.Context) -> (result :List(Wallet.Wallet));
    loadWallet @54 (context :Proxy.Context, name :Text) -> (error :Text, warning: Text, result :Wallet.Wallet);
    createWallet @55 (context :Proxy.Context, passphrase :Text, flags :UInt64, name :Text) -> (error :Text, warning: Text, status: Int32, result :Wallet.Wallet);
    handleInitMessage @56 (context :Proxy.Context, callback :InitMessageCallback) -> (result :Handler.Handler);
    handleMessageBox @57 (context :Proxy.Context, callback :MessageBoxCallback) -> (result :Handler.Handler);
    handleQuestion @58 (context :Proxy.Context, callback :QuestionCallback) -> (result :Handler.Handler);
    handleShowProgress @59 (context :Proxy.Context, callback :ShowNodeProgressCallback) -> (result :Handler.Handler);
    handleLoadWallet @60 (context :Proxy.Context, callback :LoadWalletCallback) -> (result :Handler.Handler);
    handleNotifyNumConnectionsChanged @61 (context :Proxy.Context, callback :NotifyNumConnectionsChangedCallback) -> (result :Handler.Handler);
    handleNotifyNetworkActiveChanged @62 (context :Proxy.Context, callback :NotifyNetworkActiveChangedCallback) -> (result :Handler.Handler);
    handleNotifyAlertChanged @63 (context :Proxy.Context, callback :NotifyAlertChangedCallback) -> (result :Handler.Handler);
    handleBannedListChanged @64 (context :Proxy.Context, callback :BannedListChangedCallback) -> (result :Handler.Handler);
    handleNotifyBlockTip @65 (context :Proxy.Context, callback :NotifyBlockTipCallback) -> (result :Handler.Handler);
    handleNotifyHeaderTip @66 (context :Proxy.Context, callback :NotifyHeaderTipCallback) -> (result :Handler.Handler);
}

interface InitMessageCallback $Proxy.wrap("ProxyCallback<interfaces::Node::InitMessageFn>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, message :Text) -> ();
}

interface MessageBoxCallback $Proxy.wrap("ProxyCallback<interfaces::Node::MessageBoxFn>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, message :Text, caption :Text, style :UInt32) -> (result :Bool);
}

interface QuestionCallback $Proxy.wrap("ProxyCallback<interfaces::Node::QuestionFn>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, message :Text, nonInteractiveMessage :Text, caption :Text, style :UInt32) -> (result :Bool);
}

interface ShowNodeProgressCallback $Proxy.wrap("ProxyCallback<interfaces::Node::ShowProgressFn>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, title :Text, progress :Int32, resumePossible :Bool) -> ();
}

interface LoadWalletCallback $Proxy.wrap("ProxyCallback<interfaces::Node::LoadWalletFn>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, wallet :Wallet.Wallet) -> ();
}

interface NotifyNumConnectionsChangedCallback $Proxy.wrap("ProxyCallback<interfaces::Node::NotifyNumConnectionsChangedFn>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, newNumConnections :Int32) -> ();
}

interface NotifyNetworkActiveChangedCallback $Proxy.wrap("ProxyCallback<interfaces::Node::NotifyNetworkActiveChangedFn>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, networkActive :Bool) -> ();
}

interface NotifyAlertChangedCallback $Proxy.wrap("ProxyCallback<interfaces::Node::NotifyAlertChangedFn>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context) -> ();
}

interface BannedListChangedCallback $Proxy.wrap("ProxyCallback<interfaces::Node::BannedListChangedFn>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context) -> ();
}

interface NotifyBlockTipCallback $Proxy.wrap("ProxyCallback<interfaces::Node::NotifyBlockTipFn>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, initialDownload :Bool, height :Int32, blockTime :Int64, verificationProgress :Float64) -> ();
}

interface NotifyHeaderTipCallback $Proxy.wrap("ProxyCallback<interfaces::Node::NotifyHeaderTipFn>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context, initialDownload :Bool, height :Int32, blockTime :Int64, verificationProgress :Float64) -> ();
}

struct ProxyType $Proxy.wrap("proxyType") {
    proxy @0 :Data;
    randomizeCredentials @1 :Bool $Proxy.name("randomize_credentials");
}

struct NodeStats $Proxy.wrap("CNodeStats") {
    nodeid @0 :Int64;
    services @1 :Int64 $Proxy.name("nServices");
    relayTxes @2 :Bool $Proxy.name("fRelayTxes");
    lastSend @3 :Int64 $Proxy.name("nLastSend");
    lastRecv @4 :Int64 $Proxy.name("nLastRecv");
    timeConnected @5 :Int64 $Proxy.name("nTimeConnected");
    timeOffset @6 :Int64 $Proxy.name("nTimeOffset");
    addrName @7 :Text;
    version @8 :Int32 $Proxy.name("nVersion");
    cleanSubVer @9 :Text;
    inbound @10 :Bool $Proxy.name("fInbound");
    manualConnection @11 :Bool $Proxy.name("m_manual_connection");
    startingHeight @12 :Int32 $Proxy.name("nStartingHeight");
    sendBytes @13 :UInt64 $Proxy.name("nSendBytes");
    sendBytesPerMsgCmd @14 :List(Common.PairStr64) $Proxy.name("mapSendBytesPerMsgCmd");
    recvBytes @15 :UInt64 $Proxy.name("nRecvBytes");
    recvBytesPerMsgCmd @16 :List(Common.PairStr64) $Proxy.name("mapRecvBytesPerMsgCmd");
    whitelisted @17 :Bool $Proxy.name("m_legacyWhitelisted");
    pingTime @18 :Float64 $Proxy.name("dPingTime");
    pingWait @19 :Float64 $Proxy.name("dPingWait");
    minPing @20 :Float64 $Proxy.name("dMinPing");
    addrLocal @21 :Text;
    addr @22 :Data;
    addrBind @23 :Data;
    stateStats @24 :NodeStateStats $Proxy.skip;
}

struct NodeStateStats $Proxy.wrap("CNodeStateStats") {
    misbehavior @0 :Int32 $Proxy.name("nMisbehavior");
    syncHeight @1 :Int32 $Proxy.name("nSyncHeight");
    commonHeight @2 :Int32 $Proxy.name("nCommonHeight");
    heightInFlight @3 :List(Int32) $Proxy.name("vHeightInFlight");
}
