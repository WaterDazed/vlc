/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "helpers.hpp"

#include <QQuickItem>
#include <QApplication>
#include <QQmlProperty>
#include <QQmlInfo>

#include "player/player_controller.hpp"

void Helpers::setAppOverrideCursor(Qt::CursorShape cursor)
{
    QApplication::setOverrideCursor(QCursor(cursor));
}

void Helpers::restoreAppOverrideCursor()
{
    QApplication::restoreOverrideCursor();
}

double Helpers::clamp(double number, double min, double max)
{
    return std::clamp(number, min, max);
}

int Helpers::clamp(int number, int min, int max)
{
    return std::clamp(number, min, max);
}

void Helpers::enforceFocus(QQuickItem *item, Qt::FocusReason reason)
{
    if (item->hasActiveFocus() && (item->property("focusReason") == reason))
        return;

    item->setFocus(false);
    item->forceActiveFocus(reason);
}

void Helpers::applyVolume(PlayerController *player, int delta)
{
    const int steps = std::ceil(static_cast<float>(std::abs(delta)) / 8 / 15);

    player->setMuted(false);

    if (delta > 0)
        player->setVolumeUp(steps);
    else if (delta < 0)
        player->setVolumeDown(steps);
    else
        Q_UNREACHABLE();
}

bool Helpers::pointInRadius(double x, double y, double radius)
{
    return (x * x + y * y < radius * radius);
}

bool Helpers::contains(const QRect &rect, const QPoint &pos)
{
    return (clamp(pos.x(), rect.x(), rect.x() + rect.width()) == pos.x())
            && (clamp(pos.y(), rect.y(), rect.y() + rect.height()) == pos.y());
}

double Helpers::alignUp(double a, double b)
{
    return std::ceil(a / b) * b;
}

double Helpers::alignDown(double a, double b)
{
    return std::floor(a / b) / b;
}

QList<int> Helpers::jsArrayToIntegerList(const QJSValue &jsArray)
{
    assert(isArray(jsArray));

    QList<int> list;

    const int length = jsArray.property("length").toInt();
    for (int i = 0; i < length; ++i)
    {
        const QJSValue j = jsArray.property(i);
        assert(j.isNumber());
        const QJSPrimitiveValue k = j.toPrimitive();
        assert(k.type() == QJSPrimitiveValue::Integer);
        list.append(k.toInteger());
    }

    return list;
}

bool Helpers::isSortedIntegerArrayConsecutive(const QJSValue &array)
{
    return isSortedIntegerArrayConsecutive(jsArrayToIntegerList(array));
}

bool Helpers::itemsMovable(const QJSValue &sortedItemIndexes, int targetIndex)
{
    return itemsMovable(jsArrayToIntegerList(sortedItemIndexes), targetIndex);
}

template<class T>
bool Helpers::isSortedIntegerArrayConsecutive(const QList<T> &array)
{
    for (qsizetype i = 1; i < array.count(); ++i)
    {
        if (array[i] - array[i - 1] != 1)
            return false;
    }

    return true;
}

template<class T>
bool Helpers::itemsMovable(const QList<T> &sortedItemIndexes, T targetIndex)
{
    return !isSortedIntegerArrayConsecutive(sortedItemIndexes) ||
           (targetIndex > (sortedItemIndexes[sortedItemIndexes.count() - 1] + 1) ||
            targetIndex < sortedItemIndexes[0]);
}

bool Helpers::isArray(const QJSValue &value)
{
    // QTBUG-112291
    return value.hasProperty(QStringLiteral("length"));
}

double Helpers::flickablePositionContaining(const QQuickItem *flickable, double y, double height, double topMargin, double bottomMargin)
{
    assert(flickable->inherits("QQuickFlickable"));
    const double itemTopY = flickable->property("originY").value<double>() + y;
    const double itemBottomY = itemTopY + height;

    const double viewTopY = flickable->property("contentY").value<double>();
    const double viewBottomY = viewTopY + flickable->height();

    double newContentY;

    if (itemTopY < viewTopY)
        newContentY = itemTopY - topMargin;
    else if (itemBottomY > viewBottomY)
        newContentY = itemBottomY + bottomMargin - flickable->height();
    else
        newContentY = viewTopY;

    return newContentY;
}

void Helpers::setCursor(QQuickItem *item, Qt::CursorShape cursor)
{
    assert(item);
    item->setCursor(cursor);
}

void Helpers::unsetCursor(QQuickItem *item)
{
    assert(item);
    item->unsetCursor();
}

QJSValue Helpers::urlListToMimeData(const QJSValue &array) const
{
    // NOTE: Due to a Qt regression since 17318c4
    //       (Nov 11, 2022), it is not possible to
    //       use RFC-2483 compliant string here.
    //       This regression was later corrected by
    //       c25f53b (Jul 31, 2024).
    // NOTE: Qt starts supporting string list since
    //       17318c4, so starting from 6.5.0 a string
    //       list can be used which is not affected
    //       by the said issue. For Qt versions below
    //       6.5.0, use byte array which is used as is
    //       by Qt.
    assert(array.property("length").toInt() > 0);

    QJSEngine* const engine = qjsEngine(this);
    assert(engine);

    QJSValue data;
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
    QString string;
    for (int i = 0; i < array.property(QStringLiteral("length")).toInt(); ++i)
    {
        QString decodedUrl;
        const QJSValue element = array.property(i);
        if (element.isUrl())
            // QJSValue does not have `toUrl()`
            decodedUrl = QJSManagedValue(element, engine).toUrl().toString(QUrl::FullyEncoded);
        else if (element.isString())
            // If the element is string, we assume it is already encoded
            decodedUrl = element.toString();
        else
            Q_UNREACHABLE(); // Assertion failure in debug builds
        string += decodedUrl + QStringLiteral("\r\n");
    }
    string.chop(2);
    data = engine->toScriptValue(string);
#else
    data = array;
#endif
    QJSValue ret = engine->newObject();
    ret.setProperty(QStringLiteral("text/uri-list"), data);
    return ret;
}

void Helpers::setAttachedToolTip(QObject *toolTip)
{
    // See QQuickToolTipAttachedPrivate::instance(bool create)
    assert(toolTip);

    // Prevent possible invalid down-casting:
    assert(toolTip->inherits("QQuickToolTip"));

    QQmlEngine* const engine = qmlEngine(toolTip);
    assert(engine);
    assert(engine->objectOwnership(toolTip) == QQmlEngine::ObjectOwnership::JavaScriptOwnership);

    // Dynamic internal property:
    static const char* const name = "_q_QQuickToolTip";

    if (const auto obj = engine->property(name).value<QObject *>())
    {
        if (engine->objectOwnership(obj) == QQmlEngine::ObjectOwnership::CppOwnership)
            obj->deleteLater();
    }

    // setProperty() will return false, so there is no
    // need to check the return value:
    engine->setProperty(name, QVariant::fromValue(toolTip));

// Check if the attached tooltip is actually the
// one that is set
#ifndef NDEBUG
    QQmlComponent component(engine);
    component.setData(QByteArrayLiteral("import QtQuick; import QtQuick.Controls; Item { }"), {});
    QObject* const obj = component.create();
    assert(obj);
    // Consider disabling setting of custom attached
    // tooltip if the following assertion fails:
    if (QQmlProperty::read(obj, QStringLiteral("ToolTip.toolTip"), qmlContext(obj)).value<QObject*>() != toolTip)
        qmlWarning(obj) << "Could not set self as custom ToolTip!";
    obj->deleteLater();
#endif
};
