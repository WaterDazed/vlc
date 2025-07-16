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
import QtQuick.Layouts
import QtQml.Models

import VLC.MainInterface
import VLC.MediaLibrary

import VLC.Widgets as Widgets
import VLC.Util
import VLC.Style

Rectangle {
    id: root

    required property var section
    property var album: null
    property var listViewId
    property var albumCover: (root.album && root.album.cover && root.album.cover !== "") ? root.album.cover : VLCStyle.noArtAlbumCover

    property bool forcePlayActionBtnFocusOnce: false
    property bool showClosePanelButton: true
    property bool largeCoverSize: false
    property bool showHeaderUnderArtAndControls: false
    property bool showBlurredAlbumCover: false
    property bool isStickyCommonHeaderEnabled: listViewId != null && !VLCStyle.isScreenSmall

    property bool prevAlbumBtnVisible: false
    property bool nextAlbumBtnVisible: false
    property bool prevAlbumBtnPointToEnd: false
    property bool nextAlbumBtnPointToStart: false

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    implicitHeight: {
        const verticalMargins = layout.anchors.topMargin + layout.anchors.bottomMargin
        return artAndControl.height + verticalMargins
    }

    function fetchAlbumData() {
        if (!section || albumModel.loading) return
        albumModel.getDataAtId(section).then((albumData) => {
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

    Component.onCompleted: { fetchAlbumData() }
    onSectionChanged: { fetchAlbumData() }

    width: parent.width
    height: col.implicitHeight + VLCStyle.margin_small
    color: "transparent"

    Component {
        id: cover

        Widgets.ImageExt {
            id: expand_cover_id

            property int cover_height: parent.cover_height
            property int cover_width: parent.cover_width

            height: cover_height
            width: cover_width
            radius: VLCStyle.expandCover_music_radius
            source: root.albumCover
            sourceSize: Qt.size(width * eDPR, height * eDPR)

            readonly property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)

            Widgets.DefaultShadow {
                visible: (parent.status === Image.Ready)
            }
        }
    }

    Component {
        id: bgCover

        Rectangle {

            Widgets.ImageExt {
                id: expand_cover_id_blur

                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                source: root.albumCover
            }

            Item {
                anchors.fill: expand_cover_id_blur
                clip: !blurEffect.sourceNeedsLayering

                Widgets.FrostedGlassEffect {
                    id: blurEffect
                    ColorContext {
                        id: frostedTheme
                        palette: VLCStyle.palette
                        colorSet: ColorContext.Window
                    }

                    tint: frostedTheme.bg.secondary
                    tintStrength: 0.87

                    source: expand_cover_id_blur
                }
            }
        }
    }

    Component {
        id: buttons

        Widgets.NavigableRow {
            id: actionButtons

            property alias enqueueActionBtn: _enqueueActionBtn
            property alias playActionBtn: _playActionBtn
            property alias prevAlbumBtn: _prevAlbumBtn
            property alias nextAlbumBtn: _nextAlbumBtn

            focus: true
            width: root.largeCoverSize ? VLCStyle.listCover_music_width : VLCStyle.expandCover_music_width

            spacing: VLCStyle.margin_small

            Layout.alignment: Qt.AlignCenter

            model: ObjectModel {
                Widgets.ActionButtonPrimary {
                    id: _playActionBtn

                    iconTxt: VLCIcons.play
                    text: qsTr("Play")
                    onClicked: MediaLib.addAndPlay( root.album.id )
                }

                Widgets.ButtonExt {
                    id: _enqueueActionBtn

                    iconTxt: VLCIcons.enqueue
                    text: qsTr("Enqueue")
                    onClicked: MediaLib.addToPlaylist( root.album.id )
                }

                Widgets.ButtonExt {
                    id: _prevAlbumBtn

                    iconTxt: root.prevAlbumBtnPointToEnd ? VLCIcons.chevron_down : VLCIcons.chevron_up
                    text: root.prevAlbumBtnPointToEnd ? qsTr("Bottom") : qsTr("Prev")
                    // onClicked: root.changeAlbum( 1 )
                    visible: root.prevAlbumBtnVisible
                }

                Widgets.ButtonExt {
                    id: _nextAlbumBtn

                    iconTxt: root.nextAlbumBtnPointToStart ? VLCIcons.chevron_up : VLCIcons.chevron_down
                    text: root.nextAlbumBtnPointToStart ? qsTr("Top") : qsTr("Next")
                    // onClicked: root.changeAlbum( -1 )
                    visible: root.nextAlbumBtnVisible
                }
            }
        }
    }

    Loader {
        anchors.fill: parent
        sourceComponent: bgCover
    }

    RowLayout {
        id: layout

        anchors.fill: parent
        anchors.topMargin: VLCStyle.margin_small
        anchors.leftMargin: VLCStyle.margin_large

        spacing: VLCStyle.margin_normal
        Layout.alignment: Qt.AlignLeft

        FocusScope {
            id: artAndControl

            visible: !VLCStyle.isScreenSmall
            focus: !VLCStyle.isScreenSmall

            implicitHeight: artAndControlLayout.implicitHeight
            implicitWidth: artAndControlLayout.implicitWidth
            Layout.alignment: Qt.AlignTop

            Column {
                id: artAndControlLayout

                spacing: VLCStyle.margin_normal
                bottomPadding: VLCStyle.margin_large

                /* A bigger cover for the album */
                Loader {
                    sourceComponent: !VLCStyle.isScreenSmall ? cover : null
                    property int cover_height: root.largeCoverSize ? VLCStyle.listCover_music_height : VLCStyle.expandCover_music_height
                    property int cover_width: root.largeCoverSize ? VLCStyle.listCover_music_width : VLCStyle.expandCover_music_width
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true

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

            Loader {
                Layout.topMargin: VLCStyle.margin_small

                Layout.fillHeight: true
                Layout.fillWidth: true

                sourceComponent: !VLCStyle.isScreenSmall ? buttons : null

                onLoaded: {
                    root.playActionBtn = item.playActionBtn
                    root.enqueueActionBtn = item.enqueueActionBtn
                    root.prevAlbumBtn = item.prevAlbumBtn
                    root.nextAlbumBtn = item.nextAlbumBtn
                }
            }
        }
    }
}
