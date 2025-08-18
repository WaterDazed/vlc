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
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Templates as T
import QtQuick.Layouts
import QtQml.Models

import VLC.MainInterface
import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

MusicAlbumSectionDelegate {
    id: root

    function setCurrentItemFocus(reason) {
        root.contentItem.forceActiveFocus(reason)
    }

    background: null

    contentItem: FocusScope {
        implicitWidth: main_loader.item ? main_loader.item.implicitWidth : 0
        implicitHeight: main_loader.item ? main_loader.item.implicitHeight : 0

        Loader {
            id: main_loader

            anchors.fill: parent
            sourceComponent: VLCStyle.isScreenSmall ? pinnedViewSmall : pinnedView
            focus: true
        }
    }

    Component {
        id: pinnedView

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: VLCStyle.margin_small
            height: implicitHeight

            Loader {
                Layout.alignment: Qt.AlignVCenter
                sourceComponent: root.albumCoverImageComponent

                property int cover_height: VLCStyle.cover_xxsmall
                property int cover_width: VLCStyle.cover_xxsmall
            }

            Loader {
                id: albumTitleLoader

                Layout.alignment: Qt.AlignLeft

                Layout.preferredWidth: implicitWidth
                Layout.preferredHeight: implicitHeight

                property int containerWidth: root.width

                property int leftUsed: buttonLoader.item?.width ?? 0
                property int leftMargins: VLCStyle.margin_small * 2

                property int rightUsed: album_caption_label.implicitWidth
                property int rightMargins: VLCStyle.margin_small * 2

                sourceComponent: root.scrollingALbumTitleComponent
            }

            Widgets.CaptionLabel {
                id: album_caption_label

                Layout.fillWidth: true // stretch to fill the remaining space and help to align items to the left
                Layout.alignment: Qt.AlignLeft
                Layout.leftMargin: VLCStyle.margin_small

                color: theme.fg.secondary
                text: root._getAlbumCaption()
            }

            Loader {
                id: buttonLoader
                sourceComponent: root.buttonsRowComponent
                Layout.alignment: Qt.AlignLeft
                focus: true
            }
        }
    }

    Component {
        id: pinnedViewSmall

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: VLCStyle.margin_small
            height: implicitHeight

            Loader {
                id: albumTitleSmallLoader

                Layout.alignment: Qt.AlignLeft

                Layout.preferredWidth: implicitWidth
                Layout.preferredHeight: implicitHeight

                property int containerWidth: root.width

                property int leftMargins: VLCStyle.margin_small
                property int rightMargins: VLCStyle.margin_small * 2

                sourceComponent: root.scrollingALbumTitleComponent
            }

            Widgets.CaptionLabel {
                Layout.fillWidth: true // stretch to fill the remaining space and help to align items to the left
                Layout.alignment: Qt.AlignLeft

                color: theme.fg.secondary
                text: root._getAlbumCaption()
            }

            Loader {
                sourceComponent: root.buttonsRowComponent
                Layout.alignment: Qt.AlignLeft
                focus: true
            }
        }
    }
}
