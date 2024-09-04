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

Q_MOC_INCLUDE("qquickitem.h")
Q_MOC_INCLUDE("player/player_controller.hpp")

class Helpers : public QObject
{
    Q_OBJECT

    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Helpers(QObject *parent) : QObject(parent) { };

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
