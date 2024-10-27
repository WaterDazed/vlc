/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
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
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects


import VLC.Style
import VLC.MainInterface
import VLC.Widgets as Widgets
import VLC.Playlist
import VLC.Player

import VLC.Util
import VLC.Dialogs

FocusScope {
    id: g_mainDisplay

    // Properties

    property bool hasMiniPlayer: miniPlayer.visible

    // NOTE: The main view must be above the indexing bar and the mini player.
    property real displayMargin: (height - miniPlayer.y) + (loaderProgress.active ? loaderProgress.height : 0)

    //MainDisplay behave as a PageLoader
    property alias pagePrefix: stackView.pagePrefix

    readonly property int positionSliderY: {
        var size = miniPlayer.y + miniPlayer.sliderY

        if (MainCtx.pinVideoControls)
            return size - VLCStyle.margin_xxxsmall
        else
            return size
    }

    property bool _showMiniPlayer: false

    // functions

    //MainDisplay behave as a PageLoader
    function loadView(path, properties, focusReason) {
        const found = stackView.loadView(path, properties, focusReason)
        if (!found)
            return

        const item = stackView.currentItem

        globalTopbar.menuDelegate = Qt.binding(function () {
            return item.menuDelegate ?? null
        })

        // NOTE: sortMenu is declared with the SortMenu type, so when it's undefined we have to
        //       return null to avoid a QML warning.
        globalTopbar.sortMenu = Qt.binding(function () {
            return item.sortMenu ?? null
        })

        MainCtx.hasGridListMode = Qt.binding(() => item.hasGridListMode !== undefined && item.hasGridListMode)
        MainCtx.search.available = Qt.binding(() => item.isSearchable !== undefined && item.isSearchable)
        MainCtx.sort.model = Qt.binding(function () { return item.sortModel })
        MainCtx.sort.available = Qt.binding(function () { return Helpers.isArray(item.sortModel) && item.sortModel.length > 0 })

        if (Player.hasVideoOutput && MainCtx.hasEmbededVideo)
            _showMiniPlayer = true
    }

    Component.onCompleted: {
        if (MainCtx.canShowVideoPIP)
            pipPlayerComponent.createObject(this)
    }

    Navigation.cancelAction: function() {
        History.previous(Qt.BacktabFocusReason)
    }

    Keys.onPressed: (event) => {
        if (KeyHelper.matchSearch(event)) {
            MainCtx.search.askShow()
            event.accepted = true
        }
        //unhandled keys are forwarded as hotkeys
        if (!event.accepted)
            MainCtx.sendHotkey(event.key, event.modifiers);
    }

    layer.enabled: (StackView.status === StackView.Deactivating || StackView.status === StackView.Activating)

    readonly property var pageModel: [
        {
            listed: MainCtx.mediaLibraryAvailable,
            displayText: qsTr("Video"),
            icon: VLCIcons.topbar_video,
            name: "video",
            url: "qrc:///qt/qml/VLC/MediaLibrary/VideoDisplay.qml"
        }, {
            listed: MainCtx.mediaLibraryAvailable,
            displayText: qsTr("Music"),
            icon: VLCIcons.topbar_music,
            name: "music",
            url: "qrc:///qt/qml/VLC/MediaLibrary/MusicDisplay.qml"
        }, {
            listed: !MainCtx.mediaLibraryAvailable,
            displayText: qsTr("Home"),
            icon: VLCIcons.home,
            name: "home",
            url: "qrc:///qt/qml/VLC/MainInterface/NoMedialibHome.qml"
        }, {
            listed: true,
            displayText: qsTr("Browse"),
            icon: VLCIcons.topbar_network,
            name: "network",
            url: "qrc:///qt/qml/VLC/Network/BrowseDisplay.qml"
        }, {
            listed: true,
            displayText: qsTr("Discover"),
            icon: VLCIcons.topbar_discover,
            name: "discover",
            url: "qrc:///qt/qml/VLC/Network/DiscoverDisplay.qml"
        }, {
            listed: false,
            name: "mlsettings",
            url: "qrc:///qt/qml/VLC/MediaLibrary/MLFoldersSettings.qml"
        }
    ]

    property ListModel tabModel: ListModel {
        id: tabModelid
        Component.onCompleted: {
            pageModel.forEach(function(e) {
                if (!e.listed)
                    return
                append({
                           displayText: e.displayText,
                           icon: e.icon,
                           name: e.name,
                       })
            })
        }
    }

    ColorContext {
        id: theme
        palette: VLCStyle.palette
        colorSet: ColorContext.View
    }

    Item {

        anchors {
            top: parent.top
            right: parent.right
            left: parent.left
            bottom: parent.bottom
        }

        Rectangle {
            id: stackViewParent

            // This rectangle is used to display the effect in
            // the area of miniplayer background.
            // We can not directly apply the effect on the
            // view because its size is limited and the effect
            // should exceed the size. Also, it is beneficial
            // to have a rectangle here because if the background
            // is transparent we would lose subpixel font rendering
            // support.

            anchors.fill: parent

            implicitWidth: stackView.implicitWidth
            implicitHeight: stackView.implicitHeight

            color: theme.bg.primary

            layer.enabled: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader) &&
                           (miniPlayer.visible || (loaderProgress.active && loaderProgress.item.visible))

            layer.effect: Widgets.PartialEffect {
                id: stackViewParentLayerEffect

                blending: stackViewParent.color.a < (1.0 - Number.EPSILON)

                effectRect: Qt.rect(0,
                                    stackView.height,
                                    width,
                                    height - stackView.height)

                effectLayer.effect: Component {
                    Widgets.FrostedGlassEffect {
                        ColorContext {
                            id: frostedTheme
                            palette: VLCStyle.palette
                            colorSet: ColorContext.Window
                        }

                        blending: stackViewParentLayerEffect.blending

                        tint: frostedTheme.bg.secondary
                    }
                }
            }

            GlobalTopbar {
                id: globalTopbar
                z: 2

                anchors {
                    top: stackViewParent.top
                    right: stackViewParent.right
                    left: VLCStyle.isScreenSmall ? stackViewParent.left : sidebar.right
                }

                plListView: playlistLoader.active ? playlistLoader.item
                    : (playlistWindowLoader.status === Loader.Ready ? playlistWindowLoader.item.playlistView
                    : null)
            }

            NavigationPane {
                id: sidebar
                z: 99

                anchors {
                    top: VLCStyle.isScreenSmall ? globalTopbar.bottom : stackViewParent.top
                    left: stackViewParent.left
                    bottom: stackViewParent.bottom
                }

                Rectangle {
                    id: sidebarRightBorder

                    parent: sidebar

                    anchors {
                        top: parent.top
                        bottom: parent.bottom
                        right: parent.right
                    }

                    width: VLCStyle.border
                    color: theme.separator
                }

                onItemClicked: (sectionUri, modelUri) => {
                    if (stackView.isDefaulLoadedForPath([modelUri]))
                        return;

                    if (sectionUri === "undefined")
                        History.push(["mc", modelUri])
                    else
                        History.push(["mc", sectionUri, modelUri])
                }
            }

            Widgets.PageLoader {
                id: stackView

                focus: true

                anchors  {
                    top: globalTopbar.bottom
                    bottom: parent.bottom
                    left: sidebar.right
                    right: parent.right
                }
                anchors.rightMargin: (playlistLoader.shown && !VLCStyle.isScreenSmall)
                                     ? playlistLoader.width
                                     : 0
                anchors.bottomMargin: g_mainDisplay.displayMargin

                pageModel: g_mainDisplay.pageModel

                leftPadding: VLCStyle.applicationHorizontalMargin

                rightPadding: playlistLoader.shown
                              ? 0
                              : VLCStyle.applicationHorizontalMargin


                Navigation.parentItem: g_mainDisplay
                Navigation.upItem: globalTopbar
                Navigation.rightItem: playlistLoader
                Navigation.downItem:  miniPlayer.visible ? miniPlayer : null
            }

            Rectangle {
                // overlay for smallscreens

                anchors  {
                    //top: VLCStyle.isScreenSmall ? globalTopbar.bottom : parent.top 
                    top: globalTopbar.bottom
                    bottom: parent.bottom
                    left: sidebar.right
                    right: parent.right
                }
                visible: VLCStyle.isScreenSmall && playlistLoader.shown
                color: "black"
                opacity: 0.4

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: {
                        MainCtx.playlistVisible = false
                    }

                    // Capture WheelEvents before they reach stackView
                    onWheel: {
                        wheel.accepted = true
                    }
                }
            }

            Loader {
                id: playlistLoader

                anchors {
                    top: globalTopbar.bottom
                    right: parent.right
                    bottom: miniPlayer.visible ? miniPlayer.top : null
                }

                width: 0
                height: parent.height - g_mainDisplay.displayMargin

                visible: false

                active: MainCtx.playlistDocked

                state: ((status === Loader.Ready) && MainCtx.playlistVisible) ? "expanded" : ""

                readonly property bool shown: (status === Loader.Ready) && item.visible

                Component.onCompleted: {
                    Qt.callLater(() => { playlistTransition.enabled = true; })
                }

                states: State {
                    name: "expanded"
                    PropertyChanges {
                        target: playlistLoader
                        width: Math.round(playlistLoader.implicitWidth)
                        visible: true
                    }
                }

                transitions: Transition {
                    id: playlistTransition
                    enabled: false

                    from: ""; to: "expanded";
                    reversible: true

                    SequentialAnimation {
                        PropertyAction { property: "visible" }

                        NumberAnimation {
                            property: "width"
                            duration: VLCStyle.duration_short
                            easing.type: Easing.InOutSine
                        }
                    }
                }

                sourceComponent: PlaylistListView {
                    id: playlist

                    implicitWidth: VLCStyle.isScreenSmall
                                   ? g_mainDisplay.width * 0.8
                                   : Helpers.clamp(g_mainDisplay.width / resizeHandle.widthFactor,
                                                   minimumWidth,
                                                   g_mainDisplay.width / 2 + playlistLeftBorder.width / 2)

                    focus: true

                    leftPadding: playlistLeftBorder.width
                    rightPadding: VLCStyle.applicationHorizontalMargin
                    topPadding: VLCStyle.layoutTitle_top_padding
                    bottomPadding: VLCStyle.margin_normal + Math.max(VLCStyle.applicationVerticalMargin - g_mainDisplay.displayMargin, 0)

                    Navigation.parentItem: g_mainDisplay
                    Navigation.upItem: globalTopbar
                    Navigation.downItem: miniPlayer.visible ? miniPlayer : null

                    Navigation.leftAction: function() {
                        stackView.currentItem.setCurrentItemFocus(Qt.TabFocusReason);
                    }

                    Navigation.cancelAction: function() {
                        MainCtx.playlistVisible = false
                        stackView.forceActiveFocus()
                    }

                    Rectangle {
                        id: playlistLeftBorder

                        parent: playlist

                        anchors {
                            top: parent.top
                            bottom: parent.bottom
                            left: parent.left
                        }

                        width: VLCStyle.border
                        color: theme.separator

                        visible: playlistLoader.shown
                    }

                    Widgets.HorizontalResizeHandle {
                        id: resizeHandle

                        property bool _inhibitMainInterfaceUpdate: false

                        parent: playlist

                        anchors {
                            top: parent.top
                            bottom: parent.bottom
                            left: parent.left
                        }

                        atRight: false
                        targetWidth: parent.width
                        sourceWidth: g_mainDisplay.width

                        onWidthFactorChanged: {
                            if (!_inhibitMainInterfaceUpdate)
                                MainCtx.setPlaylistWidthFactor(widthFactor)
                        }

                        Component.onCompleted:  _updateFromMainInterface()

                        function _updateFromMainInterface() {
                            if (widthFactor == MainCtx.playlistWidthFactor)
                                return

                            _inhibitMainInterfaceUpdate = true
                            widthFactor = MainCtx.playlistWidthFactor
                            _inhibitMainInterfaceUpdate = false
                        }

                        Connections {
                            target: MainCtx

                            function onPlaylistWidthFactorChanged() {
                                resizeHandle._updateFromMainInterface()
                            }
                        }
                    }
                }

                Connections {
                    target: VLCStyle

                    function onAppWidthChanged() { 
                        if (VLCStyle.isScreenSmall && MainCtx.playlistVisible) {
                            MainCtx.playlistVisible = false
                        }
                    }
                    function onAppHeightChanged() {
                        if (VLCStyle.isScreenSmall && MainCtx.playlistVisible ) {
                            MainCtx.playlistVisible = false
                        }
                    }
                }
            }
        }
    }

    Loader {
        id: loaderProgress

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: miniPlayer.top

        active: (MainCtx.mediaLibraryAvailable && MainCtx.mediaLibrary.idle === false)

        height: active ? implicitHeight : 0

        source: "qrc:///qt/qml/VLC/Widgets/ScanProgressBar.qml"

        onLoaded: {
            item.background.visible = Qt.binding(function() { return !stackViewParent.layer.enabled })

            item.leftPadding = Qt.binding(function() { return VLCStyle.margin_large + VLCStyle.applicationHorizontalMargin })
            item.rightPadding = Qt.binding(function() { return VLCStyle.margin_large + VLCStyle.applicationHorizontalMargin })
            item.bottomPadding = Qt.binding(function() { return VLCStyle.margin_small + (miniPlayer.visible ? 0 : VLCStyle.applicationVerticalMargin) })
        }
    }

    Component {
        id: pipPlayerComponent

        PIPPlayer {
            id: playerPip
            anchors {
                bottom: miniPlayer.top
                left: parent.left
                bottomMargin: VLCStyle.margin_normal
                leftMargin: VLCStyle.margin_normal + VLCStyle.applicationHorizontalMargin
            }

            width: VLCStyle.dp(320, VLCStyle.scale)
            height: VLCStyle.dp(180, VLCStyle.scale)
            z: 2
            visible: g_mainDisplay._showMiniPlayer && MainCtx.hasEmbededVideo
            enabled: g_mainDisplay._showMiniPlayer && MainCtx.hasEmbededVideo

            dragXMin: 0
            dragXMax: g_mainDisplay.width - playerPip.width
            dragYMin: globalTopbar.y + globalTopbar.height
            dragYMax: miniPlayer.y - playerPip.height

            //keep the player visible on resize
            Connections {
                target: g_mainDisplay
                function onWidthChanged() {
                    if (playerPip.x > playerPip.dragXMax)
                        playerPip.x = playerPip.dragXMax
                }
                function onHeightChanged() {
                    if (playerPip.y > playerPip.dragYMax)
                        playerPip.y = playerPip.dragYMax
                }
            }
        }
    }

    Dialogs {
        z: 10
        bgContent: g_mainDisplay

        anchors {
            bottom: miniPlayer.visible ? miniPlayer.top : parent.bottom
            left: parent.left
            right: parent.right
        }
    }

    MiniPlayer {
        id: miniPlayer

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        z: 3

        horizontalPadding: VLCStyle.applicationHorizontalMargin
        bottomPadding: VLCStyle.applicationVerticalMargin + VLCStyle.margin_xsmall

        background.visible: !stackViewParent.layer.enabled

        Navigation.parentItem: g_mainDisplay
        Navigation.upItem: stackView
        Navigation.cancelItem:globalTopbar
        onVisibleChanged: {
            if (!visible && miniPlayer.activeFocus)
                stackView.forceActiveFocus()
        }
    }

    Connections {
        target: Player
        function onHasVideoOutputChanged() {
            if (Player.hasVideoOutput && MainCtx.hasEmbededVideo) {
                if (!History.match(History.viewPath, ["player"]))
                    History.push(["player"])
            } else {
                _showMiniPlayer = false;
            }
        }
    }
}
