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
import QtQuick.Templates as T
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import QtQml.Models
import QtQuick.Window


import VLC.MainInterface
import VLC.Style
import VLC.Playlist
import VLC.Widgets as Widgets
import VLC.Menus as Menus
import VLC.Util

T.ToolBar {
    id: root

    property int selectedIndex: 0
    property alias sortMenu: sortControl.menu
    property var menuDelegate: undefined

    // For now, used for d&d functionality
    // Not strictly necessary to set
    property PlaylistListView plListView: null

    property bool _showCSD: MainCtx.clientSideDecoration && !(MainCtx.intfMainWindow.visibility === Window.FullScreen)
    property bool _csdOnToolbarLine: _showCSD && !MainCtx.hasToolbarMenu

    hoverEnabled: true
    implicitHeight: implicitContentHeight
    implicitWidth: implicitContentWidth

    ColorContext {
        id: theme
        colorSet: ColorContext.Window
    }

    Binding {
        target: MainCtx.search
        property: "pattern"
        value: searchBox.searchPattern
    }

    Connections {
        target: MainCtx.search
        function onAskShow() {
            searchBox.expandAndFocus()
        }
    }

    background: Widgets.AcrylicBackground {
        tintColor: theme.bg.secondary
        alternativeColor: theme.bg.secondary
    }

    contentItem:  Item {
        implicitHeight: (MainCtx.hasToolbarMenu ? menubar.height : 0)
                        + (menuDelegateSmallScreen.visible ? menuDelegateSmallScreen.height : 0)
                        + globalToolbarContent.implicitHeight
                        + VLCStyle.applicationVerticalMargin

        //drag and dbl click the titlebar in CSD mode
        Loader {
            z:-1
            anchors.fill: parent
            active: root._showCSD
            source: "qrc:///qt/qml/VLC/Widgets/CSDTitlebarTapNDrapHandler.qml"
        }

        Menus.Menubar {
            id: menubar
            height: root._showCSD ? Math.max(implicitHeight, csdButtons.height) : implicitHeight

            anchors {
                left: parent.left
                right: root._showCSD ? csdButtons.left : parent.right
                top: parent.top
                topMargin: VLCStyle.applicationVerticalMargin
                rightMargin: root._showCSD ? 0 : VLCStyle.applicationHorizontalMargin
            }
            visible: MainCtx.hasToolbarMenu
            enabled: visible

            Navigation.parentItem: root
            Navigation.downItem: globalToolbarContent
        }

        Item {
            id: globalToolbarContent
            implicitHeight: VLCStyle.globalToolbar_height

            anchors {
                right: root._csdOnToolbarLine ? csdButtons.left : parent.right
                left: parent.left
                top: menubar.visible ? menubar.bottom : parent.top
                topMargin: menubar.visible ? 0 : VLCStyle.applicationVerticalMargin
                rightMargin: root._csdOnToolbarLine ? 0 : VLCStyle.applicationHorizontalMargin
            }

            Navigation.parentItem: root
            Navigation.upItem: menubar.visible ? menubar : null
            Navigation.downItem: menuDelegateSmallScreen.visible ? menuDelegateSmallScreen : null

            Row {
                id: globalToolbarLeft
                spacing: VLCStyle.margin_normal

                height: VLCStyle.globalToolbar_height

                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: VLCStyle.margin_xsmall
                }

                Accessible.role: Accessible.ToolBar

                Widgets.IconToolButton {
                     id: history_back

                     anchors.verticalCenter: parent.verticalCenter

                     font.pixelSize: VLCStyle.icon_banner
                     text: VLCIcons.back
                     description: qsTr("Previous")
                     height: VLCStyle.bannerButton_height
                     width: VLCStyle.bannerButton_width
                     onClicked: History.previous()
                     enabled: !History.previousEmpty

                     onEnabledChanged: {
                        if (!enabled && focus)
                            globalToolbarLeftContextGroup.focus = true
                     }

                     Navigation.parentItem: globalToolbarContent
                     Navigation.rightItem: globalToolbarLeftContextGroup
                }

                Widgets.NavigableRow {
                    id: globalToolbarLeftContextGroup

                    anchors.verticalCenter: parent.verticalCenter

                    spacing: VLCStyle.margin_normal
                    enabled: list_grid_btn.visible || sortControl.visible

                    onEnabledChanged: {
                        if (!enabled && focus) {
                            networkAddressbar = true
                        }
                    }

                    //TODO: visible value of MainCtx.sort.available and MainCtx.hasGridListMode is initialised correctly but on first load still shows up
                    model: ObjectModel {
                        Widgets.IconToolButton {
                            id: list_grid_btn

                            visible: MainCtx.hasGridListMode
                            // visible: true
                            enabled: visible
                            width: VLCStyle.bannerButton_width
                            height: VLCStyle.bannerButton_height
                            font.pixelSize: VLCStyle.icon_banner
                            text: MainCtx.gridView ? VLCIcons.list : VLCIcons.grid
                            description: qsTr("List/Grid")
                            onClicked: MainCtx.gridView = !MainCtx.gridView
                        }

                        Widgets.SortControl {
                            id: sortControl

                            visible: MainCtx.sort.available
                            enabled: visible
                            width: VLCStyle.bannerButton_width
                            height: VLCStyle.bannerButton_height
                            font.pixelSize: VLCStyle.icon_banner
                            description: qsTr("Sort")

                            model: MainCtx.sort.model

                            sortKey:  MainCtx.sort.criteria
                            sortOrder: MainCtx.sort.order

                            onSortSelected: (key) => {
                                MainCtx.sort.criteria = key
                            }
                            onSortOrderSelected: (type) => {
                                MainCtx.sort.order = type
                            }
                        }
                    }

                    Navigation.parentItem: globalToolbarContent
                    Navigation.leftItem: history_back
                    Navigation.rightItem: menuDelegateLargeScreen
                }
            }

            MenuDelegateLoader {
                id: menuDelegateLargeScreen

                //component is centered in globalToolbarContent, taking symetrically space up to the closet toolbar
                readonly property int _availableLeft: (parent.width / 2) - (globalToolbarLeft.x + globalToolbarLeft.width)
                readonly property int _availableRight: globalToolbarRight.x - (parent.width / 2)
                width: Math.max(0, 2 * Math.min(_availableLeft, _availableRight))
                height: VLCStyle.globalToolbar_height
                enabled: sourceComponent !== null
                visible: enabled
                sourceComponent: VLCStyle.isScreenSmall ? null : root.menuDelegate
                anchors {
                    horizontalCenter: parent.horizontalCenter
                    verticalCenter: parent.verticalCenter
                    rightMargin: VLCStyle.margin_xxsmall
                    leftMargin: VLCStyle.margin_xxsmall
                }

                onEnabledChanged: {
                    if (!enabled && focus) {
                        globalToolbarRight.focus = true
                    }
                }

                Navigation.parentItem: globalToolbarContent
                Navigation.leftItem: sortControl
                Navigation.rightItem: globalToolbarRight
            }

            Widgets.NavigableRow {
                id: globalToolbarRight

                anchors {
                    verticalCenter: parent.verticalCenter
                    right: parent.right
                    rightMargin: VLCStyle.margin_xsmall
                }
                
                spacing: VLCStyle.margin_normal

                model: ObjectModel {
                    Widgets.SearchBox {
                        id: searchBox

                        // set max width so that search field not overflows with small screens
                        // assumes all other sibling is a button of 'VLCStyle.bannerButton_width' width
                        maxSearchFieldWidth: root.width
                                             - (VLCStyle.bannerButton_width * globalToolbarRight.count)
                                             - (globalToolbarRight.spacing * (globalToolbarRight.count - 1))
                                             - globalToolbarRight.anchors.rightMargin
                                             - VLCStyle.margin_small // padding to left

                        //TODO: initialise visible value with MainCtx
                        // visible: MainCtx.search.available
                        visible: true
                        height: VLCStyle.bannerButton_height
                        buttonWidth: VLCStyle.bannerButton_width
                    }

                    Widgets.IconToolButton {
                        id: playlist_btn

                        checked: MainCtx.playlistVisible

                        font.pixelSize: VLCStyle.icon_banner
                        text: VLCIcons.playlist
                        description: qsTr("Playlist")
                        width: VLCStyle.bannerButton_width
                        height: VLCStyle.bannerButton_height
                        highlighted: MainCtx.playlistVisible

                        onClicked:  MainCtx.playlistVisible = !MainCtx.playlistVisible

                        DropArea {
                            anchors.fill: parent

                            onContainsDragChanged: {
                                if (containsDrag) {
                                    _timer.restart()

                                    if (plListView)
                                        MainCtx.setCursor(Qt.DragCopyCursor)
                                } else {
                                    _timer.stop()

                                    if (plListView)
                                        MainCtx.restoreCursor()
                                }
                            }

                            onEntered: (drag) => {
                                if (plListView) {
                                    console.assert(plListView.isDropAcceptableFunc)
                                    console.assert(plListView.model)
                                    if (plListView.isDropAcceptableFunc(drag, plListView.model.count)) {
                                        drag.accept()
                                    } else {
                                        drag.accepted = false
                                    }
                                } else {
                                    drag.accepted = false
                                }
                            }

                            onDropped: (drop) => {
                                if (plListView) {
                                    console.assert(plListView.acceptDropFunc)
                                    plListView.acceptDropFunc(plListView.model.count, drop)
                                }
                            }

                            Timer {
                                id: _timer
                                interval: VLCStyle.duration_humanMoment

                                onTriggered: {
                                    MainCtx.playlistVisible = true
                                }
                            }
                        }
                    }

                    Widgets.IconToolButton {
                        id: menu_selector

                        visible: !MainCtx.hasToolbarMenu
                        font.pixelSize: VLCStyle.icon_banner
                        text: VLCIcons.more
                        description: qsTr("Menu")
                        width: VLCStyle.bannerButton_width
                        height: VLCStyle.bannerButton_height
                        checked: contextMenu.shown

                        onClicked: contextMenu.popup(this.mapToGlobal(0, height))

                        Menus.QmlGlobalMenu {
                            id: contextMenu
                            ctx: MainCtx
                            playerViewVisible: History.match(History.viewPath, ["player"])
                        }
                    }
                }

                Navigation.parentItem: globalToolbarContent
                Navigation.leftItem: menuDelegateLargeScreen
            }

        }


        MenuDelegateLoader {
            id: menuDelegateSmallScreen

            height: VLCStyle.globalToolbar_height
            enabled: sourceComponent !== null
            visible: enabled
            sourceComponent: VLCStyle.isScreenSmall ? root.menuDelegate : null
            anchors {
                left: parent.left
                right: parent.right
                bottom: parent.bottom
            }

            onEnabledChanged: {
                if (!enabled && focus) {
                    globalToolbarRight.focus = true
                }
            }

            Navigation.parentItem: root
            Navigation.upItem: globalToolbarContent
        }

        Loader {
            id: csdButtons
            anchors.right: parent.right
            height: VLCStyle.globalToolbar_height
            active: root._showCSD
            source: VLCStyle.palette.hasCSDImage
                      ? "qrc:///qt/qml/VLC/Widgets/CSDThemeButtonSet.qml"
                      : "qrc:///qt/qml/VLC/Widgets/CSDWindowButtonSet.qml"
        }

        Keys.priority: Keys.AfterItem
        Keys.onPressed: (event) => root.Navigation.defaultKeyAction(event)
    }


    component MenuDelegateLoader : T.Pane {
        id: menuDelegate

        property alias sourceComponent: menuDelegateLoader.sourceComponent

        contentItem: Flickable {
            id: menuDelegateFlickable
            clip: contentWidth > width

            contentWidth: Math.max(menuDelegateLoader.width, menuDelegateFlickable.width)
            contentHeight: menuDelegateFlickable.height // don't allow vertical flickering

            Loader {
                id: menuDelegateLoader

                focus: true

                enabled: status === Loader.Ready
                //ensure the component is centered in the view
                y: status === Loader.Ready ? ((menuDelegateFlickable.contentHeight - item.height) / 2) : 0
                x: status === Loader.Ready ? ((menuDelegateFlickable.contentWidth - item.width) / 2) : 0
                width: !!item
                       ? Helpers.clamp(menuDelegateFlickable.width,
                                       item.minimumWidth || item.implicitWidth,
                                       item.maximumWidth || item.implicitWidth)
                       : 0

                onItemChanged: {
                    if (!item)
                        return
                    item.Navigation.parentItem = menuDelegate
                }
            }
        }
    }
}
