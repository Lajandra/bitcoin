// Copyright (c) 2011-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <qt/optionsmodel.h>

#include <qt/bitcoinunits.h>
#include <qt/guiconstants.h>
#include <qt/guiutil.h>

#include <interfaces/node.h>
#include <mapport.h>
#include <net.h>
#include <netbase.h>
#include <txdb.h>       // for -dbcache defaults
#include <util/string.h>
#include <validation.h> // For DEFAULT_SCRIPTCHECK_THREADS
#include <wallet/wallet.h> // For DEFAULT_SPEND_ZEROCONF_CHANGE

#include <QDebug>
#include <QLatin1Char>
#include <QSettings>
#include <QStringList>
#include <QVariant>

#include <univalue.h>

const char *DEFAULT_GUI_PROXY_HOST = "127.0.0.1";

static const QString GetDefaultProxyAddress();

const char* SettingName(OptionsModel::OptionID option)
{
    switch (option) {
    case OptionsModel::DatabaseCache: return "dbcache";
    case OptionsModel::ThreadsScriptVerif: return "par";
    case OptionsModel::SpendZeroConfChange: return "spendzeroconfchange";
    case OptionsModel::ExternalSignerPath: return "signer";
    case OptionsModel::MapPortUPnP: return "upnp";
    case OptionsModel::MapPortNatpmp: return "natpmp";
    default: throw std::logic_error(strprintf("GUI option %i has no corresponding node setting.", option));
    };
}

void UpdateSetting(interfaces::Node& node, OptionsModel::OptionID option, const util::SettingsValue& value)
{
    // Write int settings that Bitcoin 22.x releases try read as strings at
    // startup as strings instead of numbers, to prevent uncaught exceptions in
    // addOverriddenOption() thrown by UniValue::get_str(). These errors were
    // fixed in later releases by https://github.com/bitcoin/bitcoin/pull/24498.
    if (value.isNum() &&
        (option == OptionsModel::DatabaseCache ||
         option == OptionsModel::ThreadsScriptVerif)) {
        node.updateSetting(SettingName(option), value.getValStr());
    } else {
        node.updateSetting(SettingName(option), value);
    }
}

OptionsModel::OptionsModel(interfaces::Node& node, QObject *parent) :
    QAbstractListModel(parent), m_node{node}
{
}

void OptionsModel::addOverriddenOption(const std::string &option)
{
    strOverriddenByCommandLine += QString::fromStdString(option) + "=" + QString::fromStdString(gArgs.GetArg(option, "")) + " ";
}

// Writes all missing QSettings with their default values
bool OptionsModel::Init(bilingual_str& error)
{
    checkAndMigrate();

    QSettings settings;

    // Ensure restart flag is unset on client startup
    setRestartRequired(false);

    // These are Qt-only settings:

    // Window
    if (!settings.contains("fHideTrayIcon")) {
        settings.setValue("fHideTrayIcon", false);
    }
    m_show_tray_icon = !settings.value("fHideTrayIcon").toBool();
    Q_EMIT showTrayIconChanged(m_show_tray_icon);

    if (!settings.contains("fMinimizeToTray"))
        settings.setValue("fMinimizeToTray", false);
    fMinimizeToTray = settings.value("fMinimizeToTray").toBool() && m_show_tray_icon;

    if (!settings.contains("fMinimizeOnClose"))
        settings.setValue("fMinimizeOnClose", false);
    fMinimizeOnClose = settings.value("fMinimizeOnClose").toBool();

    // Display
    if (!settings.contains("DisplayBitcoinUnit")) {
        settings.setValue("DisplayBitcoinUnit", QVariant::fromValue(BitcoinUnit::BTC));
    }
    QVariant unit = settings.value("DisplayBitcoinUnit");
    if (unit.canConvert<BitcoinUnit>()) {
        m_display_bitcoin_unit = unit.value<BitcoinUnit>();
    } else {
        m_display_bitcoin_unit = BitcoinUnit::BTC;
        settings.setValue("DisplayBitcoinUnit", QVariant::fromValue(m_display_bitcoin_unit));
    }

    if (!settings.contains("strThirdPartyTxUrls"))
        settings.setValue("strThirdPartyTxUrls", "");
    strThirdPartyTxUrls = settings.value("strThirdPartyTxUrls", "").toString();

    if (!settings.contains("fCoinControlFeatures"))
        settings.setValue("fCoinControlFeatures", false);
    fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();

    if (!settings.contains("enable_psbt_controls")) {
        settings.setValue("enable_psbt_controls", false);
    }
    m_enable_psbt_controls = settings.value("enable_psbt_controls", false).toBool();

    // These are shared with the core or have a command-line parameter
    // and we want command-line parameters to overwrite the GUI settings.
    for (OptionID option : {DatabaseCache, ThreadsScriptVerif, SpendZeroConfChange, ExternalSignerPath, MapPortUPnP,
                            MapPortNatpmp}) {
        std::string setting = SettingName(option);
        if (node().isSettingIgnored(setting)) addOverriddenOption("-" + setting);
        try {
            getOption(option);
        } catch (const std::exception& e) {
            // This handles exceptions thrown by univalue that can happen if
            // settings in settings.json don't have the expected types.
            error = strprintf(_("Could not read setting \"%s\", %s."), setting, e.what());
            return false;
        }
    }

    // If setting doesn't exist create it with defaults.
    //
    // If gArgs.SoftSetArg() or gArgs.SoftSetBoolArg() return false we were overridden
    // by command-line and show this in the UI.

    // Main
    if (!settings.contains("bPrune"))
        settings.setValue("bPrune", false);
    if (!settings.contains("nPruneSize"))
        settings.setValue("nPruneSize", DEFAULT_PRUNE_TARGET_GB);
    SetPruneEnabled(settings.value("bPrune").toBool());
    if (!settings.contains("strDataDir"))
        settings.setValue("strDataDir", GUIUtil::getDefaultDataDirectory());

    // Wallet
#ifdef ENABLE_WALLET
    if (!settings.contains("SubFeeFromAmount")) {
        settings.setValue("SubFeeFromAmount", false);
    }
    m_sub_fee_from_amount = settings.value("SubFeeFromAmount", false).toBool();
#endif

    // Network
    if (!settings.contains("fListen"))
        settings.setValue("fListen", DEFAULT_LISTEN);
    const bool listen{settings.value("fListen").toBool()};
    if (!gArgs.SoftSetBoolArg("-listen", listen)) {
        addOverriddenOption("-listen");
    } else if (!listen) {
        // We successfully set -listen=0, thus mimic the logic from InitParameterInteraction():
        // "parameter interaction: -listen=0 -> setting -listenonion=0".
        //
        // Both -listen and -listenonion default to true.
        //
        // The call order is:
        //
        // InitParameterInteraction()
        //     would set -listenonion=0 if it sees -listen=0, but for bitcoin-qt with
        //     fListen=false -listen is 1 at this point
        //
        // OptionsModel::Init()
        //     (this method) can flip -listen from 1 to 0 if fListen=false
        //
        // AppInitParameterInteraction()
        //     raises an error if -listen=0 and -listenonion=1
        gArgs.SoftSetBoolArg("-listenonion", false);
    }

    if (!settings.contains("server")) {
        settings.setValue("server", false);
    }
    if (!gArgs.SoftSetBoolArg("-server", settings.value("server").toBool())) {
        addOverriddenOption("-server");
    }

    if (!settings.contains("fUseProxy"))
        settings.setValue("fUseProxy", false);
    if (!settings.contains("addrProxy"))
        settings.setValue("addrProxy", GetDefaultProxyAddress());
    // Only try to set -proxy, if user has enabled fUseProxy
    if ((settings.value("fUseProxy").toBool() && !gArgs.SoftSetArg("-proxy", settings.value("addrProxy").toString().toStdString())))
        addOverriddenOption("-proxy");
    else if(!settings.value("fUseProxy").toBool() && !gArgs.GetArg("-proxy", "").empty())
        addOverriddenOption("-proxy");

    if (!settings.contains("fUseSeparateProxyTor"))
        settings.setValue("fUseSeparateProxyTor", false);
    if (!settings.contains("addrSeparateProxyTor"))
        settings.setValue("addrSeparateProxyTor", GetDefaultProxyAddress());
    // Only try to set -onion, if user has enabled fUseSeparateProxyTor
    if ((settings.value("fUseSeparateProxyTor").toBool() && !gArgs.SoftSetArg("-onion", settings.value("addrSeparateProxyTor").toString().toStdString())))
        addOverriddenOption("-onion");
    else if(!settings.value("fUseSeparateProxyTor").toBool() && !gArgs.GetArg("-onion", "").empty())
        addOverriddenOption("-onion");

    // Display
    if (!settings.contains("language"))
        settings.setValue("language", "");
    if (!gArgs.SoftSetArg("-lang", settings.value("language").toString().toStdString()))
        addOverriddenOption("-lang");

    language = settings.value("language").toString();

    if (!settings.contains("UseEmbeddedMonospacedFont")) {
        settings.setValue("UseEmbeddedMonospacedFont", "true");
    }
    m_use_embedded_monospaced_font = settings.value("UseEmbeddedMonospacedFont").toBool();
    Q_EMIT useEmbeddedMonospacedFontChanged(m_use_embedded_monospaced_font);

    return true;
}

/** Helper function to copy contents from one QSettings to another.
 * By using allKeys this also covers nested settings in a hierarchy.
 */
static void CopySettings(QSettings& dst, const QSettings& src)
{
    for (const QString& key : src.allKeys()) {
        dst.setValue(key, src.value(key));
    }
}

/** Back up a QSettings to an ini-formatted file. */
static void BackupSettings(const fs::path& filename, const QSettings& src)
{
    qInfo() << "Backing up GUI settings to" << GUIUtil::PathToQString(filename);
    QSettings dst(GUIUtil::PathToQString(filename), QSettings::IniFormat);
    dst.clear();
    CopySettings(dst, src);
}

void OptionsModel::Reset()
{
    QSettings settings;

    // Backup old settings to chain-specific datadir for troubleshooting
    BackupSettings(gArgs.GetDataDirNet() / "guisettings.ini.bak", settings);

    // Save the strDataDir setting
    QString dataDir = GUIUtil::getDefaultDataDirectory();
    dataDir = settings.value("strDataDir", dataDir).toString();

    // Remove all entries from our QSettings object
    settings.clear();

    // Set strDataDir
    settings.setValue("strDataDir", dataDir);

    // Set that this was reset
    settings.setValue("fReset", true);

    // default setting for OptionsModel::StartAtStartup - disabled
    if (GUIUtil::GetStartOnSystemStartup())
        GUIUtil::SetStartOnSystemStartup(false);
}

int OptionsModel::rowCount(const QModelIndex & parent) const
{
    return OptionIDRowCount;
}

struct ProxySetting {
    bool is_set;
    QString ip;
    QString port;
};

static ProxySetting GetProxySetting(QSettings &settings, const QString &name)
{
    static const ProxySetting default_val = {false, DEFAULT_GUI_PROXY_HOST, QString("%1").arg(DEFAULT_GUI_PROXY_PORT)};
    // Handle the case that the setting is not set at all
    if (!settings.contains(name)) {
        return default_val;
    }
    // contains IP at index 0 and port at index 1
    QStringList ip_port = GUIUtil::SplitSkipEmptyParts(settings.value(name).toString(), ":");
    if (ip_port.size() == 2) {
        return {true, ip_port.at(0), ip_port.at(1)};
    } else { // Invalid: return default
        return default_val;
    }
}

static void SetProxySetting(QSettings &settings, const QString &name, const ProxySetting &ip_port)
{
    settings.setValue(name, QString{ip_port.ip + QLatin1Char(':') + ip_port.port});
}

static const QString GetDefaultProxyAddress()
{
    return QString("%1:%2").arg(DEFAULT_GUI_PROXY_HOST).arg(DEFAULT_GUI_PROXY_PORT);
}

void OptionsModel::SetPruneEnabled(bool prune, bool force)
{
    QSettings settings;
    settings.setValue("bPrune", prune);
    const int64_t prune_target_mib = PruneGBtoMiB(settings.value("nPruneSize").toInt());
    std::string prune_val = prune ? ToString(prune_target_mib) : "0";
    if (force) {
        gArgs.ForceSetArg("-prune", prune_val);
        return;
    }
    if (!gArgs.SoftSetArg("-prune", prune_val)) {
        addOverriddenOption("-prune");
    }
}

void OptionsModel::SetPruneTargetGB(int prune_target_gb, bool force)
{
    const bool prune = prune_target_gb > 0;
    if (prune) {
        QSettings settings;
        settings.setValue("nPruneSize", prune_target_gb);
    }
    SetPruneEnabled(prune, force);
}

// read QSettings values and return them
QVariant OptionsModel::data(const QModelIndex & index, int role) const
{
    if(role == Qt::EditRole)
    {
        return getOption(OptionID(index.row()));
    }
    return QVariant();
}

// write QSettings values
bool OptionsModel::setData(const QModelIndex & index, const QVariant & value, int role)
{
    bool successful = true; /* set to false on parse error */
    if(role == Qt::EditRole)
    {
        successful = setOption(OptionID(index.row()), value);
    }

    Q_EMIT dataChanged(index, index);

    return successful;
}

QVariant OptionsModel::getOption(OptionID option) const
{
    auto setting = [&]{ return node().getPersistentSetting(SettingName(option)); };

    QSettings settings;
    switch (option) {
    case StartAtStartup:
        return GUIUtil::GetStartOnSystemStartup();
    case ShowTrayIcon:
        return m_show_tray_icon;
    case MinimizeToTray:
        return fMinimizeToTray;
    case MapPortUPnP:
#ifdef USE_UPNP
        return SettingToBool(setting(), DEFAULT_UPNP);
#else
        return false;
#endif // USE_UPNP
    case MapPortNatpmp:
#ifdef USE_NATPMP
        return SettingToBool(setting(), DEFAULT_NATPMP);
#else
        return false;
#endif // USE_NATPMP
    case MinimizeOnClose:
        return fMinimizeOnClose;

    // default proxy
    case ProxyUse:
        return settings.value("fUseProxy", false);
    case ProxyIP:
        return GetProxySetting(settings, "addrProxy").ip;
    case ProxyPort:
        return GetProxySetting(settings, "addrProxy").port;

    // separate Tor proxy
    case ProxyUseTor:
        return settings.value("fUseSeparateProxyTor", false);
    case ProxyIPTor:
        return GetProxySetting(settings, "addrSeparateProxyTor").ip;
    case ProxyPortTor:
        return GetProxySetting(settings, "addrSeparateProxyTor").port;

#ifdef ENABLE_WALLET
    case SpendZeroConfChange:
        return SettingToBool(setting(), wallet::DEFAULT_SPEND_ZEROCONF_CHANGE);
    case ExternalSignerPath:
        return QString::fromStdString(SettingToString(setting(), ""));
    case SubFeeFromAmount:
        return m_sub_fee_from_amount;
#endif
    case DisplayUnit:
        return QVariant::fromValue(m_display_bitcoin_unit);
    case ThirdPartyTxUrls:
        return strThirdPartyTxUrls;
    case Language:
        return settings.value("language");
    case UseEmbeddedMonospacedFont:
        return m_use_embedded_monospaced_font;
    case CoinControlFeatures:
        return fCoinControlFeatures;
    case EnablePSBTControls:
        return settings.value("enable_psbt_controls");
    case Prune:
        return settings.value("bPrune");
    case PruneSize:
        return settings.value("nPruneSize");
    case DatabaseCache:
        return qlonglong(SettingToInt(setting(), nDefaultDbCache));
    case ThreadsScriptVerif:
        return qlonglong(SettingToInt(setting(), DEFAULT_SCRIPTCHECK_THREADS));
    case Listen:
        return settings.value("fListen");
    case Server:
        return settings.value("server");
    default:
        return QVariant();
    }
}

bool OptionsModel::setOption(OptionID option, const QVariant& value)
{
    auto changed = [&] { return value.isValid() && value != getOption(option); };
    auto update = [&](const util::SettingsValue& value) { return UpdateSetting(node(), option, value); };

    bool successful = true; /* set to false on parse error */
    QSettings settings;

    switch (option) {
    case StartAtStartup:
        successful = GUIUtil::SetStartOnSystemStartup(value.toBool());
        break;
    case ShowTrayIcon:
        m_show_tray_icon = value.toBool();
        settings.setValue("fHideTrayIcon", !m_show_tray_icon);
        Q_EMIT showTrayIconChanged(m_show_tray_icon);
        break;
    case MinimizeToTray:
        fMinimizeToTray = value.toBool();
        settings.setValue("fMinimizeToTray", fMinimizeToTray);
        break;
    case MapPortUPnP: // core option - can be changed on-the-fly
        if (changed()) {
            update(value.toBool());
            node().mapPort(value.toBool(), getOption(MapPortNatpmp).toBool());
        }
        break;
    case MapPortNatpmp: // core option - can be changed on-the-fly
        if (changed()) {
            update(value.toBool());
            node().mapPort(getOption(MapPortUPnP).toBool(), value.toBool());
        }
        break;
    case MinimizeOnClose:
        fMinimizeOnClose = value.toBool();
        settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
        break;

    // default proxy
    case ProxyUse:
        if (settings.value("fUseProxy") != value) {
            settings.setValue("fUseProxy", value.toBool());
            setRestartRequired(true);
        }
        break;
    case ProxyIP: {
        auto ip_port = GetProxySetting(settings, "addrProxy");
        if (!ip_port.is_set || ip_port.ip != value.toString()) {
            ip_port.ip = value.toString();
            SetProxySetting(settings, "addrProxy", ip_port);
            setRestartRequired(true);
        }
    }
    break;
    case ProxyPort: {
        auto ip_port = GetProxySetting(settings, "addrProxy");
        if (!ip_port.is_set || ip_port.port != value.toString()) {
            ip_port.port = value.toString();
            SetProxySetting(settings, "addrProxy", ip_port);
            setRestartRequired(true);
        }
    }
    break;

    // separate Tor proxy
    case ProxyUseTor:
        if (settings.value("fUseSeparateProxyTor") != value) {
            settings.setValue("fUseSeparateProxyTor", value.toBool());
            setRestartRequired(true);
        }
        break;
    case ProxyIPTor: {
        auto ip_port = GetProxySetting(settings, "addrSeparateProxyTor");
        if (!ip_port.is_set || ip_port.ip != value.toString()) {
            ip_port.ip = value.toString();
            SetProxySetting(settings, "addrSeparateProxyTor", ip_port);
            setRestartRequired(true);
        }
    }
    break;
    case ProxyPortTor: {
        auto ip_port = GetProxySetting(settings, "addrSeparateProxyTor");
        if (!ip_port.is_set || ip_port.port != value.toString()) {
            ip_port.port = value.toString();
            SetProxySetting(settings, "addrSeparateProxyTor", ip_port);
            setRestartRequired(true);
        }
    }
    break;

#ifdef ENABLE_WALLET
    case SpendZeroConfChange:
        if (changed()) {
            update(value.toBool());
            setRestartRequired(true);
        }
        break;
    case ExternalSignerPath:
        if (changed()) {
            update(value.toString().toStdString());
            setRestartRequired(true);
        }
        break;
    case SubFeeFromAmount:
        m_sub_fee_from_amount = value.toBool();
        settings.setValue("SubFeeFromAmount", m_sub_fee_from_amount);
        break;
#endif
    case DisplayUnit:
        setDisplayUnit(value);
        break;
    case ThirdPartyTxUrls:
        if (strThirdPartyTxUrls != value.toString()) {
            strThirdPartyTxUrls = value.toString();
            settings.setValue("strThirdPartyTxUrls", strThirdPartyTxUrls);
            setRestartRequired(true);
        }
        break;
    case Language:
        if (settings.value("language") != value) {
            settings.setValue("language", value);
            setRestartRequired(true);
        }
        break;
    case UseEmbeddedMonospacedFont:
        m_use_embedded_monospaced_font = value.toBool();
        settings.setValue("UseEmbeddedMonospacedFont", m_use_embedded_monospaced_font);
        Q_EMIT useEmbeddedMonospacedFontChanged(m_use_embedded_monospaced_font);
        break;
    case CoinControlFeatures:
        fCoinControlFeatures = value.toBool();
        settings.setValue("fCoinControlFeatures", fCoinControlFeatures);
        Q_EMIT coinControlFeaturesChanged(fCoinControlFeatures);
        break;
    case EnablePSBTControls:
        m_enable_psbt_controls = value.toBool();
        settings.setValue("enable_psbt_controls", m_enable_psbt_controls);
        break;
    case Prune:
        if (settings.value("bPrune") != value) {
            settings.setValue("bPrune", value);
            setRestartRequired(true);
        }
        break;
    case PruneSize:
        if (settings.value("nPruneSize") != value) {
            settings.setValue("nPruneSize", value);
            setRestartRequired(true);
        }
        break;
    case DatabaseCache:
        if (changed()) {
            update(static_cast<int64_t>(value.toLongLong()));
            setRestartRequired(true);
        }
        break;
    case ThreadsScriptVerif:
        if (changed()) {
            update(static_cast<int64_t>(value.toLongLong()));
            setRestartRequired(true);
        }
        break;
    case Listen:
        if (settings.value("fListen") != value) {
            settings.setValue("fListen", value);
            setRestartRequired(true);
        }
        break;
    case Server:
        if (settings.value("server") != value) {
            settings.setValue("server", value);
            setRestartRequired(true);
        }
        break;
    default:
        break;
    }

    return successful;
}

void OptionsModel::setDisplayUnit(const QVariant& new_unit)
{
    if (new_unit.isNull() || new_unit.value<BitcoinUnit>() == m_display_bitcoin_unit) return;
    m_display_bitcoin_unit = new_unit.value<BitcoinUnit>();
    QSettings settings;
    settings.setValue("DisplayBitcoinUnit", QVariant::fromValue(m_display_bitcoin_unit));
    Q_EMIT displayUnitChanged(m_display_bitcoin_unit);
}

void OptionsModel::setRestartRequired(bool fRequired)
{
    QSettings settings;
    return settings.setValue("fRestartRequired", fRequired);
}

bool OptionsModel::isRestartRequired() const
{
    QSettings settings;
    return settings.value("fRestartRequired", false).toBool();
}

void OptionsModel::checkAndMigrate()
{
    // Migration of default values
    // Check if the QSettings container was already loaded with this client version
    QSettings settings;
    static const char strSettingsVersionKey[] = "nSettingsVersion";
    int settingsVersion = settings.contains(strSettingsVersionKey) ? settings.value(strSettingsVersionKey).toInt() : 0;
    if (settingsVersion < CLIENT_VERSION)
    {
        // -dbcache was bumped from 100 to 300 in 0.13
        // see https://github.com/bitcoin/bitcoin/pull/8273
        // force people to upgrade to the new value if they are using 100MB
        if (settingsVersion < 130000 && settings.contains("nDatabaseCache") && settings.value("nDatabaseCache").toLongLong() == 100)
            settings.setValue("nDatabaseCache", (qint64)nDefaultDbCache);

        settings.setValue(strSettingsVersionKey, CLIENT_VERSION);
    }

    // Overwrite the 'addrProxy' setting in case it has been set to an illegal
    // default value (see issue #12623; PR #12650).
    if (settings.contains("addrProxy") && settings.value("addrProxy").toString().endsWith("%2")) {
        settings.setValue("addrProxy", GetDefaultProxyAddress());
    }

    // Overwrite the 'addrSeparateProxyTor' setting in case it has been set to an illegal
    // default value (see issue #12623; PR #12650).
    if (settings.contains("addrSeparateProxyTor") && settings.value("addrSeparateProxyTor").toString().endsWith("%2")) {
        settings.setValue("addrSeparateProxyTor", GetDefaultProxyAddress());
    }

    // Migrate and delete legacy GUI settings that have now moved to <datadir>/settings.json.
    auto migrate_setting = [&](OptionID option, const QString& qt_name) {
        if (!settings.contains(qt_name)) return;
        QVariant value = settings.value(qt_name);
        if (node().getPersistentSetting(SettingName(option)).isNull()) {
            setOption(option, value);
        }
        settings.remove(qt_name);
    };

    migrate_setting(DatabaseCache, "nDatabaseCache");
    migrate_setting(ThreadsScriptVerif, "nThreadsScriptVerif");
#ifdef ENABLE_WALLET
    migrate_setting(SpendZeroConfChange, "bSpendZeroConfChange");
    migrate_setting(ExternalSignerPath, "external_signer_path");
#endif
    migrate_setting(MapPortUPnP, "fUseUPnP");
    migrate_setting(MapPortNatpmp, "fUseNatpmp");
}
