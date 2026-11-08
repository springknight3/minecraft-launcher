/****************************************************************************
** Meta object code from reading C++ file 'LauncherController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.12.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../../cpp/ui/LauncherController.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'LauncherController.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.12.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN6strata2ui18LauncherControllerE_t {};
} // unnamed namespace

template <> constexpr inline auto strata::ui::LauncherController::qt_create_metaobjectdata<qt_meta_tag_ZN6strata2ui18LauncherControllerE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "strata::ui::LauncherController",
        "versionsChanged",
        "",
        "loaderVersionsChanged",
        "instancesChanged",
        "lastLaunchedChanged",
        "launchTrackingChanged",
        "deletingInstanceChanged",
        "installationChanged",
        "initializationChanged",
        "errorMessageChanged",
        "installFinished",
        "success",
        "instanceName",
        "error",
        "offlineAccountChanged",
        "accountsChanged",
        "microsoftAuthChanged",
        "consoleTextChanged",
        "consoleInstanceChanged",
        "processStateChanged",
        "instanceId",
        "state",
        "runtimesChanged",
        "runtimePreferencesChanged",
        "globalJavaArgumentsChanged",
        "runtimeInstallationChanged",
        "serversChanged",
        "serverContentChanged",
        "refresh",
        "loadLoaderVersions",
        "loaderType",
        "minecraftVersion",
        "installInstance",
        "name",
        "versionId",
        "installInstanceWithLoader",
        "loaderVersion",
        "saveOfflineUsername",
        "addOfflineAccount",
        "selectAccount",
        "accountId",
        "removeAccount",
        "setMicrosoftClientId",
        "clientId",
        "signInMicrosoft",
        "cancelMicrosoftSignIn",
        "launchInstance",
        "deleteInstance",
        "setTrackInstanceLaunches",
        "track",
        "stopInstance",
        "stopConsoleInstance",
        "openConsole",
        "closeConsole",
        "setRuntimePreferences",
        "vendor",
        "redundant",
        "setGlobalJavaArguments",
        "flags",
        "installInstanceRuntime",
        "useSystemJava",
        "refreshRuntimes",
        "deleteRuntime",
        "directory",
        "refreshServers",
        "createServer",
        "version",
        "distribution",
        "updateServerSettings",
        "id",
        "eulaAccepted",
        "javaFlags",
        "viewDistance",
        "onlineMode",
        "useManagedJavaRuntime",
        "setServerEulaAccepted",
        "accepted",
        "loadServerProperties",
        "saveServerProperties",
        "contents",
        "deleteServer",
        "selectServer",
        "installServer",
        "installServerRuntime",
        "startServer",
        "stopServer",
        "terminateServer",
        "sendServerCommand",
        "command",
        "searchServerContent",
        "query",
        "loadServerContentVersions",
        "projectId",
        "installServerContent",
        "versions",
        "QVariantList",
        "contentController",
        "loaderVersions",
        "loadingLoaderVersions",
        "loaderVersionsError",
        "instances",
        "lastLaunchedInstanceId",
        "lastLaunchedInstanceName",
        "recentLaunchedInstances",
        "trackInstanceLaunches",
        "latestRelease",
        "installing",
        "initializing",
        "currentInstallName",
        "installationProgress",
        "currentInstallItem",
        "errorMessage",
        "usingCachedManifest",
        "offlineUsername",
        "accounts",
        "activeAccountName",
        "activeAccountId",
        "microsoftClientId",
        "microsoftUserCode",
        "microsoftVerificationUrl",
        "microsoftAuthInProgress",
        "consoleText",
        "consoleInstanceName",
        "installedRuntimes",
        "instanceSettingsController",
        "runtimeVendor",
        "redundantRuntimes",
        "globalJavaArguments",
        "recommendedJavaArguments",
        "installingRuntime",
        "deletingInstance",
        "runtimeInstallationProgress",
        "runtimeInstallationItem",
        "servers",
        "serverBusy",
        "serverStatus",
        "serverProperties",
        "serverProgress",
        "serverInstallItem",
        "serverConsoleText",
        "serverProcessState",
        "serverResourceStatus",
        "serverContentProjects",
        "serverContentVersions",
        "serverContentStatus",
        "serverContentBusy"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'versionsChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'loaderVersionsChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'instancesChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'lastLaunchedChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'launchTrackingChanged'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'deletingInstanceChanged'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'installationChanged'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'initializationChanged'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'errorMessageChanged'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'installFinished'
        QtMocHelpers::SignalData<void(bool, const QString &, const QString &)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 12 }, { QMetaType::QString, 13 }, { QMetaType::QString, 14 },
        }}),
        // Signal 'offlineAccountChanged'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'accountsChanged'
        QtMocHelpers::SignalData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'microsoftAuthChanged'
        QtMocHelpers::SignalData<void()>(17, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'consoleTextChanged'
        QtMocHelpers::SignalData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'consoleInstanceChanged'
        QtMocHelpers::SignalData<void()>(19, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'processStateChanged'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 }, { QMetaType::QString, 22 },
        }}),
        // Signal 'runtimesChanged'
        QtMocHelpers::SignalData<void()>(23, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'runtimePreferencesChanged'
        QtMocHelpers::SignalData<void()>(24, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'globalJavaArgumentsChanged'
        QtMocHelpers::SignalData<void()>(25, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'runtimeInstallationChanged'
        QtMocHelpers::SignalData<void()>(26, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'serversChanged'
        QtMocHelpers::SignalData<void()>(27, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'serverContentChanged'
        QtMocHelpers::SignalData<void()>(28, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'refresh'
        QtMocHelpers::MethodData<void()>(29, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'loadLoaderVersions'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(30, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 31 }, { QMetaType::QString, 32 },
        }}),
        // Method 'installInstance'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(33, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 34 }, { QMetaType::QString, 35 },
        }}),
        // Method 'installInstanceWithLoader'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QString &, const QString &)>(36, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 34 }, { QMetaType::QString, 35 }, { QMetaType::QString, 31 }, { QMetaType::QString, 37 },
        }}),
        // Method 'saveOfflineUsername'
        QtMocHelpers::MethodData<void(const QString &)>(38, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 34 },
        }}),
        // Method 'addOfflineAccount'
        QtMocHelpers::MethodData<void(const QString &)>(39, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 34 },
        }}),
        // Method 'selectAccount'
        QtMocHelpers::MethodData<void(const QString &)>(40, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 },
        }}),
        // Method 'removeAccount'
        QtMocHelpers::MethodData<void(const QString &)>(42, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 41 },
        }}),
        // Method 'setMicrosoftClientId'
        QtMocHelpers::MethodData<void(const QString &)>(43, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 44 },
        }}),
        // Method 'signInMicrosoft'
        QtMocHelpers::MethodData<void(const QString &)>(45, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 44 },
        }}),
        // Method 'cancelMicrosoftSignIn'
        QtMocHelpers::MethodData<void()>(46, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'launchInstance'
        QtMocHelpers::MethodData<void(const QString &)>(47, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 },
        }}),
        // Method 'deleteInstance'
        QtMocHelpers::MethodData<void(const QString &)>(48, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 },
        }}),
        // Method 'setTrackInstanceLaunches'
        QtMocHelpers::MethodData<void(bool)>(49, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 50 },
        }}),
        // Method 'stopInstance'
        QtMocHelpers::MethodData<void(const QString &)>(51, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 },
        }}),
        // Method 'stopConsoleInstance'
        QtMocHelpers::MethodData<void()>(52, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'openConsole'
        QtMocHelpers::MethodData<void(const QString &)>(53, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 },
        }}),
        // Method 'closeConsole'
        QtMocHelpers::MethodData<void()>(54, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'setRuntimePreferences'
        QtMocHelpers::MethodData<void(const QString &, bool)>(55, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 56 }, { QMetaType::Bool, 57 },
        }}),
        // Method 'setGlobalJavaArguments'
        QtMocHelpers::MethodData<void(const QString &)>(58, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 59 },
        }}),
        // Method 'installInstanceRuntime'
        QtMocHelpers::MethodData<void(const QString &)>(60, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 },
        }}),
        // Method 'useSystemJava'
        QtMocHelpers::MethodData<void(const QString &)>(61, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 },
        }}),
        // Method 'refreshRuntimes'
        QtMocHelpers::MethodData<void()>(62, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'deleteRuntime'
        QtMocHelpers::MethodData<void(const QString &)>(63, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 64 },
        }}),
        // Method 'refreshServers'
        QtMocHelpers::MethodData<void()>(65, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'createServer'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QString &)>(66, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 34 }, { QMetaType::QString, 67 }, { QMetaType::QString, 68 },
        }}),
        // Method 'updateServerSettings'
        QtMocHelpers::MethodData<void(const QString &, const QString &, bool, const QString &, int, bool, bool)>(69, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 }, { QMetaType::QString, 34 }, { QMetaType::Bool, 71 }, { QMetaType::QString, 72 },
            { QMetaType::Int, 73 }, { QMetaType::Bool, 74 }, { QMetaType::Bool, 75 },
        }}),
        // Method 'setServerEulaAccepted'
        QtMocHelpers::MethodData<void(const QString &, bool)>(76, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 }, { QMetaType::Bool, 77 },
        }}),
        // Method 'loadServerProperties'
        QtMocHelpers::MethodData<void(const QString &)>(78, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 },
        }}),
        // Method 'saveServerProperties'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(79, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 }, { QMetaType::QString, 80 },
        }}),
        // Method 'deleteServer'
        QtMocHelpers::MethodData<void(const QString &)>(81, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 },
        }}),
        // Method 'selectServer'
        QtMocHelpers::MethodData<void(const QString &)>(82, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 },
        }}),
        // Method 'installServer'
        QtMocHelpers::MethodData<void(const QString &)>(83, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 },
        }}),
        // Method 'installServerRuntime'
        QtMocHelpers::MethodData<void(const QString &)>(84, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 },
        }}),
        // Method 'startServer'
        QtMocHelpers::MethodData<void(const QString &)>(85, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 },
        }}),
        // Method 'stopServer'
        QtMocHelpers::MethodData<void(const QString &)>(86, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 },
        }}),
        // Method 'terminateServer'
        QtMocHelpers::MethodData<void(const QString &)>(87, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 },
        }}),
        // Method 'sendServerCommand'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(88, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 }, { QMetaType::QString, 89 },
        }}),
        // Method 'searchServerContent'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(90, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 }, { QMetaType::QString, 91 },
        }}),
        // Method 'loadServerContentVersions'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(92, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 }, { QMetaType::QString, 93 },
        }}),
        // Method 'installServerContent'
        QtMocHelpers::MethodData<void(const QString &, const QString &, const QString &)>(94, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 70 }, { QMetaType::QString, 93 }, { QMetaType::QString, 35 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'versions'
        QtMocHelpers::PropertyData<QVariantList>(95, 0x80000000 | 96, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'contentController'
        QtMocHelpers::PropertyData<QObject*>(97, QMetaType::QObjectStar, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'loaderVersions'
        QtMocHelpers::PropertyData<QStringList>(98, QMetaType::QStringList, QMC::DefaultPropertyFlags, 1),
        // property 'loadingLoaderVersions'
        QtMocHelpers::PropertyData<bool>(99, QMetaType::Bool, QMC::DefaultPropertyFlags, 1),
        // property 'loaderVersionsError'
        QtMocHelpers::PropertyData<QString>(100, QMetaType::QString, QMC::DefaultPropertyFlags, 1),
        // property 'instances'
        QtMocHelpers::PropertyData<QVariantList>(101, 0x80000000 | 96, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'lastLaunchedInstanceId'
        QtMocHelpers::PropertyData<QString>(102, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
        // property 'lastLaunchedInstanceName'
        QtMocHelpers::PropertyData<QString>(103, QMetaType::QString, QMC::DefaultPropertyFlags, 2),
        // property 'recentLaunchedInstances'
        QtMocHelpers::PropertyData<QVariantList>(104, 0x80000000 | 96, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 3),
        // property 'trackInstanceLaunches'
        QtMocHelpers::PropertyData<bool>(105, QMetaType::Bool, QMC::DefaultPropertyFlags, 4),
        // property 'latestRelease'
        QtMocHelpers::PropertyData<QString>(106, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'installing'
        QtMocHelpers::PropertyData<bool>(107, QMetaType::Bool, QMC::DefaultPropertyFlags, 6),
        // property 'initializing'
        QtMocHelpers::PropertyData<bool>(108, QMetaType::Bool, QMC::DefaultPropertyFlags, 7),
        // property 'currentInstallName'
        QtMocHelpers::PropertyData<QString>(109, QMetaType::QString, QMC::DefaultPropertyFlags, 6),
        // property 'installationProgress'
        QtMocHelpers::PropertyData<double>(110, QMetaType::Double, QMC::DefaultPropertyFlags, 6),
        // property 'currentInstallItem'
        QtMocHelpers::PropertyData<QString>(111, QMetaType::QString, QMC::DefaultPropertyFlags, 6),
        // property 'errorMessage'
        QtMocHelpers::PropertyData<QString>(112, QMetaType::QString, QMC::DefaultPropertyFlags, 8),
        // property 'usingCachedManifest'
        QtMocHelpers::PropertyData<bool>(113, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'offlineUsername'
        QtMocHelpers::PropertyData<QString>(114, QMetaType::QString, QMC::DefaultPropertyFlags, 10),
        // property 'accounts'
        QtMocHelpers::PropertyData<QVariantList>(115, 0x80000000 | 96, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 11),
        // property 'activeAccountName'
        QtMocHelpers::PropertyData<QString>(116, QMetaType::QString, QMC::DefaultPropertyFlags, 11),
        // property 'activeAccountId'
        QtMocHelpers::PropertyData<QString>(117, QMetaType::QString, QMC::DefaultPropertyFlags, 11),
        // property 'microsoftClientId'
        QtMocHelpers::PropertyData<QString>(118, QMetaType::QString, QMC::DefaultPropertyFlags, 11),
        // property 'microsoftUserCode'
        QtMocHelpers::PropertyData<QString>(119, QMetaType::QString, QMC::DefaultPropertyFlags, 12),
        // property 'microsoftVerificationUrl'
        QtMocHelpers::PropertyData<QString>(120, QMetaType::QString, QMC::DefaultPropertyFlags, 12),
        // property 'microsoftAuthInProgress'
        QtMocHelpers::PropertyData<bool>(121, QMetaType::Bool, QMC::DefaultPropertyFlags, 12),
        // property 'consoleText'
        QtMocHelpers::PropertyData<QString>(122, QMetaType::QString, QMC::DefaultPropertyFlags, 13),
        // property 'consoleInstanceName'
        QtMocHelpers::PropertyData<QString>(123, QMetaType::QString, QMC::DefaultPropertyFlags, 14),
        // property 'installedRuntimes'
        QtMocHelpers::PropertyData<QVariantList>(124, 0x80000000 | 96, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 16),
        // property 'instanceSettingsController'
        QtMocHelpers::PropertyData<QObject*>(125, QMetaType::QObjectStar, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'runtimeVendor'
        QtMocHelpers::PropertyData<QString>(126, QMetaType::QString, QMC::DefaultPropertyFlags, 17),
        // property 'redundantRuntimes'
        QtMocHelpers::PropertyData<bool>(127, QMetaType::Bool, QMC::DefaultPropertyFlags, 17),
        // property 'globalJavaArguments'
        QtMocHelpers::PropertyData<QStringList>(128, QMetaType::QStringList, QMC::DefaultPropertyFlags, 18),
        // property 'recommendedJavaArguments'
        QtMocHelpers::PropertyData<QStringList>(129, QMetaType::QStringList, QMC::DefaultPropertyFlags | QMC::Constant),
        // property 'installingRuntime'
        QtMocHelpers::PropertyData<bool>(130, QMetaType::Bool, QMC::DefaultPropertyFlags, 19),
        // property 'deletingInstance'
        QtMocHelpers::PropertyData<bool>(131, QMetaType::Bool, QMC::DefaultPropertyFlags, 5),
        // property 'runtimeInstallationProgress'
        QtMocHelpers::PropertyData<double>(132, QMetaType::Double, QMC::DefaultPropertyFlags, 19),
        // property 'runtimeInstallationItem'
        QtMocHelpers::PropertyData<QString>(133, QMetaType::QString, QMC::DefaultPropertyFlags, 19),
        // property 'servers'
        QtMocHelpers::PropertyData<QVariantList>(134, 0x80000000 | 96, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 20),
        // property 'serverBusy'
        QtMocHelpers::PropertyData<bool>(135, QMetaType::Bool, QMC::DefaultPropertyFlags, 20),
        // property 'serverStatus'
        QtMocHelpers::PropertyData<QString>(136, QMetaType::QString, QMC::DefaultPropertyFlags, 20),
        // property 'serverProperties'
        QtMocHelpers::PropertyData<QString>(137, QMetaType::QString, QMC::DefaultPropertyFlags, 20),
        // property 'serverProgress'
        QtMocHelpers::PropertyData<double>(138, QMetaType::Double, QMC::DefaultPropertyFlags, 20),
        // property 'serverInstallItem'
        QtMocHelpers::PropertyData<QString>(139, QMetaType::QString, QMC::DefaultPropertyFlags, 20),
        // property 'serverConsoleText'
        QtMocHelpers::PropertyData<QString>(140, QMetaType::QString, QMC::DefaultPropertyFlags, 20),
        // property 'serverProcessState'
        QtMocHelpers::PropertyData<QString>(141, QMetaType::QString, QMC::DefaultPropertyFlags, 20),
        // property 'serverResourceStatus'
        QtMocHelpers::PropertyData<QString>(142, QMetaType::QString, QMC::DefaultPropertyFlags, 20),
        // property 'serverContentProjects'
        QtMocHelpers::PropertyData<QVariantList>(143, 0x80000000 | 96, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 21),
        // property 'serverContentVersions'
        QtMocHelpers::PropertyData<QVariantList>(144, 0x80000000 | 96, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 21),
        // property 'serverContentStatus'
        QtMocHelpers::PropertyData<QString>(145, QMetaType::QString, QMC::DefaultPropertyFlags, 21),
        // property 'serverContentBusy'
        QtMocHelpers::PropertyData<bool>(146, QMetaType::Bool, QMC::DefaultPropertyFlags, 21),
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<LauncherController, qt_meta_tag_ZN6strata2ui18LauncherControllerE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject strata::ui::LauncherController::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6strata2ui18LauncherControllerE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6strata2ui18LauncherControllerE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6strata2ui18LauncherControllerE_t>.metaTypes,
    nullptr
} };

void strata::ui::LauncherController::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<LauncherController *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->versionsChanged(); break;
        case 1: _t->loaderVersionsChanged(); break;
        case 2: _t->instancesChanged(); break;
        case 3: _t->lastLaunchedChanged(); break;
        case 4: _t->launchTrackingChanged(); break;
        case 5: _t->deletingInstanceChanged(); break;
        case 6: _t->installationChanged(); break;
        case 7: _t->initializationChanged(); break;
        case 8: _t->errorMessageChanged(); break;
        case 9: _t->installFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 10: _t->offlineAccountChanged(); break;
        case 11: _t->accountsChanged(); break;
        case 12: _t->microsoftAuthChanged(); break;
        case 13: _t->consoleTextChanged(); break;
        case 14: _t->consoleInstanceChanged(); break;
        case 15: _t->processStateChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 16: _t->runtimesChanged(); break;
        case 17: _t->runtimePreferencesChanged(); break;
        case 18: _t->globalJavaArgumentsChanged(); break;
        case 19: _t->runtimeInstallationChanged(); break;
        case 20: _t->serversChanged(); break;
        case 21: _t->serverContentChanged(); break;
        case 22: _t->refresh(); break;
        case 23: _t->loadLoaderVersions((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 24: _t->installInstance((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 25: _t->installInstanceWithLoader((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4]))); break;
        case 26: _t->saveOfflineUsername((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 27: _t->addOfflineAccount((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 28: _t->selectAccount((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 29: _t->removeAccount((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 30: _t->setMicrosoftClientId((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 31: _t->signInMicrosoft((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 32: _t->cancelMicrosoftSignIn(); break;
        case 33: _t->launchInstance((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 34: _t->deleteInstance((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 35: _t->setTrackInstanceLaunches((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 36: _t->stopInstance((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 37: _t->stopConsoleInstance(); break;
        case 38: _t->openConsole((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 39: _t->closeConsole(); break;
        case 40: _t->setRuntimePreferences((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 41: _t->setGlobalJavaArguments((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 42: _t->installInstanceRuntime((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 43: _t->useSystemJava((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 44: _t->refreshRuntimes(); break;
        case 45: _t->deleteRuntime((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 46: _t->refreshServers(); break;
        case 47: _t->createServer((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 48: _t->updateServerSettings((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[4])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[5])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[6])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[7]))); break;
        case 49: _t->setServerEulaAccepted((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 50: _t->loadServerProperties((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 51: _t->saveServerProperties((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 52: _t->deleteServer((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 53: _t->selectServer((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 54: _t->installServer((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 55: _t->installServerRuntime((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 56: _t->startServer((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 57: _t->stopServer((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 58: _t->terminateServer((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 59: _t->sendServerCommand((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 60: _t->searchServerContent((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 61: _t->loadServerContentVersions((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 62: _t->installServerContent((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::versionsChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::loaderVersionsChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::instancesChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::lastLaunchedChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::launchTrackingChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::deletingInstanceChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::installationChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::initializationChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::errorMessageChanged, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)(bool , const QString & , const QString & )>(_a, &LauncherController::installFinished, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::offlineAccountChanged, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::accountsChanged, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::microsoftAuthChanged, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::consoleTextChanged, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::consoleInstanceChanged, 14))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)(const QString & , const QString & )>(_a, &LauncherController::processStateChanged, 15))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::runtimesChanged, 16))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::runtimePreferencesChanged, 17))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::globalJavaArgumentsChanged, 18))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::runtimeInstallationChanged, 19))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::serversChanged, 20))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::serverContentChanged, 21))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QVariantList*>(_v) = _t->versions(); break;
        case 1: *reinterpret_cast<QObject**>(_v) = _t->contentController(); break;
        case 2: *reinterpret_cast<QStringList*>(_v) = _t->loaderVersions(); break;
        case 3: *reinterpret_cast<bool*>(_v) = _t->loadingLoaderVersions(); break;
        case 4: *reinterpret_cast<QString*>(_v) = _t->loaderVersionsError(); break;
        case 5: *reinterpret_cast<QVariantList*>(_v) = _t->instances(); break;
        case 6: *reinterpret_cast<QString*>(_v) = _t->lastLaunchedInstanceId(); break;
        case 7: *reinterpret_cast<QString*>(_v) = _t->lastLaunchedInstanceName(); break;
        case 8: *reinterpret_cast<QVariantList*>(_v) = _t->recentLaunchedInstances(); break;
        case 9: *reinterpret_cast<bool*>(_v) = _t->trackInstanceLaunches(); break;
        case 10: *reinterpret_cast<QString*>(_v) = _t->latestRelease(); break;
        case 11: *reinterpret_cast<bool*>(_v) = _t->installing(); break;
        case 12: *reinterpret_cast<bool*>(_v) = _t->initializing(); break;
        case 13: *reinterpret_cast<QString*>(_v) = _t->currentInstallName(); break;
        case 14: *reinterpret_cast<double*>(_v) = _t->installationProgress(); break;
        case 15: *reinterpret_cast<QString*>(_v) = _t->currentInstallItem(); break;
        case 16: *reinterpret_cast<QString*>(_v) = _t->errorMessage(); break;
        case 17: *reinterpret_cast<bool*>(_v) = _t->usingCachedManifest(); break;
        case 18: *reinterpret_cast<QString*>(_v) = _t->offlineUsername(); break;
        case 19: *reinterpret_cast<QVariantList*>(_v) = _t->accounts(); break;
        case 20: *reinterpret_cast<QString*>(_v) = _t->activeAccountName(); break;
        case 21: *reinterpret_cast<QString*>(_v) = _t->activeAccountId(); break;
        case 22: *reinterpret_cast<QString*>(_v) = _t->microsoftClientId(); break;
        case 23: *reinterpret_cast<QString*>(_v) = _t->microsoftUserCode(); break;
        case 24: *reinterpret_cast<QString*>(_v) = _t->microsoftVerificationUrl(); break;
        case 25: *reinterpret_cast<bool*>(_v) = _t->microsoftAuthInProgress(); break;
        case 26: *reinterpret_cast<QString*>(_v) = _t->consoleText(); break;
        case 27: *reinterpret_cast<QString*>(_v) = _t->consoleInstanceName(); break;
        case 28: *reinterpret_cast<QVariantList*>(_v) = _t->installedRuntimes(); break;
        case 29: *reinterpret_cast<QObject**>(_v) = _t->instanceSettingsController(); break;
        case 30: *reinterpret_cast<QString*>(_v) = _t->runtimeVendor(); break;
        case 31: *reinterpret_cast<bool*>(_v) = _t->redundantRuntimes(); break;
        case 32: *reinterpret_cast<QStringList*>(_v) = _t->globalJavaArguments(); break;
        case 33: *reinterpret_cast<QStringList*>(_v) = _t->recommendedJavaArguments(); break;
        case 34: *reinterpret_cast<bool*>(_v) = _t->installingRuntime(); break;
        case 35: *reinterpret_cast<bool*>(_v) = _t->deletingInstance(); break;
        case 36: *reinterpret_cast<double*>(_v) = _t->runtimeInstallationProgress(); break;
        case 37: *reinterpret_cast<QString*>(_v) = _t->runtimeInstallationItem(); break;
        case 38: *reinterpret_cast<QVariantList*>(_v) = _t->servers(); break;
        case 39: *reinterpret_cast<bool*>(_v) = _t->serverBusy(); break;
        case 40: *reinterpret_cast<QString*>(_v) = _t->serverStatus(); break;
        case 41: *reinterpret_cast<QString*>(_v) = _t->serverProperties(); break;
        case 42: *reinterpret_cast<double*>(_v) = _t->serverProgress(); break;
        case 43: *reinterpret_cast<QString*>(_v) = _t->serverInstallItem(); break;
        case 44: *reinterpret_cast<QString*>(_v) = _t->serverConsoleText(); break;
        case 45: *reinterpret_cast<QString*>(_v) = _t->serverProcessState(); break;
        case 46: *reinterpret_cast<QString*>(_v) = _t->serverResourceStatus(); break;
        case 47: *reinterpret_cast<QVariantList*>(_v) = _t->serverContentProjects(); break;
        case 48: *reinterpret_cast<QVariantList*>(_v) = _t->serverContentVersions(); break;
        case 49: *reinterpret_cast<QString*>(_v) = _t->serverContentStatus(); break;
        case 50: *reinterpret_cast<bool*>(_v) = _t->serverContentBusy(); break;
        default: break;
        }
    }
}

const QMetaObject *strata::ui::LauncherController::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *strata::ui::LauncherController::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6strata2ui18LauncherControllerE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int strata::ui::LauncherController::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 63)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 63;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 63)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 63;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 51;
    }
    return _id;
}

// SIGNAL 0
void strata::ui::LauncherController::versionsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void strata::ui::LauncherController::loaderVersionsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void strata::ui::LauncherController::instancesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void strata::ui::LauncherController::lastLaunchedChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void strata::ui::LauncherController::launchTrackingChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void strata::ui::LauncherController::deletingInstanceChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void strata::ui::LauncherController::installationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void strata::ui::LauncherController::initializationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void strata::ui::LauncherController::errorMessageChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void strata::ui::LauncherController::installFinished(bool _t1, const QString & _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1, _t2, _t3);
}

// SIGNAL 10
void strata::ui::LauncherController::offlineAccountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void strata::ui::LauncherController::accountsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void strata::ui::LauncherController::microsoftAuthChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 12, nullptr);
}

// SIGNAL 13
void strata::ui::LauncherController::consoleTextChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 13, nullptr);
}

// SIGNAL 14
void strata::ui::LauncherController::consoleInstanceChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void strata::ui::LauncherController::processStateChanged(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 15, nullptr, _t1, _t2);
}

// SIGNAL 16
void strata::ui::LauncherController::runtimesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 16, nullptr);
}

// SIGNAL 17
void strata::ui::LauncherController::runtimePreferencesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 17, nullptr);
}

// SIGNAL 18
void strata::ui::LauncherController::globalJavaArgumentsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 18, nullptr);
}

// SIGNAL 19
void strata::ui::LauncherController::runtimeInstallationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 19, nullptr);
}

// SIGNAL 20
void strata::ui::LauncherController::serversChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 20, nullptr);
}

// SIGNAL 21
void strata::ui::LauncherController::serverContentChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 21, nullptr);
}
QT_WARNING_POP
