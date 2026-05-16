/****************************************************************************
** Meta object code from reading C++ file 'SignalBus.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/app/SignalBus.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'SignalBus.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.1. It"
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
struct qt_meta_tag_ZN9SignalBusE_t {};
} // unnamed namespace

template <> constexpr inline auto SignalBus::qt_create_metaobjectdata<qt_meta_tag_ZN9SignalBusE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "SignalBus",
        "frameReady",
        "",
        "frame",
        "region",
        "ocrCompleted",
        "OCRResult",
        "result",
        "translationReady",
        "original",
        "translated",
        "sourceRect",
        "modeChanged",
        "Mode",
        "newMode",
        "styleChanged",
        "StyleConfig",
        "style",
        "globalVisibilityChanged",
        "visible",
        "translationError",
        "text",
        "error",
        "areaConfirmed",
        "SelectionArea",
        "area",
        "snapshotRequested"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'frameReady'
        QtMocHelpers::SignalData<void(const QImage &, const QRect &)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QImage, 3 }, { QMetaType::QRect, 4 },
        }}),
        // Signal 'ocrCompleted'
        QtMocHelpers::SignalData<void(const OCRResult &)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 6, 7 },
        }}),
        // Signal 'translationReady'
        QtMocHelpers::SignalData<void(const QString &, const QString &, const QRect &)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 9 }, { QMetaType::QString, 10 }, { QMetaType::QRect, 11 },
        }}),
        // Signal 'modeChanged'
        QtMocHelpers::SignalData<void(Mode)>(12, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 13, 14 },
        }}),
        // Signal 'styleChanged'
        QtMocHelpers::SignalData<void(const StyleConfig &)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 16, 17 },
        }}),
        // Signal 'globalVisibilityChanged'
        QtMocHelpers::SignalData<void(bool)>(18, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 19 },
        }}),
        // Signal 'translationError'
        QtMocHelpers::SignalData<void(const QString &, const QString &)>(20, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::QString, 21 }, { QMetaType::QString, 22 },
        }}),
        // Signal 'areaConfirmed'
        QtMocHelpers::SignalData<void(const SelectionArea &)>(23, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 24, 25 },
        }}),
        // Signal 'snapshotRequested'
        QtMocHelpers::SignalData<void()>(26, 2, QMC::AccessPublic, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<SignalBus, qt_meta_tag_ZN9SignalBusE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject SignalBus::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SignalBusE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SignalBusE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN9SignalBusE_t>.metaTypes,
    nullptr
} };

void SignalBus::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<SignalBus *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->frameReady((*reinterpret_cast< std::add_pointer_t<QImage>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QRect>>(_a[2]))); break;
        case 1: _t->ocrCompleted((*reinterpret_cast< std::add_pointer_t<OCRResult>>(_a[1]))); break;
        case 2: _t->translationReady((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<QRect>>(_a[3]))); break;
        case 3: _t->modeChanged((*reinterpret_cast< std::add_pointer_t<Mode>>(_a[1]))); break;
        case 4: _t->styleChanged((*reinterpret_cast< std::add_pointer_t<StyleConfig>>(_a[1]))); break;
        case 5: _t->globalVisibilityChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 6: _t->translationError((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 7: _t->areaConfirmed((*reinterpret_cast< std::add_pointer_t<SelectionArea>>(_a[1]))); break;
        case 8: _t->snapshotRequested(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (SignalBus::*)(const QImage & , const QRect & )>(_a, &SignalBus::frameReady, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (SignalBus::*)(const OCRResult & )>(_a, &SignalBus::ocrCompleted, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (SignalBus::*)(const QString & , const QString & , const QRect & )>(_a, &SignalBus::translationReady, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (SignalBus::*)(Mode )>(_a, &SignalBus::modeChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (SignalBus::*)(const StyleConfig & )>(_a, &SignalBus::styleChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (SignalBus::*)(bool )>(_a, &SignalBus::globalVisibilityChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (SignalBus::*)(const QString & , const QString & )>(_a, &SignalBus::translationError, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (SignalBus::*)(const SelectionArea & )>(_a, &SignalBus::areaConfirmed, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (SignalBus::*)()>(_a, &SignalBus::snapshotRequested, 8))
            return;
    }
}

const QMetaObject *SignalBus::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *SignalBus::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN9SignalBusE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int SignalBus::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void SignalBus::frameReady(const QImage & _t1, const QRect & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1, _t2);
}

// SIGNAL 1
void SignalBus::ocrCompleted(const OCRResult & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1);
}

// SIGNAL 2
void SignalBus::translationReady(const QString & _t1, const QString & _t2, const QRect & _t3)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2, _t3);
}

// SIGNAL 3
void SignalBus::modeChanged(Mode _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void SignalBus::styleChanged(const StyleConfig & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 4, nullptr, _t1);
}

// SIGNAL 5
void SignalBus::globalVisibilityChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 5, nullptr, _t1);
}

// SIGNAL 6
void SignalBus::translationError(const QString & _t1, const QString & _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 6, nullptr, _t1, _t2);
}

// SIGNAL 7
void SignalBus::areaConfirmed(const SelectionArea & _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 7, nullptr, _t1);
}

// SIGNAL 8
void SignalBus::snapshotRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}
QT_WARNING_POP
