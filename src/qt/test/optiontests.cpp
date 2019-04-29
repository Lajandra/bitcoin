// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <init.h>
#include <qt/bitcoin.h>
#include <qt/test/optiontests.h>
#include <test/util/setup_common.h>
#include <util/system.h>

#include <QSettings>
#include <QTest>

#include <univalue.h>

#include <fstream>

//! Entry point for BitcoinApplication tests.
void OptionTests::optionTests()
{
    // Test regression https://github.com/bitcoin/bitcoin/issues/24457. Ensure
    // that setting integer prune value doesn't cause an exception to be thrown
    // in the OptionsModel constructor
    gArgs.LockSettings([&](util::Settings& settings) {
        settings.forced_settings.erase("prune");
        settings.rw_settings["prune"] = 3814;
    });
    gArgs.WriteSettingsFile();
    OptionsModel{m_node};
    gArgs.LockSettings([&](util::Settings& settings) {
        settings.rw_settings.erase("prune");
    });
    gArgs.WriteSettingsFile();

    BasicTestingSetup test{CBaseChainParams::REGTEST};
    m_node.setContext(&test.m_node);
    m_node.updateSetting("wallet", {}); // Clear setting to be independent of ENABLE_WALLET

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

    std::ifstream file(gArgs.GetDataDirNet() / "settings.json");
    QCOMPARE(std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()).c_str(), "{\n"
        "    \"dbcache\": 600,\n"
        "    \"listen\": false,\n"
        "    \"onion\": \"onion:234\",\n"
        "    \"par\": 12,\n"
        "    \"proxy\": \"proxy:123\",\n"
        "    \"prune\": 2861\n"
        "}\n");
}

void OptionTests::parametersInteraction()
{
    // Test that the bug https://github.com/bitcoin-core/gui/issues/567 does not resurface.
    // It was fixed via https://github.com/bitcoin-core/gui/pull/568.
    // With fListen=false in ~/.config/Bitcoin/Bitcoin-Qt.conf and all else left as default,
    // bitcoin-qt should set both -listen and -listenonion to false and start successfully.
    gArgs.ClearPathCache();

    gArgs.LockSettings([&](util::Settings& s) {
        s.forced_settings.erase("listen");
        s.forced_settings.erase("listenonion");
    });
    QVERIFY(!gArgs.IsArgSet("-listen"));
    QVERIFY(!gArgs.IsArgSet("-listenonion"));

    QSettings settings;
    settings.setValue("fListen", false);

    OptionsModel{};

    const bool expected{false};

    QVERIFY(gArgs.IsArgSet("-listen"));
    QCOMPARE(gArgs.GetBoolArg("-listen", !expected), expected);

    QVERIFY(gArgs.IsArgSet("-listenonion"));
    QCOMPARE(gArgs.GetBoolArg("-listenonion", !expected), expected);

    QVERIFY(AppInitParameterInteraction(gArgs));

    // cleanup
    settings.remove("fListen");
    QVERIFY(!settings.contains("fListen"));
    gArgs.ClearPathCache();
}
