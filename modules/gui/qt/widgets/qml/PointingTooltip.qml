/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick
import QtQuick.Controls

import VLC.Style
import VLC.Util
import VLC.MainInterface

ToolTipExt {
    id: root

    margins: 0
    padding: VLCStyle.margin_xxsmall

    height: implicitHeight + background.arrowHeight
    bottomInset: height - implicitHeight

    x: isAWindow ? _tootipPos.x : _clippedPos.x
    y: isAWindow ? _tootipPos.y : _clippedPos.y

    required property point pos

    readonly property point _tootipPos: Qt.point(
            pos.x - (width / 2),
            pos.y - (implicitHeight + arrowArea.implicitHeight + VLCStyle.dp(7.5)))

    // workaround for QTBUG-113468
    // when tooltip get negative coordinates relative to the window
    // it will flickers. To avoid this, we ensure that the tooltip cannot exceed
    // the window boundaries

    //tooltip position in window referential
    readonly property point _tooltipScenePos: parent.mapToItem(Window.contentItem, _tootipPos)

    //restrict tooltip position to window boundaries
    // use MainCtx.intfMainWindow.width here as Overlay.overlay.width returns 0 with Qt 6.2 (works with Qt6.5)
    // Window.width always returns 0 from here (6.2 -> 6.8)
    readonly property point _clippedPos: root.isAWindow ? Qt.point(0,0) //only compute value when tooltip is embed
        : parent.mapFromItem(
            Window.contentItem,
            Helpers.clamp(_tooltipScenePos.x, 0, MainCtx.intfMainWindow.width - root.width),
            Helpers.clamp(_tooltipScenePos.y, 0, MainCtx.intfMainWindow.height - root.height)
        )

    background: Rectangle {
        border.color: root.colorContext.border
        color: root.colorContext.bg.primary
        radius: VLCStyle.dp(6, VLCStyle.scale)

        readonly property real arrowHeight: arrow.implicitHeight + border.width

        Item {
            id: arrowArea

            z: 1
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.bottom
            anchors.topMargin: -(parent.border.width)

            implicitHeight: arrow.implicitHeight * Math.sqrt(2) / 2

            clip: true

            Rectangle {
                id: arrow

                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: (root.isAWindow) ? 0
                                                                 : (root._tootipPos.x - root._clippedPos.x)
                anchors.verticalCenter: parent.top

                implicitWidth: VLCStyle.dp(10, VLCStyle.scale)
                implicitHeight: VLCStyle.dp(10, VLCStyle.scale)

                rotation: 45

                color: root.colorContext.bg.primary
                border.color: root.colorContext.border
            }
        }
    }
}
