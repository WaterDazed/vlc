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

import VLC.Widgets as Widgets
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
            anchors.leftMargin: VLCStyle.margin_small
            sourceComponent: VLCStyle.isScreenSmall ? pinnedViewSmall : pinnedView
            focus: true
        }
    }

    Component {
        id: pinnedView

        RowLayout {
            spacing: VLCStyle.margin_small

            Widgets.ImageExt {
                Layout.preferredHeight: VLCStyle.cover_xxsmall
                Layout.preferredWidth: VLCStyle.cover_xxsmall

                radius: VLCStyle.expandCover_music_radius

                source: root._albumCover
                sourceSize: Qt.size(width * root.eDPR, height * root.eDPR)
                asynchronous: true

                Widgets.DefaultShadow {
                    visible: (parent.status === Image.Ready)
                }
            }

            Widgets.TextAutoScroller {
                label: albumTitleLabel
                forceScroll: root.visualFocus
                clip: scrolling

                Layout.fillWidth: true
                Layout.preferredWidth: Math.min(albumTitleLabel.implicitWidth, root.width)
                // A small margin is needed to prevent eliding even though width is satisfactory and does not need to elide
                Layout.maximumWidth: albumTitleLabel.implicitWidth + VLCStyle.margin_xxsmall
                Layout.preferredHeight: albumTitleLabel.implicitHeight

                Widgets.SubtitleLabel {
                    id: albumTitleLabel
                    text: _albumData?.title || qsTr("Unknown title")
                    color: theme.fg.primary
                }
            }

            Widgets.CaptionLabel {
                id: album_caption_label

                Layout.fillWidth: true

                color: theme.fg.secondary
                text: root._getAlbumCaption()
            }

            Loader {
                id: buttonLoader
                sourceComponent: root.buttonsRowComponent
                focus: true
            }
        }
    }

    Component {
        id: pinnedViewSmall

        ColumnLayout {
            spacing: VLCStyle.margin_xsmall

            Widgets.TextAutoScroller {
                label: albumTitleLabelSmall
                forceScroll: root.visualFocus
                clip: scrolling

                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: albumTitleLabelSmall.implicitHeight - VLCStyle.margin_xsmall

                Widgets.SubtitleLabel {
                    id: albumTitleLabelSmall
                    text: _albumData?.title || qsTr("Unknown title")
                    color: theme.fg.primary
                }
            }

            Widgets.CaptionLabel {
                // stretch to fill the remaining space and help to align items to the left
                Layout.fillWidth: true

                color: theme.fg.secondary
                text: root._getAlbumCaption()
            }

            Loader {
                sourceComponent: root.buttonsRowComponent
                focus: true
            }
        }
    }
}
