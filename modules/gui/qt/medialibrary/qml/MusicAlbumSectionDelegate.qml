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

    property bool largeCoverSize: false
    property bool showBlurredAlbumCover: false

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

    background: Item {
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

    function fetchAlbumData() {
        if (!section || albumModel.loading) return
        albumModel.getDataByIdSerialized(section).then((albumData) => {
            album = albumData
        })
    }

    function _getStringTrack() {
        const count = root.album?.nb_tracks ?? 0;

        if (count < 2)
            return qsTr("%1 track").arg(count);
        else
            return qsTr("%1 tracks").arg(count);
    }

    onSectionChanged: {
        fetchAlbumData()
    }

    RowLayout {
        id: layout

        anchors.fill: parent
        anchors.bottomMargin: root.bottomSpacingMargin
        anchors.leftMargin: VLCStyle.margin_large

        spacing: VLCStyle.margin_normal

        Item {
            id: albumCover

            implicitHeight: albumCoverLayout.implicitHeight
            implicitWidth: albumCoverLayout.implicitWidth
            Layout.alignment: Qt.AlignVCenter

            Column {
                id: albumCoverLayout

                spacing: VLCStyle.margin_normal

                Widgets.ImageExt {
                    property int cover_height: VLCStyle.cover_small
                    property int cover_width: VLCStyle.cover_small
                    property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)

                    height: cover_height
                    width: cover_width
                    radius: VLCStyle.expandCover_music_radius
                    source: root.albumCover
                    sourceSize: Qt.size(width * eDPR, height * eDPR)

                    Widgets.DefaultShadow {
                        visible: (parent.status === Image.Ready)
                    }
                }
            }
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
                    id: expand_infos_subtitle_id

                    color: theme.fg.secondary
                    width: parent.width

                    text: qsTr("%1 - %2 - %3 - %4")
                        .arg(root.album?.main_artist || qsTr("Unknown artist"))
                        .arg(root.album?.release_year || "")
                        .arg(_getStringTrack())
                        .arg(root.album?.duration?.formatHMS() ?? 0)
                }

                RowLayout {
                    id: actionButtons

                    width: VLCStyle.isScreenSmall ? VLCStyle.listCover_music_width : VLCStyle.expandCover_music_width
                    spacing: VLCStyle.margin_small

                    Widgets.ActionButtonPrimary {
                        id: _playActionBtn

                        iconTxt: VLCIcons.play
                        text: qsTr("Play")
                        onClicked: {
                            MediaLib.addAndPlay( root.album.id )
                        }
                    }

                    Widgets.ButtonExt {
                        id: _enqueueActionBtn

                        iconTxt: VLCIcons.enqueue
                        text: qsTr("Enqueue")
                        onClicked: {
                            MediaLib.addToPlaylist( root.album.id )
                        }
                    }

                    Widgets.ButtonExt {
                        id: _prevAlbumBtn

                        iconTxt: root.prevAlbumBtnPointToEnd ? VLCIcons.chevron_down : VLCIcons.chevron_up
                        text: root.prevAlbumBtnPointToEnd ? qsTr("Bottom") : qsTr("Prev")
                        onClicked: {
                            root.requestAlbumChange( -1 )
                        }
                        visible: root.prevAlbumBtnVisible
                    }

                    Widgets.ButtonExt {
                        id: _nextAlbumBtn

                        iconTxt: root.nextAlbumBtnPointToStart ? VLCIcons.chevron_up : VLCIcons.chevron_down
                        text: root.nextAlbumBtnPointToStart ? qsTr("Top") : qsTr("Next")
                        onClicked: {
                            root.requestAlbumChange( 1 )
                        }
                        visible: root.nextAlbumBtnVisible
                    }
                }
            }
        }
    }
}
