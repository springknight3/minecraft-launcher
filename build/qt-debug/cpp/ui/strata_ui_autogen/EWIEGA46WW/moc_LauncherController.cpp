/****************************************************************************
** Meta object code from reading C++ file 'LauncherController.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
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
#error "This file was generated using the moc from 6.11.1. It"
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
        "instancesChanged",
        "accountsChanged",
        "installationChanged",
        "errorMessageChanged",
        "installFinished",
        "success",
        "instanceName",
        "error",
        "offlineAccountChanged",
        "consoleTextChanged",
        "consoleInstanceChanged",
        "processStateChanged",
        "instanceId",
        "state",
        "runtimesChanged",
        "runtimePreferencesChanged",
        "runtimeInstallationChanged",
        "refresh",
        "installInstance",
        "name",
        "versionId",
        "saveOfflineUsername",
        "selectAccount",
        "accountId",
        "removeAccount",
        "launchInstance",
        "stopInstance",
        "stopConsoleInstance",
        "openConsole",
        "closeConsole",
        "setRuntimePreferences",
        "vendor",
        "redundant",
        "installInstanceRuntime",
        "useSystemJava",
        "refreshRuntimes",
        "deleteRuntime",
        "directory",
        "versions",
        "QVariantList",
        "instances",
        "accounts",
        "activeAccountId",
        "latestRelease",
        "installing",
        "currentInstallName",
        "installationProgress",
        "currentInstallItem",
        "errorMessage",
        "usingCachedManifest",
        "offlineUsername",
        "consoleText",
        "consoleInstanceName",
        "installedRuntimes",
        "runtimeVendor",
        "redundantRuntimes",
        "installingRuntime",
        "runtimeInstallationProgress",
        "runtimeInstallationItem"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'versionsChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'instancesChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'accountsChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'installationChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'errorMessageChanged'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'installFinished'
        QtMocHelpers::SignalData<void(bool, const QString &, const QString &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 8 }, { QMetaType::QString, 9 }, { QMetaType::QString, 10 },
        }}),
        // Signal 'offlineAccountChanged'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'consoleTextChanged'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'consoleInstanceChanged'
        QtMocHelpers::SignalData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'processStateChanged'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 }, { QMetaType::QString, 16 },
        }}),
        // Signal 'runtimesChanged'
        QtMocHelpers::SignalData<void()>(17, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'runtimePreferencesChanged'
        QtMocHelpers::SignalData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'runtimeInstallationChanged'
        QtMocHelpers::SignalData<void()>(19, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'refresh'
        QtMocHelpers::MethodData<void()>(20, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'installInstance'
        QtMocHelpers::MethodData<void(const QString &, const QString &)>(21, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 }, { QMetaType::QString, 23 },
        }}),
        // Method 'saveOfflineUsername'
        QtMocHelpers::MethodData<void(const QString &)>(24, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 22 },
        }}),
        // Method 'selectAccount'
        QtMocHelpers::MethodData<void(const QString &)>(25, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 26 },
        }}),
        // Method 'removeAccount'
        QtMocHelpers::MethodData<void(const QString &)>(27, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 26 },
        }}),
        // Method 'launchInstance'
        QtMocHelpers::MethodData<void(const QString &)>(28, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Method 'stopInstance'
        QtMocHelpers::MethodData<void(const QString &)>(29, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Method 'stopConsoleInstance'
        QtMocHelpers::MethodData<void()>(30, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'openConsole'
        QtMocHelpers::MethodData<void(const QString &)>(31, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Method 'closeConsole'
        QtMocHelpers::MethodData<void()>(32, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'setRuntimePreferences'
        QtMocHelpers::MethodData<void(const QString &, bool)>(33, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 34 }, { QMetaType::Bool, 35 },
        }}),
        // Method 'installInstanceRuntime'
        QtMocHelpers::MethodData<void(const QString &)>(36, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Method 'useSystemJava'
        QtMocHelpers::MethodData<void(const QString &)>(37, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 15 },
        }}),
        // Method 'refreshRuntimes'
        QtMocHelpers::MethodData<void()>(38, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'deleteRuntime'
        QtMocHelpers::MethodData<void(const QString &)>(39, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 40 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'versions'
        QtMocHelpers::PropertyData<QVariantList>(41, 0x80000000 | 42, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 0),
        // property 'instances'
        QtMocHelpers::PropertyData<QVariantList>(43, 0x80000000 | 42, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 1),
        // property 'accounts'
        QtMocHelpers::PropertyData<QVariantList>(44, 0x80000000 | 42, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 2),
        // property 'activeAccountId'
        QtMocHelpers::PropertyData<QString>(45, QMetaType::QString, QMC::DefaultPropertyFlags, 2),
        // property 'latestRelease'
        QtMocHelpers::PropertyData<QString>(46, QMetaType::QString, QMC::DefaultPropertyFlags, 0),
        // property 'installing'
        QtMocHelpers::PropertyData<bool>(47, QMetaType::Bool, QMC::DefaultPropertyFlags, 3),
        // property 'currentInstallName'
        QtMocHelpers::PropertyData<QString>(48, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
        // property 'installationProgress'
        QtMocHelpers::PropertyData<double>(49, QMetaType::Double, QMC::DefaultPropertyFlags, 3),
        // property 'currentInstallItem'
        QtMocHelpers::PropertyData<QString>(50, QMetaType::QString, QMC::DefaultPropertyFlags, 3),
        // property 'errorMessage'
        QtMocHelpers::PropertyData<QString>(51, QMetaType::QString, QMC::DefaultPropertyFlags, 4),
        // property 'usingCachedManifest'
        QtMocHelpers::PropertyData<bool>(52, QMetaType::Bool, QMC::DefaultPropertyFlags, 0),
        // property 'offlineUsername'
        QtMocHelpers::PropertyData<QString>(53, QMetaType::QString, QMC::DefaultPropertyFlags, 6),
        // property 'consoleText'
        QtMocHelpers::PropertyData<QString>(54, QMetaType::QString, QMC::DefaultPropertyFlags, 7),
        // property 'consoleInstanceName'
        QtMocHelpers::PropertyData<QString>(55, QMetaType::QString, QMC::DefaultPropertyFlags, 8),
        // property 'installedRuntimes'
        QtMocHelpers::PropertyData<QVariantList>(56, 0x80000000 | 42, QMC::DefaultPropertyFlags | QMC::EnumOrFlag, 10),
        // property 'runtimeVendor'
        QtMocHelpers::PropertyData<QString>(57, QMetaType::QString, QMC::DefaultPropertyFlags, 11),
        // property 'redundantRuntimes'
        QtMocHelpers::PropertyData<bool>(58, QMetaType::Bool, QMC::DefaultPropertyFlags, 11),
        // property 'installingRuntime'
        QtMocHelpers::PropertyData<bool>(59, QMetaType::Bool, QMC::DefaultPropertyFlags, 12),
        // property 'runtimeInstallationProgress'
        QtMocHelpers::PropertyData<double>(60, QMetaType::Double, QMC::DefaultPropertyFlags, 12),
        // property 'runtimeInstallationItem'
        QtMocHelpers::PropertyData<QString>(61, QMetaType::QString, QMC::DefaultPropertyFlags, 12),
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
        case 1: _t->instancesChanged(); break;
        case 2: _t->accountsChanged(); break;
        case 3: _t->installationChanged(); break;
        case 4: _t->errorMessageChanged(); break;
        case 5: _t->installFinished((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[3]))); break;
        case 6: _t->offlineAccountChanged(); break;
        case 7: _t->consoleTextChanged(); break;
        case 8: _t->consoleInstanceChanged(); break;
        case 9: _t->processStateChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 10: _t->runtimesChanged(); break;
        case 11: _t->runtimePreferencesChanged(); break;
        case 12: _t->runtimeInstallationChanged(); break;
        case 13: _t->refresh(); break;
        case 14: _t->installInstance((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 15: _t->saveOfflineUsername((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 16: _t->selectAccount((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 17: _t->removeAccount((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 18: _t->launchInstance((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 19: _t->stopInstance((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 20: _t->stopConsoleInstance(); break;
        case 21: _t->openConsole((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 22: _t->closeConsole(); break;
        case 23: _t->setRuntimePreferences((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[2]))); break;
        case 24: _t->installInstanceRuntime((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 25: _t->useSystemJava((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        case 26: _t->refreshRuntimes(); break;
        case 27: _t->deleteRuntime((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::versionsChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::instancesChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::accountsChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::installationChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::errorMessageChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)(bool , const QString & , const QString & )>(_a, &LauncherController::installFinished, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::offlineAccountChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::consoleTextChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::consoleInstanceChanged, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)(const QString & , const QString & )>(_a, &LauncherController::processStateChanged, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::runtimesChanged, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::runtimePreferencesChanged, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (LauncherController::*)()>(_a, &LauncherController::runtimeInstallationChanged, 12))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<QVariantList*>(_v) = _t->versions(); break;
        case 1: *reinterpret_cast<QVariantList*>(_v) = _t->instances(); break;
        case 2: *reinterpret_cast<QVariantList*>(_v) = _t->accounts(); break;
        case 3: *reinterpret_cast<QString*>(_v) = _t->activeAccountId(); break;
        case 4: *reinterpret_cast<QString*>(_v) = _t->latestRelease(); break;
        case 5: *reinterpret_cast<bool*>(_v) = _t->installing(); break;
        case 6: *reinterpret_cast<QString*>(_v) = _t->currentInstallName(); break;
        case 7: *reinterpret_cast<double*>(_v) = _t->installationProgress(); break;
        case 8: *reinterpret_cast<QString*>(_v) = _t->currentInstallItem(); break;
        case 9: *reinterpret_cast<QString*>(_v) = _t->errorMessage(); break;
        case 10: *reinterpret_cast<bool*>(_v) = _t->usingCachedManifest(); break;
        case 11: *reinterpret_cast<QString*>(_v) = _t->offlineUsername(); break;
        case 12: *reinterpret_cast<QString*>(_v) = _t->consoleText(); break;
        case 13: *reinterpret_cast<QString*>(_v) = _t->consoleInstanceName(); break;
        case 14: *reinterpret_cast<QVariantList*>(_v) = _t->installedRuntimes(); break;
        case 15: *reinterpret_cast<QString*>(_v) = _t->runtimeVendor(); break;
        case 16: *reinterpret_cast<bool*>(_v) = _t->redundantRuntimes(); break;
        case 17: *reinterpret_cast<bool*>(_v) = _t->installingRuntime(); break;
        case 18: *reinterpret_cast<double*>(_v) = _t->runtimeInstallationProgress(); break;
        case 19: *reinterpret_cast<QString*>(_v) = _t->runtimeInstallationItem(); break;
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
        if (_id < 28)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 28;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 28)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 28;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 20;
    }
    return _id;
}

// SIGNAL 0
void strata::ui::LauncherController::versionsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void strata::ui::LauncherController::instancesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void strata::ui::LauncherController::accountsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void strata::ui::LauncherController::installationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void strata::ui::LauncherController::errorMessageChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void strata::ui::LauncherController::installFinished(bool _t1, const QString & _t2, const QString & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1, _t2, _t3);
}

// SIGNAL 6
void strata::ui::LauncherController::offlineAccountChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void strata::ui::LauncherController::consoleTextChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void strata::ui::LauncherController::consoleInstanceChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void strata::ui::LauncherController::processStateChanged(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 9, nullptr, _t1, _t2);
}

// SIGNAL 10
void strata::ui::LauncherController::runtimesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void strata::ui::LauncherController::runtimePreferencesChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void strata::ui::LauncherController::runtimeInstallationChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 12, nullptr);
}
QT_WARNING_POP
