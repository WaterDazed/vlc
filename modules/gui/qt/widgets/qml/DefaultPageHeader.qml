/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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
import QtQuick.Layouts
import QtQuick.Templates as T


import VLC.MainInterface
import VLC.Style
import VLC.Util

T.ToolBar {
    id: root

    // Properties
    property alias text: label.text

    property alias sortMenu: gridSortFilter.sortMenu

    position: T.ToolBar.Header

    topPadding: VLCStyle.layoutTitle_top_padding
    bottomPadding: VLCStyle.layoutTitle_bottom_padding

    // Settings

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)


    Navigation.navigable: gridSortFilter.enabled

    // Children

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    background: Rectangle {
        color: theme.bg.primary
    }

    RowLayout {
        id: row

        anchors.fill: parent

        SubtitleLabel {
            id: label

            Layout.fillWidth: true

            color: theme.fg.primary
        }

        GridSortFilterControls {
            id: gridSortFilter
            focus: true
            Navigation.parentItem: root
        }
    }
}
