#include "ipc/capnp/serialize.h"

#include "clientversion.h"
#include "net.h"
#include "net_processing.h"

namespace ipc {
namespace capnp {

void BuildNodeStats(messages::NodeStats::Builder&& builder, const CNodeStats& nodeStats)
{
    builder.setNodeid(nodeStats.nodeid);
    builder.setServices(nodeStats.nServices);
    builder.setRelayTxes(nodeStats.fRelayTxes);
    builder.setLastSend(nodeStats.nLastSend);
    builder.setLastRecv(nodeStats.nLastRecv);
    builder.setTimeConnected(nodeStats.nTimeConnected);
    builder.setTimeOffset(nodeStats.nTimeOffset);
    builder.setAddrName(nodeStats.addrName);
    builder.setVersion(nodeStats.nVersion);
    builder.setCleanSubVer(nodeStats.cleanSubVer);
    builder.setInbound(nodeStats.fInbound);
    builder.setAddnode(nodeStats.fAddnode);
    builder.setStartingHeight(nodeStats.nStartingHeight);
    builder.setSendBytes(nodeStats.nSendBytes);
    auto resultSend = builder.initSendBytesPerMsgCmd().initEntries(nodeStats.mapSendBytesPerMsgCmd.size());
    size_t i = 0;
    for (const auto& entry : nodeStats.mapSendBytesPerMsgCmd) {
        resultSend[i].setKey(entry.first);
        resultSend[i].setValue(entry.second);
        ++i;
    }
    builder.setRecvBytes(nodeStats.nRecvBytes);
    auto resultRecv = builder.initRecvBytesPerMsgCmd().initEntries(nodeStats.mapRecvBytesPerMsgCmd.size());
    i = 0;
    for (const auto& entry : nodeStats.mapRecvBytesPerMsgCmd) {
        resultRecv[i].setKey(entry.first);
        resultRecv[i].setValue(entry.second);
        ++i;
    }
    builder.setWhitelisted(nodeStats.fWhitelisted);
    builder.setPingTime(nodeStats.dPingTime);
    builder.setPingWait(nodeStats.dPingWait);
    builder.setMinPing(nodeStats.dMinPing);
    builder.setAddrLocal(nodeStats.addrLocal);
    builder.setAddr(ToArray(Serialize(nodeStats.addr)));
}

void ReadNodeStats(CNodeStats& nodeStats, const messages::NodeStats::Reader& reader)
{
    nodeStats.nodeid = NodeId(reader.getNodeid());
    nodeStats.nServices = ServiceFlags(reader.getServices());
    nodeStats.fRelayTxes = reader.getRelayTxes();
    nodeStats.nLastSend = reader.getLastSend();
    nodeStats.nLastRecv = reader.getLastRecv();
    nodeStats.nTimeConnected = reader.getTimeConnected();
    nodeStats.nTimeOffset = reader.getTimeOffset();
    nodeStats.addrName = ToString(reader.getAddrName());
    nodeStats.nVersion = reader.getVersion();
    nodeStats.cleanSubVer = ToString(reader.getCleanSubVer());
    nodeStats.fInbound = reader.getInbound();
    nodeStats.fAddnode = reader.getAddnode();
    nodeStats.nStartingHeight = reader.getStartingHeight();
    nodeStats.nSendBytes = reader.getSendBytes();
    for (const auto& entry : reader.getSendBytesPerMsgCmd().getEntries()) {
        nodeStats.mapSendBytesPerMsgCmd[ToString(entry.getKey())] = entry.getValue();
    }
    nodeStats.nRecvBytes = reader.getRecvBytes();
    for (const auto& entry : reader.getRecvBytesPerMsgCmd().getEntries()) {
        nodeStats.mapRecvBytesPerMsgCmd[ToString(entry.getKey())] = entry.getValue();
    }
    nodeStats.fWhitelisted = reader.getWhitelisted();
    nodeStats.dPingTime = reader.getPingTime();
    nodeStats.dPingWait = reader.getPingWait();
    nodeStats.dMinPing = reader.getMinPing();
    nodeStats.addrLocal = ToString(reader.getAddrLocal());
    Unserialize(nodeStats.addr, reader.getAddr());
}

void BuildNodeStateStats(messages::NodeStateStats::Builder&& builder, const CNodeStateStats& nodeStateStats)
{
    builder.setMisbehavior(nodeStateStats.nMisbehavior);
    builder.setSyncHeight(nodeStateStats.nSyncHeight);
    builder.setCommonHeight(nodeStateStats.nCommonHeight);
    auto heights = builder.initHeightInFlight(nodeStateStats.vHeightInFlight.size());
    size_t i = 0;
    for (int height : nodeStateStats.vHeightInFlight) {
        heights.set(i, height);
        ++i;
    }
}

void ReadNodeStateStats(CNodeStateStats& nodeStateStats, const messages::NodeStateStats::Reader& reader)
{
    nodeStateStats.nMisbehavior = reader.getMisbehavior();
    nodeStateStats.nSyncHeight = reader.getSyncHeight();
    nodeStateStats.nCommonHeight = reader.getCommonHeight();
    for (int height : reader.getHeightInFlight()) {
        nodeStateStats.vHeightInFlight.emplace_back(height);
    }
}

} // namespace capnp
} // namespace ipc
