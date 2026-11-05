/****************************************************************************
** Meta object code from reading C++ file 'GameProcess.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../../cpp/core/launch/GameProcess.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'GameProcess.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN6strata6launch11GameProcessE_t {};
} // unnamed namespace

template <> constexpr inline auto strata::launch::GameProcess::qt_create_metaobjectdata<qt_meta_tag_ZN6strata6launch11GameProcessE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "strata::launch::GameProcess",
        "statusChanged",
        "",
        "instanceId",
        "status",
        "outputReceived",
        "text",
        "logOutputReceived",
        "logFile",
        "output",
        "finished",
        "exitCode",
        "crashed",
        "start",
        "stop"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'statusChanged'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::QString, 4 },
        }}),
        // Signal 'outputReceived'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::QString, 6 },
        }}),
        // Signal 'logOutputReceived'
        QtMocHelpers::SignalData<void(const QString &, const QByteArray &)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 8 }, { QMetaType::QByteArray, 9 },
        }}),
        // Signal 'finished'
        QtMocHelpers::SignalData<void(const QString &, int, bool)>(10, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 3 }, { QMetaType::Int, 11 }, { QMetaType::Bool, 12 },
        }}),
        // Method 'start'
        QtMocHelpers::MethodData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'stop'
        QtMocHelpers::MethodData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<GameProcess, qt_meta_tag_ZN6strata6launch11GameProcessE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject strata::launch::GameProcess::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6strata6launch11GameProcessE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6strata6launch11GameProcessE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN6strata6launch11GameProcessE_t>.metaTypes,
    nullptr
} };

void strata::launch::GameProcess::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<GameProcess *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->statusChanged((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 1: _t->outputReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QString>>(_a[2]))); break;
        case 2: _t->logOutputReceived((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QByteArray>>(_a[2]))); break;
        case 3: _t->finished((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast<std::add_pointer_t<bool>>(_a[3]))); break;
        case 4: _t->start(); break;
        case 5: _t->stop(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (GameProcess::*)(const QString & , const QString & )>(_a, &GameProcess::statusChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameProcess::*)(const QString & , const QString & )>(_a, &GameProcess::outputReceived, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameProcess::*)(const QString & , const QByteArray & )>(_a, &GameProcess::logOutputReceived, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (GameProcess::*)(const QString & , int , bool )>(_a, &GameProcess::finished, 3))
            return;
    }
}

const QMetaObject *strata::launch::GameProcess::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *strata::launch::GameProcess::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN6strata6launch11GameProcessE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int strata::launch::GameProcess::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 6)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 6;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 6)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 6;
    }
    return _id;
}

// SIGNAL 0
void strata::launch::GameProcess::statusChanged(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void strata::launch::GameProcess::outputReceived(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void strata::launch::GameProcess::logOutputReceived(const QString & _t1, const QByteArray & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void strata::launch::GameProcess::finished(const QString & _t1, int _t2, bool _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2, _t3);
}
QT_WARNING_POP
