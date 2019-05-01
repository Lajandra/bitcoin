// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/bitcoin.h>
#include <qt/test/optiontests.h>
#include <util/system.h>

#include <QSettings>
#include <QTest>

namespace {
} // namespace

//! Entry point for BitcoinApplication tests.
void OptionTests::optionTests()
{
    QSettings settings;
    settings.setValue("nDatabaseCache", 600);
    settings.setValue("nThreadsScriptVerif", 12);
    settings.setValue("fUseUPnP", false);
    settings.setValue("fListen", false);
    settings.setValue("bPrune", true);
    settings.setValue("nPruneSize", 3);
    settings.setValue("fUseProxy", true);
    settings.setValue("addrProxy", "proxy:123");
    settings.setValue("fUseSeparateProxyTor", true);
    settings.setValue("addrSeparateProxyTor", "onion:234");

    settings.sync();

    OptionsModel options(m_node);
    QVERIFY(!settings.contains("nDatabaseCache"));
    QVERIFY(!settings.contains("nThreadsScriptVerif"));
    QVERIFY(!settings.contains("fUseUPnP"));
    QVERIFY(!settings.contains("fListen"));
    QVERIFY(!settings.contains("bPrune"));
    QVERIFY(!settings.contains("nPruneSize"));
    QVERIFY(!settings.contains("fUseProxy"));
    QVERIFY(!settings.contains("addrProxy"));
    QVERIFY(!settings.contains("fUseSeparateProxyTor"));
    QVERIFY(!settings.contains("addrSeparateProxyTor"));

    fsbridge::ifstream file(GetDataDir(/* net_specific= */ true) / "settings.json");
    QCOMPARE(std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()).c_str(), R"({
    "dbcache": 600,
    "listen": false,
    "onion": "onion:234",
    "par": 12,
    "proxy": "proxy:123",
    "prune": 2861
   }
)");
}
