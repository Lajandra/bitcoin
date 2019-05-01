// Copyright (c) 2011-2018 The Bitcoin Core developers
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
#include <validation.h> // For DEFAULT_SCRIPTCHECK_THREADS
#include <net.h>
#include <netbase.h>
#include <txdb.h> // for -dbcache defaults

#include <QNetworkProxy>
#include <QSettings>
#include <QStringList>

#include <univalue.h>

const char *DEFAULT_GUI_PROXY_HOST = "127.0.0.1";

static const QString GetDefaultProxyAddress();

//! Convert QSettings QVariant value to bitcoin setting.
static util::SettingsValue ToSetting(const QVariant& variant, const util::SettingsValue& fallback = {})
{
    if (!variant.isValid()) return fallback;
    if (variant.type() == QVariant::Bool) return variant.toBool();
    if (variant.type() == QVariant::Int) return variant.toInt();
    return variant.toString().toStdString();
}

//! Convert bitcoin setting to QVariant.
static QVariant ToVariant(const util::SettingsValue& value, const QVariant& fallback = {})
{
    if (value.isNull()) return fallback;
    if (value.isBool()) return value.get_bool();
    if (value.isNum()) return value.get_int();
    return QString::fromStdString(value.get_str());
}

//! Convert bitcoin settings value to integer.
static int ToInt(const util::SettingsValue& value, int fallback = 0)
{
    if (value.isNull()) return fallback;
    if (value.isBool()) return value.get_bool();
    if (value.isNum()) return value.get_int();
    return std::atoi(value.get_str().c_str());
}

//! Convert bitcoin settings value to QString.
static QString ToQString(const util::SettingsValue& value, const QString& fallback = {})
{
    if (value.isNull()) return fallback;
    return QString::fromStdString(value.get_str());
}

//! Get pruning enabled value to show in GUI from bitcoin -prune setting.
static bool PruneEnabled(const util::SettingsValue& prune_setting)
{
    // -prune=1 setting is manual pruning mode, so disabled for purposes of the gui
    return ToInt(prune_setting) > 1;
}

//! Get pruning size value to show in GUI from bitcoin -prune setting. If
//! pruning is not enabled, just show default recommended pruning size (2GB).
static int PruneSizeGB(const util::SettingsValue& prune_setting)
{
    int value = ToInt(prune_setting);
    return value > 1 ? PruneMiBtoGB(value) : 2;
}

//! Convert enabled/size values to bitcoin -prune setting.
static util::SettingsValue PruneSetting(bool prune_enabled, int prune_size_gb)
{
    assert(prune_size_gb >= 1); // PruneSizeGB and ParsePruneSizeGB never return less
    return prune_enabled ? PruneGBtoMiB(prune_size_gb) : 0;
}

//! Interpret pruning size value provided by user in GUI or loaded from a legacy
//! QSettings source (windows registry key or qt .conf file). Smallest value
//! that the GUI can display is 1 GB, so round up if anything less is parsed.
static int ParsePruneSizeGB(const QVariant& prune_size) { return std::max(1, prune_size.toInt()); }

struct ProxySetting {
    bool is_set;
    QString ip;
    QString port;
};
static ProxySetting ParseProxyString(const QString& proxy);
static QString ProxyString(bool is_set, QString ip, QString port);

OptionsModel::OptionsModel(interfaces::Node& node, QObject *parent, bool resetSettings) :
    QAbstractListModel(parent), m_node(node)
{
    Init(resetSettings);
}

void OptionsModel::addOverriddenOption(const std::string &option)
{
    strOverriddenByCommandLine += QString::fromStdString(option) + "=" + QString::fromStdString(gArgs.GetArg(option, "")) + " ";
}

// Writes all missing QSettings with their default values
void OptionsModel::Init(bool resetSettings)
{
    if (resetSettings)
        Reset();

    // Initialize display settings from stored settings.
    m_prune_size_gb = PruneSizeGB(m_node.getPersistentSetting("prune"));
    ProxySetting proxy = ParseProxyString(ToQString(m_node.getPersistentSetting("proxy")));
    m_proxy_ip = proxy.ip;
    m_proxy_port = proxy.port;
    ProxySetting onion = ParseProxyString(ToQString(m_node.getPersistentSetting("onion")));
    m_onion_ip = onion.ip;
    m_onion_port = onion.port;
    language = ToQString(m_node.getPersistentSetting("language"));

    checkAndMigrate();

    QSettings settings;

    // Ensure restart flag is unset on client startup
    setRestartRequired(false);

    // These are Qt-only settings:

    // Window
    if (!settings.contains("fHideTrayIcon"))
        settings.setValue("fHideTrayIcon", false);
    fHideTrayIcon = settings.value("fHideTrayIcon").toBool();
    Q_EMIT hideTrayIconChanged(fHideTrayIcon);

    if (!settings.contains("fMinimizeToTray"))
        settings.setValue("fMinimizeToTray", false);
    fMinimizeToTray = settings.value("fMinimizeToTray").toBool() && !fHideTrayIcon;

    if (!settings.contains("fMinimizeOnClose"))
        settings.setValue("fMinimizeOnClose", false);
    fMinimizeOnClose = settings.value("fMinimizeOnClose").toBool();

    // Display
    if (!settings.contains("nDisplayUnit"))
        settings.setValue("nDisplayUnit", BitcoinUnits::BTC);
    nDisplayUnit = settings.value("nDisplayUnit").toInt();

    if (!settings.contains("strThirdPartyTxUrls"))
        settings.setValue("strThirdPartyTxUrls", "");
    strThirdPartyTxUrls = settings.value("strThirdPartyTxUrls", "").toString();

    if (!settings.contains("fCoinControlFeatures"))
        settings.setValue("fCoinControlFeatures", false);
    fCoinControlFeatures = settings.value("fCoinControlFeatures", false).toBool();

    // These are shared with the core or have a command-line parameter
    // and we want command-line parameters to overwrite the GUI settings.
    if (m_node.isSettingIgnored("prune")) addOverriddenOption("-prune");
    if (m_node.isSettingIgnored("dbcache")) addOverriddenOption("-dbcache");
    if (m_node.isSettingIgnored("par")) addOverriddenOption("-par");
    if (m_node.isSettingIgnored("spendzeroconfchange")) addOverriddenOption("-spendzeroconfchange");
    if (m_node.isSettingIgnored("upnp")) addOverriddenOption("-upnp");
    if (m_node.isSettingIgnored("listen")) addOverriddenOption("-listen");
    if (m_node.isSettingIgnored("proxy")) addOverriddenOption("-proxy");
    if (m_node.isSettingIgnored("onion")) addOverriddenOption("-onion");
    if (m_node.isSettingIgnored("language")) addOverriddenOption("-language");

    // If setting doesn't exist create it with defaults.
    if (!settings.contains("strDataDir"))
        settings.setValue("strDataDir", GUIUtil::getDefaultDataDirectory());
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
    qWarning() << "Backing up GUI settings to" << GUIUtil::boostPathToQString(filename);
    QSettings dst(GUIUtil::boostPathToQString(filename), QSettings::IniFormat);
    dst.clear();
    CopySettings(dst, src);
}

void OptionsModel::Reset()
{
    QSettings settings;

    // Backup old settings to chain-specific datadir for troubleshooting
    BackupSettings(GetDataDir(true) / "guisettings.ini.bak", settings);

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

static ProxySetting ParseProxyString(const QString& proxy)
{
    static const ProxySetting default_val = {false, DEFAULT_GUI_PROXY_HOST, QString("%1").arg(DEFAULT_GUI_PROXY_PORT)};
    // Handle the case that the setting is not set at all
    if (proxy.isEmpty()) {
        return default_val;
    }
    // contains IP at index 0 and port at index 1
    QStringList ip_port = proxy.split(":", QString::SkipEmptyParts);
    if (ip_port.size() == 2) {
        return {true, ip_port.at(0), ip_port.at(1)};
    } else { // Invalid: return default
        return default_val;
    }
}

static QString ProxyString(bool is_set, QString ip, QString port)
{
    return is_set ? ip + ":" + port : "";
}

static const QString GetDefaultProxyAddress()
{
    return QString("%1:%2").arg(DEFAULT_GUI_PROXY_HOST).arg(DEFAULT_GUI_PROXY_PORT);
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
    QSettings settings;
    switch (option) {
    case StartAtStartup:
        return GUIUtil::GetStartOnSystemStartup();
    case HideTrayIcon:
        return fHideTrayIcon;
    case MinimizeToTray:
        return fMinimizeToTray;
    case MapPortUPnP:
#ifdef USE_UPNP
        return settings.value("fUseUPnP");
#else
        return false;
#endif
    case MinimizeOnClose:
        return fMinimizeOnClose;

    // default proxy
    case ProxyUse:
        return !ToQString(m_node.getPersistentSetting("proxy")).isEmpty();
    case ProxyIP:
        return m_proxy_ip;
    case ProxyPort:
        return m_proxy_port;

    // separate Tor proxy
    case ProxyUseTor:
        return !ToQString(m_node.getPersistentSetting("onion")).isEmpty();
    case ProxyIPTor:
        return m_onion_ip;
    case ProxyPortTor:
        return m_onion_port;

#ifdef ENABLE_WALLET
    case SpendZeroConfChange:
        return ToVariant(m_node.getPersistentSetting("spendzeroconfchange"), true);
#endif
    case DisplayUnit:
        return nDisplayUnit;
    case ThirdPartyTxUrls:
        return strThirdPartyTxUrls;
    case Language:
        return ToVariant(m_node.getPersistentSetting("language"), "");
    case CoinControlFeatures:
        return fCoinControlFeatures;
    case Prune:
        return PruneEnabled(m_node.getPersistentSetting("prune"));
    case PruneSize:
        return m_prune_size_gb;
    case DatabaseCache:
        return ToVariant(m_node.getPersistentSetting("dbcache"), (qint64)nDefaultDbCache);
    case ThreadsScriptVerif:
        return ToVariant(m_node.getPersistentSetting("par"), DEFAULT_SCRIPTCHECK_THREADS);
    case Listen:
        return ToVariant(m_node.getPersistentSetting("listen"), DEFAULT_LISTEN);
    default:
        return QVariant();
    }
}

bool OptionsModel::setOption(OptionID option, const QVariant& value)
{
    auto changed = [&] { return value.isValid() && value != getOption(option); };

    bool successful = true; /* set to false on parse error */
    QSettings settings;

    switch (option) {
    case StartAtStartup:
        successful = GUIUtil::SetStartOnSystemStartup(value.toBool());
        break;
    case HideTrayIcon:
        fHideTrayIcon = value.toBool();
        settings.setValue("fHideTrayIcon", fHideTrayIcon);
        Q_EMIT hideTrayIconChanged(fHideTrayIcon);
        break;
    case MinimizeToTray:
        fMinimizeToTray = value.toBool();
        settings.setValue("fMinimizeToTray", fMinimizeToTray);
        break;
    case MapPortUPnP: // core option - can be changed on-the-fly
        if (changed()) {
            m_node.updateSetting("upnp", ToSetting(value));
            m_node.mapPort(value.toBool());
        }
        break;
    case MinimizeOnClose:
        fMinimizeOnClose = value.toBool();
        settings.setValue("fMinimizeOnClose", fMinimizeOnClose);
        break;

    // default proxy
    case ProxyUse:
        if (changed()) {
            m_node.updateSetting("proxy", ProxyString(value.toBool(), m_proxy_ip, m_proxy_port).toStdString());
            setRestartRequired(true);
        }
        break;
    case ProxyIP:
        if (changed()) {
            m_proxy_ip = value.toString();
            if (getOption(ProxyUse).toBool()) {
                m_node.updateSetting("proxy", ProxyString(true, m_proxy_ip, m_proxy_port).toStdString());
                setRestartRequired(true);
            }
        }
        break;
    case ProxyPort:
        if (changed()) {
            m_proxy_port = value.toString();
            if (getOption(ProxyUse).toBool()) {
                m_node.updateSetting("proxy", ProxyString(true, m_proxy_ip, m_proxy_port).toStdString());
                setRestartRequired(true);
            }
        }
        break;

    // separate Tor proxy
    case ProxyUseTor:
        if (changed()) {
            m_node.updateSetting("onion", ProxyString(value.toBool(), m_onion_ip, m_onion_port).toStdString());
            setRestartRequired(true);
        }
        break;
    case ProxyIPTor:
        if (changed()) {
            m_onion_ip = value.toString();
            if (getOption(ProxyUseTor).toBool()) {
                m_node.updateSetting("onion", ProxyString(true, m_onion_ip, m_onion_port).toStdString());
                setRestartRequired(true);
            }
        }
        break;
    case ProxyPortTor:
        if (changed()) {
            m_onion_port = value.toString();
            if (getOption(ProxyUseTor).toBool()) {
                m_node.updateSetting("onion", ProxyString(true, m_onion_ip, m_onion_port).toStdString());
                setRestartRequired(true);
            }
        }
        break;

#ifdef ENABLE_WALLET
    case SpendZeroConfChange:
        if (changed()) {
            m_node.updateSetting("spendzeroconfchange", ToSetting(value));
            setRestartRequired(true);
        }
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
        if (changed()) {
            m_node.updateSetting("language", ToSetting(value));
            setRestartRequired(true);
        }
        break;
    case CoinControlFeatures:
        fCoinControlFeatures = value.toBool();
        settings.setValue("fCoinControlFeatures", fCoinControlFeatures);
        Q_EMIT coinControlFeaturesChanged(fCoinControlFeatures);
        break;
    case Prune:
        if (changed()) {
            m_node.updateSetting("prune", PruneSetting(value.toBool(), m_prune_size_gb));
            setRestartRequired(true);
        }
        break;
    case PruneSize:
        if (changed()) {
            m_prune_size_gb = ParsePruneSizeGB(value);
            if (getOption(Prune).toBool()) {
                m_node.updateSetting("prune", PruneSetting(true, m_prune_size_gb));
                setRestartRequired(true);
            }
        }
        break;
    case DatabaseCache:
        if (changed()) {
            m_node.updateSetting("dbcache", ToSetting(value));
            setRestartRequired(true);
        }
        break;
    case ThreadsScriptVerif:
        if (changed()) {
            m_node.updateSetting("par", ToSetting(value));
            setRestartRequired(true);
        }
        break;
    case Listen:
        if (changed()) {
            m_node.updateSetting("listen", ToSetting(value));
            setRestartRequired(true);
        }
        break;
    default:
        break;
    }
    return successful;
}

/** Updates current unit in memory, settings and emits displayUnitChanged(newUnit) signal */
void OptionsModel::setDisplayUnit(const QVariant &value)
{
    if (!value.isNull())
    {
        QSettings settings;
        nDisplayUnit = value.toInt();
        settings.setValue("nDisplayUnit", nDisplayUnit);
        Q_EMIT displayUnitChanged(nDisplayUnit);
    }
}

bool OptionsModel::getProxySettings(QNetworkProxy& proxy) const
{
    // Directly query current base proxy, because
    // GUI settings can be overridden with -proxy.
    proxyType curProxy;
    if (m_node.getProxy(NET_IPV4, curProxy)) {
        proxy.setType(QNetworkProxy::Socks5Proxy);
        proxy.setHostName(QString::fromStdString(curProxy.proxy.ToStringIP()));
        proxy.setPort(curProxy.proxy.GetPort());

        return true;
    }
    else
        proxy.setType(QNetworkProxy::NoProxy);

    return false;
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
    auto migrate_setting = [&](OptionID option, const QString& qt_name, const std::string& name = {}) {
        bool updated = false;
        if (settings.contains(qt_name)) {
            if (name.empty() || m_node.getPersistentSetting(name).isNull()) {
                if (option == ProxyIP || option == ProxyIPTor) {
                    ProxySetting proxy = ParseProxyString(settings.value(qt_name).toString());
                    setOption(option, proxy.ip);
                    setOption(option == ProxyIP ? ProxyPort : ProxyPortTor, proxy.port);
                } else {
                    setOption(option, settings.value(qt_name));
                }
                updated = true;
            }
            settings.remove(qt_name);
        }
        return updated;
    };

    migrate_setting(DatabaseCache, "nDatabaseCache", "dbcache");
    migrate_setting(ThreadsScriptVerif, "nThreadsScriptVerif", "par");
#ifdef ENABLE_WALLET
    migrate_setting(SpendZeroConfChange, "bSpendZeroConfChange", "spendzeroconfchange");
#endif
    migrate_setting(MapPortUPnP, "fUseUPnP", "upnp");
    migrate_setting(Listen, "fListen", "listen");
    migrate_setting(Prune, "bPrune", "prune") && migrate_setting(PruneSize, "nPruneSize");
    migrate_setting(ProxyUse, "fUseProxy", "proxy") && migrate_setting(ProxyIP, "addrProxy");
    migrate_setting(ProxyUseTor, "fUseSeparateProxyTor", "onion") && migrate_setting(ProxyIPTor, "addrSeparateProxyTor");
}
