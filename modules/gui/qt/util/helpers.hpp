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
#ifndef HELPERS_HPP
#define HELPERS_HPP

#include <QObject>
#include <QQmlEngine>

class QQuickItem;
class PlayerController;
struct qt_intf_t;

Q_MOC_INCLUDE("qquickitem.h")
Q_MOC_INCLUDE("player/player_controller.hpp")

class Helpers : public QObject
{
    Q_OBJECT

    QML_ELEMENT
    QML_SINGLETON

    // WARNING: It is not allowed to have properties in this singleton.
    //          When necessary, use the relevant context singleton to
    //          add property. In other words, Helpers itself should
    //          not carry state in itself.

    qt_intf_t * const m_intf;

public:
    explicit Helpers(qt_intf_t* p_intf, QObject *parent)
        : QObject(parent)
        , m_intf(p_intf)
    {
        assert(m_intf);
    };

    Q_INVOKABLE static void setAppOverrideCursor(Qt::CursorShape cursor);
    Q_INVOKABLE static void restoreAppOverrideCursor(void);

    Q_INVOKABLE static void setCursor(QQuickItem *item, Qt::CursorShape cursor);
    Q_INVOKABLE static void unsetCursor(QQuickItem *item);

    Q_INVOKABLE static /*constexpr*/ bool qtQuickControlRejectsHoverEvents() {
        // QTBUG-100543
        return (QT_VERSION < QT_VERSION_CHECK(6, 3, 0) && QT_VERSION >= QT_VERSION_CHECK(6, 2, 5)) ||
               (QT_VERSION < QT_VERSION_CHECK(6, 4, 0) && QT_VERSION >= QT_VERSION_CHECK(6, 3, 1)) ||
               (QT_VERSION >= QT_VERSION_CHECK(6, 4, 0));
    }

    Q_INVOKABLE QJSValue urlListToMimeData(const QJSValue& array) const;

    Q_INVOKABLE static void setAttachedToolTip(QObject* toolTip);

    Q_INVOKABLE QVariant settingValue(const QString &key, const QVariant &defaultValue) const;
    Q_INVOKABLE void setSettingValue(const QString &key, const QVariant &value);

    Q_INVOKABLE static double clamp(double number, double min, double max);
    Q_INVOKABLE static int clamp(int number, int min, int max);

    Q_INVOKABLE static void enforceFocus(QQuickItem* item, Qt::FocusReason reason);

    Q_INVOKABLE static void applyVolume(PlayerController* player, int delta);

    Q_INVOKABLE static bool pointInRadius(double x, double y, double radius);

    Q_INVOKABLE static bool contains(const QRect& rect, const QPoint& position);

    Q_INVOKABLE static double alignUp(double a, double b);
    Q_INVOKABLE static double alignDown(double a, double b);

    Q_INVOKABLE static QList<int> jsArrayToIntegerList(const QJSValue& jsArray);

    template<class T>
    static bool isSortedIntegerArrayConsecutive(const QList<T>& array);

    Q_INVOKABLE static bool isSortedIntegerArrayConsecutive(const QJSValue& array);

    template<class T>
    static bool itemsMovable(const QList<T>& sortedItemIndexes, T targetIndex);

    Q_INVOKABLE static bool itemsMovable(const QJSValue& sortedItemIndexes, int targetIndex);

    Q_INVOKABLE static bool isArray(const QJSValue& value);

    /**
     * calculate content y for flickable such that item with given param will be fully visible
     * @param type:Flickable flickable
     * @param type:real y
     * @param type:real height
     * @param type:real topMargin
     * @param type:real bottomMargin
     * @return type:real appropriate contentY for flickable
     */
    Q_INVOKABLE static double flickablePositionContaining(const QQuickItem *flickable,
                                                          double y,
                                                          double height,
                                                          double topMargin,
                                                          double bottomMargin);
};

#endif // HELPERS_HPP
