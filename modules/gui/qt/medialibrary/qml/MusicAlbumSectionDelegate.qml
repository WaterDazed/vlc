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

    required property string section
    readonly property ListView _view: ListView.view
    readonly property url _albumCover: (root._albumData && root._albumData.cover) ? root._albumData.cover : VLCStyle.noArtAlbumCover
    property var _albumData: null

    property bool largeCoverSize: false
    property bool showBlurredAlbumCover: false

    property bool prevAlbumBtnVisible: false
    property bool nextAlbumBtnVisible: false
    property bool prevAlbumBtnPointToEnd: false
    property bool nextAlbumBtnPointToStart: false

    property real eDPR

    readonly property Component buttonsRowComponent: buttonsRow
    readonly property Component albumCoverImageComponent: albumCoverImage

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    signal requestAlbumChange(int direction)

    topPadding: VLCStyle.margin_xsmall
    bottomPadding: VLCStyle.margin_xsmall
    leftPadding: VLCStyle.margin_large

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    function getBackgroundYPos() {
        if (!root._view) return
        // Limit pos to 0 and the blurEffect's height as that is the max value by which we can shift
        // Otherwise we will shift it beyond the effect's height which will shift the item out of the _view vertically
        const pos = Helpers.clamp(root._view.mapFromItem(root, 0, 0).y, 0, blurEffect.height)
        const height = root._view.height
        // The height of the image varies with the width of the _view, make sure that we don't shift the image
        // too much to send it out of the _view vertically when scrolling downwards
        const normalized_pos = pos * (blurEffect.height / height)
        //FIXME: When scrolling the pos shifts abruptly, might be due to the behavior of sections with contentHeight
        const parallax_multiplier = Helpers.clamp(root._view.contentY / (root._view.contentHeight - height), 0.0, 1.0)
        return -normalized_pos * parallax_multiplier
    }

    background: Item {
        clip: !blurEffect.sourceNeedsLayering
        visible: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader)

        Image {
            id: album_bg_cover

            anchors.fill: parent

            source: root._albumCover
            sourceSize: root._albumCover ? Qt.size(Helpers.alignUp(Screen.desktopAvailableWidth, 32), 0) : undefined
            mipmap: !!root._albumCover

            fillMode: Image.Stretch

            visible: !blurEffect.visible
            cache: (source === VLCStyle.noArtArtist)

            opacity: blurEffect.visible ? 1.0 : 0.5
        }

        Widgets.FrostedGlassEffect {
            id: blurEffect

            anchors.left: parent.left
            anchors.right: parent.right

            Binding on y {
                when: root._view !== null
                value: getBackgroundYPos()
            }

            readonly property bool sourceNeedsLayering: (album_bg_cover.fillMode !== Image.Stretch) ||
                                                        (MainCtx.qtVersion() < MainCtx.qtVersionCheck(6, 5, 0))
            readonly property real aspectRatio: (album_bg_cover.implicitHeight / album_bg_cover.implicitWidth)

            height: sourceNeedsLayering ? album_bg_cover.height : (aspectRatio * width)

            // Sections are re-used, but they may not release GPU resources immediately.
            // This ensures resources are freed to limit peak VRAM consumption.
            source: visible ? album_bg_cover : null

            ColorContext {
                id: frostedTheme
                palette: VLCStyle.palette
                colorSet: ColorContext.Window
            }

            tint: frostedTheme.bg.secondary
            tintStrength: 0.8
        }
    }

    contentItem: RowLayout {
        spacing: VLCStyle.margin_normal

        Loader {
            id: albumCoverLoader
            Layout.alignment: Qt.AlignVCenter
            sourceComponent: albumCoverImage

            Layout.preferredHeight: cover_height
            Layout.preferredWidth: cover_width

            property int cover_height: VLCStyle.cover_small
            property int cover_width: VLCStyle.cover_small
        }

        Item {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter

            Layout.preferredHeight: colLayout.implicitHeight + VLCStyle.margin_small / 2

            ColumnLayout {
                id: colLayout

                spacing: VLCStyle.margin_xxsmall

                Widgets.SubtitleLabel {
                    text: root._albumData?.title || qsTr("Unknown title")
                    color: theme.fg.primary
                }

                Widgets.CaptionLabel {
                    color: theme.fg.secondary
                    width: parent.width

                    text: root._getAlbumCaption()
                }

                Loader {
                    sourceComponent: buttonsRow
                }
            }
        }
    }

    function fetchAlbumData() {
        if (!section || albumModel.loading) return
        albumModel.getDataById(MediaLib.deserializeMlItemIdFromString(section)).then((albumData) => {
            _albumData = albumData
        })
    }

    function _getAlbumCaption() {
        const _albumData = root._albumData
        if (!_albumData)
            return ""

        const parts = []

        if (!root.pinnedStyle) {
            parts.push(_albumData.main_artist || qsTr("Unknown artist"))
        }

        const year = _albumData.release_year
        if (year)
            parts.push(year)

        const count = _albumData.nb_tracks ?? 0
        parts.push(qsTr(count < 2 ? "%1 track" : "%1 tracks").arg(count))

        const duration = _albumData.duration?.formatHMS()
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

            spacing: VLCStyle.margin_small

            Widgets.ActionButtonPrimary {
                id: _playActionBtn

                iconTxt: VLCIcons.play
                text: qsTr("Play")
                onClicked: {
                    MediaLib.addAndPlay( root._albumData.id )
                }

                Navigation.parentItem: root
                Navigation.rightItem: _enqueueActionBtn
                focus: true
            }

            Widgets.ButtonExt {
                id: _enqueueActionBtn

                iconTxt: VLCIcons.enqueue
                text: qsTr("Enqueue")
                onClicked: {
                    MediaLib.addToPlaylist( root._albumData.id )
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

            implicitWidth: parent.cover_width ?? VLCStyle.cover_xxsmall
            implicitHeight: parent.cover_height ?? VLCStyle.cover_xxsmall

            radius: parent.cover_radius ?? VLCStyle.expandCover_music_radius
            source: root._albumCover
            sourceSize: Qt.size(width * root.eDPR, height * root.eDPR)
            asynchronous: true

            Widgets.DefaultShadow {
                visible: (parent.status === Image.Ready)
            }
        }
    }
}
