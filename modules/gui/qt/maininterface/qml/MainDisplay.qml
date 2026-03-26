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
    property real displayMargin: (height - miniPlayer.y)

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

    property bool _showCSD: MainCtx.clientSideDecoration
        && !(MainCtx.intfMainWindow.visibility === Window.FullScreen)

    // functions

    //MainDisplay behave as a PageLoader
    function loadView(path, properties, focusReason) {
        const found = stackView.loadView(path, properties, focusReason)
        if (!found)
            return

        if (Player.hasVideoOutput && MainCtx.hasEmbededVideo)
            _showMiniPlayer = true
    }

    function loadCurrentHistoryView(focusReason) {
        loadView(History.viewPath, History.viewProp, focusReason)
        contextSaver.restore(History.viewPath)
    }

    Component.onCompleted: {
        if (MainCtx.canShowVideoPIP) {
            pipPlayerComponent.createObject(this)
        } else {
            if (MainCtx.hasEmbededVideo)
                MainPlaylistController.stop()
        }

        if (History.previousEmpty) {
            History.update(["home"])
        }
        loadCurrentHistoryView(Qt.OtherFocusReason)
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

    readonly property var pageModel: [
        {
            name: "home",
            url: MainCtx.mediaLibraryAvailable ?
                 "qrc:///qt/qml/VLC/MediaLibrary/HomeDisplay.qml" :
                 "qrc:///qt/qml/VLC/MainInterface/NoMedialibHome.qml"
        }, {
            name: "video",
            url: "qrc:///qt/qml/VLC/MediaLibrary/VideoDisplay.qml"
        }, {
            name: "music",
            url: "qrc:///qt/qml/VLC/MediaLibrary/MusicDisplay.qml"
        }, {
            name: "network",
            url: "qrc:///qt/qml/VLC/Network/BrowseDisplay.qml"
        }, {
            name: "discover",
            url: "qrc:///qt/qml/VLC/Network/DiscoverDisplay.qml"
        }, {
            name: "mlsettings",
            url: "qrc:///qt/qml/VLC/MediaLibrary/MLFoldersSettings.qml"
        }
    ]

    ModelSortSettingHandler {
        id: contextSaver
    }

    Connections {
        target: MainCtx.sort

        function onCriteriaChanged(criteria) {
            contextSaver.save(History.viewPath)
        }

        function onOrderChanged(order) {
            contextSaver.save(History.viewPath)
        }
    }

    Connections {
        target: History
        function onNavigate(focusReason) {
            loadCurrentHistoryView(focusReason)
        }
    }

    ColorContext {
        id: theme
        palette: VLCStyle.palette
        colorSet: ColorContext.View
    }

    Loader {
        id: voronoiSnowLoader

        z: 3.5
        source: "qrc:///qt/qml/VLC/Widgets/VoronoiSnow.qml"
        anchors.fill: parent
        active: false

        function toggleActive() {
            voronoiSnowLoader.active = !voronoiSnowLoader.active
        }

        Component.onCompleted: {
            if (MainCtx.useXmasCone()) {
                MainCtx.kc_pressed.connect(voronoiSnowLoader.toggleActive)
            }
        }
    }

    MenuTopbar {
        id: menuTopbar
        z: 6

        visible: MainCtx.hasToolbarMenu
        enabled: visible

        anchors {
            top: parent.top
            right: parent.right
            left: parent.left
        }

        plListView: {
            if (playlistLoader.active)
                return playlistLoader.item
            else if (playlistWindowLoader.status === Loader.Ready)
                return playlistWindowLoader.item.playlistView
            else
                return null
        }
    }

    LocalTopbar {
        id: localTopbar
        z: 5

        anchors {
            top: menuTopbar.visible ? menuTopbar.bottom : parent.top
            left: parent.left
            right: parent.right
        }

        leftPadding: VLCStyle.applicationHorizontalMargin
        rightPadding: VLCStyle.applicationHorizontalMargin
        topPadding: menuTopbar.visible ? 0 : VLCStyle.applicationVerticalMargin

        plListView: {
            if (playlistLoader.active)
                return playlistLoader.item
            else if (playlistWindowLoader.status === Loader.Ready)
                return playlistWindowLoader.item.playlistView
            else
                return null
        }

        navigationVisible: pannelVisiblity.showNavigation
        playqueueVisible: pannelVisiblity.showPlayqueue

        Navigation.parentItem: g_mainDisplay
        Navigation.upItem: menuTopbar
        Navigation.downItem: stackView.currentItem

        onToggleNavigationVisibility: pannelVisiblity.toggleNavigationVisibility()
        onTogglePlayqueueVisibility:  pannelVisiblity.togglePlayqueueVisibility()
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

        z: 1

        anchors {
            top: localTopbar.bottom
            right: parent.right
            left: parent.left
            bottom: parent.bottom
        }

        implicitWidth: stackView.implicitWidth
        implicitHeight: stackView.implicitHeight

        color: theme.bg.primary

        layer.enabled: MainCtx.backdropBlurRequested() &&
                       (GraphicsInfo.shaderType === GraphicsInfo.RhiShader) &&
                       miniPlayer.visible

        // Blurring requires to access neighbour pixels, thus the source texture should be bigger than
        // the effect so that the effect have access to the neighbor pixels for the pixels near the
        // border, where the extra size would depend on the blur configuration. When the source is
        // static, this problem is harder to notice, but when the source is not static, such as
        // during scrolling, not considering this causes glitches in the bottom side. `PartialEffect`,
        // since 03b0de26, already provides the effect the whole source texture with a proper sub-rect,
        // so the effect here can sample the top edge neighbour pixels, but for the bottom edge we
        // need to configure the layer:
        readonly property int edgeExtension: 16
        layer.sourceRect: Qt.rect(0, 0, Math.min(stackView.width + edgeExtension, stackViewParent.width), height + edgeExtension)

        Rectangle {
            // Extension of parent rectangle for the bottom extension.
            anchors.top: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: stackViewParent.edgeExtension
            visible: stackViewParent.layer.enabled && (height > 0)
            color: parent.color
        }

        layer.effect: Widgets.PartialEffect {
            id: stackViewParentLayerEffect

            blending: stackViewParent.color.a < (1.0 - Number.EPSILON)

            // Each pass of the blur effect also suffers from the border neighbour pixel issue mentioned
            // above, making all the borders problematic, to a less considerable extent. For that reason,
            // we extend both the top and the bottom edges and use viewport to prevent overdraw:
            effectRect: Qt.rect(0,
                                stackView.height - stackViewParent.edgeExtension,
                                width,
                                miniPlayer.height + 2 * stackViewParent.edgeExtension)

            // Edge extension is not necessary here, but it is provided to prevent stretching glitch at
            // initialization. Currently this is not a problem because the effect is opaque since the
            // background is opaque, and effect visual has higher z than the source visual.
            sourceVisualRect: Qt.rect(0, 0,
                                      stackViewParent.layer.sourceRect.width,
                                      stackView.height + (frostedGlassEffect.blending ? 0 : stackViewParent.edgeExtension))

            effect: frostedGlassEffect

            Widgets.FrostedGlassEffect {
                id: frostedGlassEffect

                ColorContext {
                    id: frostedTheme
                    palette: VLCStyle.palette
                    colorSet: ColorContext.Window
                }

                backgroundColor: (ready ? "transparent" : stackViewParent.color)
                tint: frostedTheme.bg.secondary

                // Prevent overdraw (the extension margin should not be painted).
                // This also saves video memory compared to solely using visual rect.
                viewportRect: Qt.rect(0,
                                      stackViewParent.edgeExtension,
                                      Math.min(stackView.width + stackViewParent.edgeExtension, stackViewParent.width),
                                      height - (2 * stackViewParent.edgeExtension))

                visualRect: (stackView.width < stackViewParent.width) ? Qt.rect(viewportRect.x,
                                                                                viewportRect.y,
                                                                                width,
                                                                                viewportRect.height)
                                                                      : Qt.rect(0, 0, 0, 0)
            }
        }

        Widgets.PageLoader {
            id: stackView

            focus: true

            anchors.fill: parent
            anchors.leftMargin: (sidebar.visible && !VLCStyle.isScreenSmall) ? sidebar.width : 0
            anchors.rightMargin: (playlistLoader.shown && !VLCStyle.isScreenSmall)
                                 ? playlistLoader.width
                                 : 0
            anchors.bottomMargin: g_mainDisplay.displayMargin

            pageModel: g_mainDisplay.pageModel

            leftPadding: sidebar.visible  ? 0 : VLCStyle.applicationHorizontalMargin

            rightPadding: playlistLoader.shown ? 0 : VLCStyle.applicationHorizontalMargin

            onCurrentItemChanged: {
                if (currentItem) {
                    {
                        // Main pages need to compensate for the mini player:

                        if (currentItem.displayMarginEnd !== undefined)
                            currentItem.displayMarginEnd = Qt.binding(() => { return g_mainDisplay.displayMargin })

                        if (currentItem.enableEndFade !== undefined)
                            currentItem.enableEndFade = Qt.binding(() => { return (g_mainDisplay.hasMiniPlayer === false) })
                    }
                }
            }

            Navigation.parentItem: g_mainDisplay
            Navigation.upItem: localTopbar
            Navigation.rightItem: playlistLoader
            Navigation.leftItem: sidebar
            Navigation.downItem:  miniPlayer.visible ? miniPlayer : null
        }
    }

    Rectangle {
        // overlay for smallscreens
        z: 2

        anchors.fill: parent
        visible: VLCStyle.isScreenSmall && (playlistLoader.shown || sidebar.visible)
        color: "black"
        opacity: 0.4

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            onClicked:  pannelVisiblity.hideVisiblePanels()

            // Capture WheelEvents before they reach stackView
            onWheel: (wheel) => {
                wheel.accepted = true
            }
        }
    }

    SideNavigationPane {
        id: sidebar

        z: 3

        anchors {
            top : localTopbar.bottom
            left: parent.left
            bottom: miniPlayer.top
        }

        property int maximumWidth: (g_mainDisplay.width + sidebarResizeHandle.width) / 3

        //topPadding: VLCStyle.isScreenSmall ? 0 : localTopbar.height
        bottomPadding: VLCStyle.applicationVerticalMargin + VLCStyle.margin_small

        safeAreaLeftMargin: VLCStyle.applicationHorizontalMargin

        useAcrylic: !VLCStyle.isScreenSmall

        onItemClicked: (modelUri) => {
            if (stackView.isDefaulLoadedForPath([...modelUri]) ||
                !!modelUri.length && History.exactMatch(History.viewPath, modelUri)) {
                stackView.positionContentAtBeginning()
                return
            }

            History.push(modelUri)
        }

        implicitWidth: Math.round(VLCStyle.isScreenSmall
                       ? g_mainDisplay.width * 0.8
                       : Helpers.clamp(MainCtx.navigationPanel.width, minimumWidth, maximumWidth))

        Navigation.parentItem: g_mainDisplay
        Navigation.upItem: localTopbar
        Navigation.rightItem: stackView.currentItem

        state: pannelVisiblity.showNavigation ? "expanded" : "retracted"

        Component.onCompleted: {
            Qt.callLater(() => { sidebarTransition.enabled = true; })
        }

        states: [
            State {
                name: "expanded"
                PropertyChanges {
                    target: sidebar
                    width: sidebar.implicitWidth
                    visible: true
                }
            },
            State {
                name: "retracted"
                PropertyChanges {
                    target: sidebar
                    width: 0
                    visible: false
                }
            }
        ]

        transitions: Transition {
            id: sidebarTransition
            enabled: false

            from: "retracted"; to: "expanded";
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

        PaneResizeHandle {
            id: sidebarResizeHandle

            parent: sidebar
            target: sidebar

            panelObject: MainCtx.navigationPanel
            atRight: true

            minimumWidth: sidebar.minimumWidth
            maximumWidth: sidebar.maximumWidth

            anchors {
                top: parent.top
                bottom: parent.bottom
                right: parent.right
            }
        }
    }

    Loader {
        id: playlistLoader

        z: 3
        anchors {
            top: localTopbar.bottom
            right: parent.right
            bottom: miniPlayer.top
        }

        width: 0
        height: parent.height - (
                (menuTopbar.visible ? menuTopbar.height : 0)
                + g_mainDisplay.displayMargin)

        visible: false

        active: MainCtx.playqueuePanel.docked

        state: ((status === Loader.Ready) && pannelVisiblity.showPlayqueue) ? "expanded" : "retracted"

        readonly property bool shown: (status === Loader.Ready) && item.visible

        Component.onCompleted: {
            Qt.callLater(() => { playlistTransition.enabled = true; })
        }

        states: [
            State {
                name: "expanded"
                PropertyChanges {
                    target: playlistLoader
                    width: playlistLoader.implicitWidth
                    visible: true
                }
           }, State {
                name: "retracted"
                PropertyChanges {
                    target: playlistLoader
                    width: 0
                    visible: false
                }
           }
        ]

        transitions: Transition {
            id: playlistTransition
            enabled: false

            from: "retracted"; to: "expanded";
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

        sourceComponent: PlaylistPane {
            id: playlist

            implicitWidth: Math.round(VLCStyle.isScreenSmall
                           ? g_mainDisplay.width * 0.8
                           : MainCtx.playqueuePanel.width)

            property int maximumWidth: (g_mainDisplay.width + playqueueResizeHandle.width ) / 3

            focus: true

            leftPadding: playqueueResizeHandle.width
            rightPadding: VLCStyle.applicationHorizontalMargin
            bottomPadding: VLCStyle.margin_normal + Math.max(VLCStyle.applicationVerticalMargin - g_mainDisplay.displayMargin, 0)

            useAcrylic: !VLCStyle.isScreenSmall

            Navigation.parentItem: g_mainDisplay
            Navigation.upItem: localTopbar
            Navigation.downItem: miniPlayer.visible ? miniPlayer : null
            Navigation.leftItem: stackView.currentItem

            Navigation.cancelAction: function() {
                MainCtx.playqueuePanel.visible = false
                stackView.forceActiveFocus()
            }

            PaneResizeHandle {
                id: playqueueResizeHandle

                parent: playlist
                target: playlist

                minimumWidth: playlist.minimumWidth
                maximumWidth: playlist.maximumWidth

                panelObject: MainCtx.playqueuePanel
                atRight: false

                anchors {
                    top: parent.top
                    bottom: parent.bottom
                    left: parent.left
                }
            }
        }
    }

    //track the visiblity state of the side panels
    //FIXME do we want proper state machine?
    Item {
        id: pannelVisiblity
        property bool showNavigation: false
        property bool showPlayqueue: false

        Component.onCompleted: {
            onIsScreenSmallChanged()
        }

        onShowNavigationChanged: {
            if (VLCStyle.isScreenSmall && pannelVisiblity.showPlayqueue && MainCtx.playqueuePanel.docked && pannelVisiblity.showNavigation) {
                pannelVisiblity.showPlayqueue = false
            }
        }

        onShowPlayqueueChanged: {
            if (VLCStyle.isScreenSmall && pannelVisiblity.showPlayqueue && MainCtx.playqueuePanel.docked && pannelVisiblity.showNavigation) {
                pannelVisiblity.showNavigation = false
            }
        }

        function hideVisiblePanels() {
            pannelVisiblity.showNavigation = false
            MainCtx.navigationPanel.visible = false
            if (MainCtx.playqueuePanel.docked) {
                pannelVisiblity.showPlayqueue = false
                MainCtx.playqueuePanel.visible = false
            }
        }

        function toggleNavigationVisibility() {
            showNavigation = !showNavigation
            MainCtx.navigationPanel.visible = showNavigation
        }

        function togglePlayqueueVisibility() {
            showPlayqueue = !showPlayqueue
            MainCtx.playqueuePanel.visible = showPlayqueue
        }

        function onIsScreenSmallChanged() {
            //when screen becomes small hide side panels
            if (VLCStyle.isScreenSmall) {

                pannelVisiblity.showNavigation = false

                if (MainCtx.playqueuePanel.docked)
                    pannelVisiblity.showPlayqueue = false
                else
                    pannelVisiblity.showPlayqueue = MainCtx.playqueuePanel.visible
            } else {
                //reshow the navigation panels to original state
                pannelVisiblity.showNavigation = MainCtx.navigationPanel.visible
                pannelVisiblity.showPlayqueue =  MainCtx.playqueuePanel.visible
            }
        }

        Connections {
            target: VLCStyle

            function onIsScreenSmallChanged() {
                pannelVisiblity.onIsScreenSmallChanged()
            }
        }

        Connections {
            target: MainCtx.playqueuePanel

            function onVisibleChanged() {
                pannelVisiblity.showPlayqueue = MainCtx.playqueuePanel.visible
            }
        }

        Connections {
            target: MainCtx.navigationPanel

            function onVisibleChanged() {
                pannelVisiblity.showNavigation = MainCtx.navigationPanel.visible
            }
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
            z: 4
            visible: g_mainDisplay._showMiniPlayer && MainCtx.hasEmbededVideo
            enabled: g_mainDisplay._showMiniPlayer && MainCtx.hasEmbededVideo

            dragXMin: 0
            dragXMax: g_mainDisplay.width - playerPip.width
            dragYMin: localTopbar.y + localTopbar.height
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

    Widgets.FloatingNotification {
        id: notif
        z: 11

        anchors {
            bottom: miniPlayer.top
            left: parent.left
            right: parent.right
            margins: VLCStyle.margin_large
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
        onVisibleChanged: {
            if (!visible && miniPlayer.activeFocus)
                stackView.forceActiveFocus()
        }
    }

    Connections {
        target: Player
        function onHasVideoOutputChanged() {
            if (Player.hasVideoOutput && MainCtx.hasEmbededVideo) {
                MainCtx.playerView = true
            } else {
                _showMiniPlayer = false;
            }
        }
    }

    component PaneResizeHandle: Item {
        id: paneResizeHandle
        required property Item target
        required property QtObject panelObject
        property alias atRight: resizeHandle.atRight

        property alias minimumWidth: resizeHandle.minimumWidth
        property alias maximumWidth: resizeHandle.maximumWidth

        implicitWidth: resizeHandle.width

        Rectangle {
            id: visualBorder

            anchors {
                top: parent.top
                bottom: parent.bottom
                left: resizeHandle.atRight ? undefined : parent.left
                right: resizeHandle.atRight ? parent.right: undefined
            }

            width: VLCStyle.border
            color: theme.separator
        }

        Widgets.HorizontalResizeHandle {
            id: resizeHandle

            property bool _inhibitMainInterfaceUpdate: false

            anchors {
                top: parent.top
                bottom: parent.bottom
                left: resizeHandle.atRight ? undefined : parent.left
                right: resizeHandle.atRight ? parent.right: undefined
            }

            atRight: false
            currentWidth: target.width

            visible: !VLCStyle.isScreenSmall

            onRequestedWidthChanged: {
                if (!_inhibitMainInterfaceUpdate)
                    paneResizeHandle.panelObject.width = requestedWidth
            }

            Component.onCompleted:  _updateFromMainInterface()

            function _updateFromMainInterface() {
                if (requestedWidth === paneResizeHandle.panelObject.width)
                    return

                _inhibitMainInterfaceUpdate = true
                requestedWidth = paneResizeHandle.panelObject.width
                _inhibitMainInterfaceUpdate = false
            }

            Connections {
                target: paneResizeHandle.panelObject

                function onWidthChanged() {
                    resizeHandle._updateFromMainInterface()
                }
            }
        }
    }

    MouseArea {
        /// handles mouse navigation buttons
        z:9
        anchors.fill: parent
        acceptedButtons: Qt.BackButton
        cursorShape: undefined
        onClicked: History.previous()
    }
}
