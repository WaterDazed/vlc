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

T.Pane {
    id: root

    required property var section
    property var album: null
    property var listViewId
    property var albumCover: (root.album && root.album.cover && root.album.cover !== "") ? root.album.cover : VLCStyle.noArtAlbumCover

    property var playActionBtn

    property bool largeCoverSize: false
    property bool showBlurredAlbumCover: false
    property bool pinnedStyle: false

    property bool prevAlbumBtnVisible: false
    property bool nextAlbumBtnVisible: false
    property bool prevAlbumBtnPointToEnd: false
    property bool nextAlbumBtnPointToStart: false

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    signal requestAlbumChange(int direction)

    topPadding: VLCStyle.margin_xsmall
    bottomPadding: VLCStyle.margin_xsmall

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    Component {
        id: background

        Item {
            clip: !blurEffect.sourceNeedsLayering
            visible: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader)

            Image {
                id: album_bg_cover

                anchors.fill: parent

                source: root.albumCover
                sourceSize: root.albumCover ? Qt.size(Helpers.alignUp(Screen.desktopAvailableWidth, 32), 0) : undefined
                mipmap: !!root.albumCover

                fillMode: Image.Stretch

                visible: !blurEffect.visible
                cache: (source === VLCStyle.noArtArtist)

                opacity: blurEffect.visible ? 1.0 : 0.5
            }

            Widgets.FrostedGlassEffect {
                id: blurEffect

                anchors.left: parent.left
                anchors.right: parent.right

                anchors.verticalCenter: parent.verticalCenter

                readonly property bool sourceNeedsLayering: (album_bg_cover.fillMode !== Image.Stretch) ||
                                                            (MainCtx.qtVersion() < MainCtx.qtVersionCheck(6, 5, 0))
                readonly property real aspectRatio: (album_bg_cover.implicitHeight / album_bg_cover.implicitWidth)

                height: sourceNeedsLayering ? album_bg_cover.height : (aspectRatio * width)

                source: album_bg_cover

                ColorContext {
                    id: frostedTheme
                    palette: VLCStyle.palette
                    colorSet: ColorContext.Window
                }

                tint: frostedTheme.bg.secondary
                tintStrength: 0.8
            }
        }
    }

    background: Loader {
        sourceComponent: pinnedStyle ? null : background
    }

    function setCurrentItemFocus(reason) {
        root.playActionBtn.forceActiveFocus(reason)
    }

    function fetchAlbumData() {
        if (!section || albumModel.loading) return
        albumModel.getDataByIdSerialized(section).then((albumData) => {
            album = albumData
        })
    }

    function _getAlbumCaption() {
        const album = root.album
        if (!album)
            return ""

        const parts = []

        if (!root.pinnedStyle) {
            parts.push(album.main_artist || qsTr("Unknown artist"))
        }

        const year = album.release_year
        if (year)
            parts.push(year)

        const count = album.nb_tracks ?? 0
        parts.push(qsTr(count < 2 ? "%1 track" : "%1 tracks").arg(count))

        const duration = album.duration?.formatHMS()
        if (duration)
            parts.push(duration)

        return parts.join(" - ")
    }

    onSectionChanged: {
        fetchAlbumData()
    }

    Component {
        id: buttonsRow

        RowLayout {
            id: actionButtons

            property alias playActionBtn: _playActionBtn
            spacing: VLCStyle.margin_small

            Widgets.ActionButtonPrimary {
                id: _playActionBtn

                iconTxt: VLCIcons.play
                text: qsTr("Play")
                onClicked: {
                    MediaLib.addAndPlay( root.album.id )
                }

                Navigation.parentItem: root
                Navigation.rightItem: _enqueueActionBtn
            }

            Widgets.ButtonExt {
                id: _enqueueActionBtn

                iconTxt: VLCIcons.enqueue
                text: qsTr("Enqueue")
                onClicked: {
                    MediaLib.addToPlaylist( root.album.id )
                }

                Navigation.parentItem: root
                Navigation.rightItem: _prevAlbumBtn
                Navigation.leftItem: _playActionBtn
            }

            Widgets.ButtonExt {
                id: _prevAlbumBtn

                iconTxt: root.prevAlbumBtnPointToEnd ? VLCIcons.chevron_down : VLCIcons.chevron_up
                text: root.prevAlbumBtnPointToEnd ? qsTr("Bottom") : qsTr("Prev")
                onClicked: {
                    root.requestAlbumChange( -1 )
                }
                visible: root.prevAlbumBtnVisible

                Navigation.parentItem: root
                Navigation.rightItem: _nextAlbumBtn
                Navigation.leftItem: _enqueueActionBtn
            }

            Widgets.ButtonExt {
                id: _nextAlbumBtn

                iconTxt: root.nextAlbumBtnPointToStart ? VLCIcons.chevron_up : VLCIcons.chevron_down
                text: root.nextAlbumBtnPointToStart ? qsTr("Top") : qsTr("Next")
                onClicked: {
                    root.requestAlbumChange( 1 )
                }
                visible: root.nextAlbumBtnVisible

                Navigation.parentItem: root
                Navigation.leftItem: _prevAlbumBtn
            }
        }
    }

    Component {
        id: albumCoverImage

        Widgets.ImageExt {
            Layout.alignment: Qt.AlignVCenter

            width: parent.cover_width ?? VLCStyle.cover_xxsmall
            height: parent.cover_height ?? VLCStyle.cover_xxsmall

            property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)

            radius: VLCStyle.expandCover_music_radius
            source: root.albumCover
            sourceSize: Qt.size(width * eDPR, height * eDPR)

            Widgets.DefaultShadow {
                visible: (parent.status === Image.Ready)
            }
        }
    }

    Component {
        id: inlineView

        RowLayout {
            id: layout

            anchors.fill: parent
            anchors.bottomMargin: root.bottomSpacingMargin
            anchors.leftMargin: VLCStyle.margin_large

            spacing: VLCStyle.margin_normal

            Loader {
                Layout.alignment: Qt.AlignVCenter
                sourceComponent: albumCoverImage

                property int cover_height: VLCStyle.cover_small
                property int cover_width: VLCStyle.cover_small
            }

            Item {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter

                implicitHeight: {
                    return colLayout.implicitHeight + VLCStyle.margin_small / 2
                }

                ColumnLayout {
                    id: colLayout

                    Widgets.SubtitleLabel {
                        text: album?.title || qsTr("Unknown title")
                        color: theme.fg.primary
                    }

                    Widgets.CaptionLabel {
                        color: theme.fg.secondary
                        width: parent.width

                        text: root._getAlbumCaption()
                    }

                    Loader {
                        sourceComponent: buttonsRow
                        onLoaded: {
                            root.playActionBtn = item.playActionBtn
                        }
                    }
                }
            }
        }
    }

    Component {
        id: scrollingALbumTitle

        Widgets.TextAutoScroller {
            label: albumTitleLabel
            forceScroll: albumTitleMouseHandler.hovered
            visible: albumTitleLabel.text !== ""
            clip: true

            property int containerWidth: (parent.containerWidth ?? 0) + 0

            property int leftUsed: parent.leftUsed ?? 0
            property int leftMargins: parent.leftMargins ?? 0
            property int rightUsed: parent.rightUsed ?? 0
            property int rightMargins: parent.rightMargins ?? 0

            implicitWidth: {
                const available = containerWidth - leftUsed - rightUsed - leftMargins - rightMargins;
                return Math.min(albumTitleLabel.implicitWidth, available);
            }
            implicitHeight: albumTitleLabel.height

            HoverHandler {
                id: albumTitleMouseHandler
            }

            Widgets.SubtitleLabel {
                id: albumTitleLabel
                text: album?.title || qsTr("Unknown title")
                color: theme.fg.primary
            }
        }
    }

    Component {
        id: pinnedView

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: VLCStyle.margin_small
            height: implicitHeight

            Loader {
                id: albumTitleLoader

                Layout.alignment: Qt.AlignLeft

                Layout.preferredWidth: implicitWidth
                Layout.preferredHeight: implicitHeight

                property int containerWidth: root.width

                property int leftUsed: (buttonLoader.item?.width ?? 0)
                property int leftMargins: VLCStyle.margin_small * 2

                property int rightUsed: album_caption_label.implicitWidth
                property int rightMargins: VLCStyle.margin_small * 2

                sourceComponent: scrollingALbumTitle
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
                sourceComponent: buttonsRow
                Layout.alignment: Qt.AlignLeft
                onLoaded: {
                    root.playActionBtn = item.playActionBtn
                }
            }
        }
    }

    Component {
        id: pinnedViewSmall

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: VLCStyle.margin_small
            height: implicitHeight

            RowLayout {
                Layout.fillWidth: true

                Loader {
                    id: albumTitleSmallLoader

                    Layout.alignment: Qt.AlignLeft

                    Layout.preferredWidth: implicitWidth
                    Layout.preferredHeight: implicitHeight

                    property int containerWidth: root.width

                    property int leftMargins: VLCStyle.margin_small
                    property int rightMargins: VLCStyle.margin_small * 2

                    sourceComponent: scrollingALbumTitle
                }
            }

            Widgets.CaptionLabel {
                Layout.fillWidth: true // stretch to fill the remaining space and help to align items to the left
                Layout.alignment: Qt.AlignLeft

                color: theme.fg.secondary
                text: root._getAlbumCaption()
            }

            RowLayout {
                Layout.fillWidth: true

                Loader {
                    sourceComponent: buttonsRow
                    Layout.alignment: Qt.AlignLeft
                    onLoaded: {
                        root.playActionBtn = item.playActionBtn
                    }
                }
            }
        }
    }

    Loader {
        id: main_loader

        anchors.fill: parent
        sourceComponent: root.pinnedStyle ? (VLCStyle.isScreenSmall ? pinnedViewSmall : pinnedView) : inlineView
    }
}
