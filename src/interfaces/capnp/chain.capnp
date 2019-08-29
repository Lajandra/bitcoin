@0x94f21a4864bd2c65;

using Cxx = import "/capnp/c++.capnp";
$Cxx.namespace("interfaces::capnp::messages");

using Proxy = import "/mp/proxy.capnp";
using Common = import "common.capnp";
using Handler = import "handler.capnp";
using Wallet = import "wallet.capnp";

interface Chain $Proxy.wrap("interfaces::Chain") {
    destroy @0 (context :Proxy.Context) -> ();
    lock @1 (context :Proxy.Context, tryLock :Bool) -> (result :ChainLock);
    findBlock @2 (context :Proxy.Context, hash :Data, wantBlock :Bool, wantTime :Bool, wantMaxTime :Bool) -> (block :Data, time :Int64, maxTime :Int64, result :Bool);
    findCoins @3 (context :Proxy.Context, coins :List(Common.Pair(Data, Data))) -> (coins :List(Common.Pair(Data, Data)));
    guessVerificationProgress @4 (context :Proxy.Context, blockHash :Data) -> (result :Float64);
    isRBFOptIn @5 (context :Proxy.Context, tx :Data) -> (result :Int32);
    hasDescendantsInMempool @6 (context :Proxy.Context, txid :Data) -> (result :Bool);
    broadcastTransaction @7 (context :Proxy.Context, tx: Data, maxTxFee :Int64, relay :Bool) -> (error: Text, result :Bool);
    getTransactionAncestry @8 (context :Proxy.Context, txid :Data) -> (ancestors :UInt64, descendants :UInt64);
    checkChainLimits @9 (context :Proxy.Context, tx :Data) -> (result :Bool);
    estimateSmartFee @10 (context :Proxy.Context, numBlocks :Int32, conservative :Bool, wantCalc :Bool) -> (calc :FeeCalculation, result :Data);
    estimateMaxBlocks @11 (context :Proxy.Context) -> (result :UInt32);
    mempoolMinFee @12 (context :Proxy.Context) -> (result :Data);
    relayMinFee @13 (context :Proxy.Context) -> (result :Data);
    relayIncrementalFee @14 (context :Proxy.Context) -> (result :Data);
    relayDustFee @15 (context :Proxy.Context) -> (result :Data);
    havePruned @16 (context :Proxy.Context) -> (result :Bool);
    isReadyToBroadcast @17 (context :Proxy.Context) -> (result :Bool);
    isInitialBlockDownload @18 (context :Proxy.Context) -> (result :Bool);
    shutdownRequested @19 (context :Proxy.Context) -> (result :Bool);
    getAdjustedTime @20 (context :Proxy.Context) -> (result :Int64);
    initMessage @21 (context :Proxy.Context, message :Text) -> ();
    initWarning @22 (context :Proxy.Context, message :Text) -> ();
    initError @23 (context :Proxy.Context, message :Text) -> ();
    loadWallet @24 (context :Proxy.Context, wallet :Wallet.Wallet) -> ();
    showProgress @25 (context :Proxy.Context, title :Text, progress :Int32, resumePossible :Bool) -> ();
    handleNotifications @26 (context :Proxy.Context, notifications :ChainNotifications) -> (result :Handler.Handler);
    waitForNotificationsIfNewBlocksConnected @27 (context :Proxy.Context, oldTip :Data) -> ();
    handleRpc @28 (context :Proxy.Context, command :RPCCommand) -> (result :Handler.Handler);
    rpcEnableDeprecated @29 (context :Proxy.Context, method :Text) -> (result :Bool);
    rpcRunLater @30 (context :Proxy.Context, name :Text, fn: RunLaterCallback, seconds: Int64) -> ();
    rpcSerializationFlags @31 (context :Proxy.Context) -> (result :Int32);
    requestMempoolTransactions @32 (context :Proxy.Context, notifications :ChainNotifications) -> ();
}

interface ChainLock $Proxy.wrap("interfaces::Chain::Lock") {
    destroy @0 (context :Proxy.Context) -> ();
    getHeight @1 (context :Proxy.Context) -> (result :Int32, hasResult :Bool);
    getBlockHeight @2 (context :Proxy.Context, hash :Data) -> (result :Int32, hasResult :Bool);
    getBlockDepth @3 (context :Proxy.Context, hash :Data) -> (result :Int32);
    getBlockHash @4 (context :Proxy.Context, height :Int32) -> (result :Data);
    getBlockTime @5 (context :Proxy.Context, height :Int32) -> (result :Int64);
    getBlockMedianTimePast @6 (context :Proxy.Context, height :Int32) -> (result :Int64);
    haveBlockOnDisk @7 (context :Proxy.Context, height :Int32) -> (result :Bool);
    findFirstBlockWithTimeAndHeight @8 (context :Proxy.Context, time :Int64, startHeight :Int32) -> (hash :Data, result :Int32, hasResult :Bool);
    findPruned @9 (context :Proxy.Context, startHeight: Int32, stopHeight :Int32, hasStopHeight :Bool) -> (result :Int32, hasResult :Bool);
    findFork @10 (context :Proxy.Context, hash :Data, wantHeight :Bool) -> (height :Int32, hasHeight :Int32, result :Int32, hasResult :Bool);
    getTipLocator @11 (context :Proxy.Context) -> (result :Data);
    findLocatorFork @12 (context :Proxy.Context, locator :Data) -> (result :Int32, hasResult :Bool);
    checkFinalTx @13 (context :Proxy.Context, tx :Data) -> (result :Bool);
}

interface ChainNotifications $Proxy.wrap("interfaces::Chain::Notifications") {
    destroy @0 (context :Proxy.Context) -> ();
    transactionAddedToMempool @1 (context :Proxy.Context, tx :Data) -> () $Proxy.name("TransactionAddedToMempool");
    transactionRemovedFromMempool @2 (context :Proxy.Context, tx :Data) -> () $Proxy.name("TransactionRemovedFromMempool");
    blockConnected @3 (context :Proxy.Context, block :Data, txConflicted :List(Data)) -> () $Proxy.name("BlockConnected");
    blockDisconnected @4 (context :Proxy.Context, block :Data) -> () $Proxy.name("BlockDisconnected");
    updatedBlockTip @5 (context :Proxy.Context) -> () $Proxy.name("UpdatedBlockTip");
    chainStateFlushed @6 (context :Proxy.Context, locator :Data) -> () $Proxy.name("ChainStateFlushed");
}

interface ChainClient $Proxy.wrap("interfaces::ChainClient") {
    destroy @0 (context :Proxy.Context) -> ();
    registerRpcs @1 (context :Proxy.Context) -> ();
    verify @2 (context :Proxy.Context) -> (result :Bool);
    load @3 (context :Proxy.Context) -> (result :Bool);
    start @4 (context :Proxy.Context, scheduler :Void) -> ();
    flush @5 (context :Proxy.Context) -> ();
    stop @6 (context :Proxy.Context) -> ();
    setMockTime @7 (context :Proxy.Context, time :Int64) -> ();
    getWallets @8 (context :Proxy.Context) -> (result :List(Wallet.Wallet));
}

struct FeeCalculation $Proxy.wrap("FeeCalculation") {
    est @0 :EstimationResult;
    reason @1 :Int32;
    desiredTarget @2 :Int32;
    returnedTarget @3 :Int32;
}

struct EstimationResult $Proxy.wrap("EstimationResult")
{
    pass @0 :EstimatorBucket;
    fail @1 :EstimatorBucket;
    decay @2 :Float64;
    scale @3 :UInt32;
}

struct EstimatorBucket $Proxy.wrap("EstimatorBucket")
{
    start @0 :Float64;
    end @1 :Float64;
    withinTarget @2 :Float64;
    totalConfirmed @3 :Float64;
    inMempool @4 :Float64;
    leftMempool @5 :Float64;
}

struct RPCCommand $Proxy.wrap("CRPCCommand") {
   category @0 :Text;
   name @1 :Text;
   actor @2 :ActorCallback;
   argNames @3 :List(Text);
   uniqueId @4 :Int64 $Proxy.name("unique_id");
}

interface ActorCallback $Proxy.wrap("ProxyCallback<CRPCCommand::Actor>") {
    call @0 (context :Proxy.Context, request :JSONRPCRequest, response :Common.UniValue, lastCallback :Bool) -> (error :Text $Proxy.exception("std::exception"), rpcError :Common.UniValue $Proxy.exception("UniValue"), response :Common.UniValue, result: Bool);
}

struct JSONRPCRequest $Proxy.wrap("JSONRPCRequest") {
    id @0 :Common.UniValue;
    method @1 :Text $Proxy.name("strMethod");
    params @2 :Common.UniValue;
    help @3 :Bool $Proxy.name("fHelp");
    uri @4 :Text $Proxy.name("URI");
    authUser @5 :Text;
}

interface RunLaterCallback $Proxy.wrap("ProxyCallback<std::function<void()>>") {
    destroy @0 (context :Proxy.Context) -> ();
    call @1 (context :Proxy.Context) -> ();
}

struct ValidationState {
    valid @0 :Bool;
    error @1 :Bool;
    reason @2 :Int32;
    rejectCode @3 :UInt32;
    rejectReason @4 :Text;
    debugMessage @5 :Text;
}
