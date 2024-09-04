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
    property alias menuDelegate: browseAddressbar.sourceComponent

    // For now, used for d&d functionality
    // Not strictly necessary to set
    property PlaylistListView plListView: null

    property bool _showCSD: MainCtx.clientSideDecoration && !(MainCtx.intfMainWindow.visibility === Window.FullScreen)

    height: VLCStyle.applicationVerticalMargin
            + (menubar.visible ? menubar.height : 0)
            + VLCStyle.globalToolbar_height

    hoverEnabled: true

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
        tintColor: theme.bg.primary
        alternativeColor: theme.bg.secondary
    }

    Menus.Menubar {
        id: menubar
        width: parent.width
        height: implicitHeight
        visible: MainCtx.hasToolbarMenu
        enabled: visible
    }

    contentItem:  Item {
        id: globalToolbar
        clip: true

        anchors {
            fill: parent
            topMargin: VLCStyle.applicationVerticalMargin
        }

        Item {
            id: globalToolbarContent
            implicitWidth: parent.width
            height: VLCStyle.globalToolbar_height
                    + (menubar.visible ? menubar.height : 0)

            anchors {
                right: globalToolbarRight.left
                rightMargin: VLCStyle.applicationHorizontalMargin
                left: parent.left
            }

            //drag and dbl click the titlebar in CSD mode
            Loader {
                anchors.fill: parent
                active: root._showCSD
                source: "qrc:///qt/qml/VLC/Widgets/CSDTitlebarTapNDrapHandler.qml"
            }

            Item {
                id: globalToolbarLeft
                width: parent.width
                height: VLCStyle.globalToolbar_height

                Accessible.role: Accessible.ToolBar

                anchors {
                    left: parent.left
                    verticalCenter: parent.verticalCenter
                    leftMargin: VLCStyle.margin_xsmall
                    right: networkAddressbar.visible ? networkAddressbar.right : globalToolbarRightContextGroup.left
                }

                Row {
                    anchors.fill: parent
                    spacing: VLCStyle.margin_normal

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

                         Navigation.parentItem: root
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

                        Navigation.parentItem: root
                        Navigation.leftItem: history_back 
                        Navigation.rightItem: globalToolbarRightContextGroup
                    }
                }
            }

            T.Pane {
                id: networkAddressbar

                property int _availableWidth: parent.width
                                              - (globalToolbarLeft.width + globalToolbarRightContextGroup.width)
                                              - (VLCStyle.applicationHorizontalMargin * 2)
                                              - (VLCStyle.margin_xsmall * 2)
                                              - (VLCStyle.margin_xxsmall * 2)

                width: Math.min(contentItem.contentWidth, _availableWidth)
                height: VLCStyle.globalToolbar_height
                enabled: browseAddressbar.sourceComponent !== null
                visible: enabled

                anchors {
                    horizontalCenter: parent.horizontalCenter
                    verticalCenter: parent.verticalCenter
                    rightMargin: VLCStyle.margin_xxsmall 
                }

                onEnabledChanged: {
                    if (!enabled && focus) {
                        globalToolbarRightContextGroup.focus = true
                    }
                }

                Navigation.parentItem: root
                Navigation.leftItem: sortControl
                Navigation.rightItem: globalToolbarRightContextGroup

                contentItem: Flickable {

                    clip: contentWidth > width

                    contentWidth: browseAddressbar.width
                    contentHeight: VLCStyle.globalToolbar_height // don't allow vertical flickering

                    Loader {
                        id: browseAddressbar

                        focus: true

                        enabled: status === Loader.Ready
                        y: status === Loader.Ready ? (VLCStyle.globalToolbar_height - item.height) / 2 : 0
                        width: !!item
                               ? Helpers.clamp(networkAddressbar._availableWidth,
                                               browseAddressbar.item.minimumWidth || browseAddressbar.item.implicitWidth,
                                               browseAddressbar.item.maximumWidth || browseAddressbar.item.implicitWidth)
                               : 0

                        onItemChanged: {
                            if (!item)
                                return
                            item.Navigation.parentItem = networkAddressbar
                        }
                    }
                }
            }

            Widgets.NavigableRow {
                id: globalToolbarRightContextGroup

                anchors {
                    verticalCenter: parent.verticalCenter
                    right: parent.right
                    rightMargin: VLCStyle.applicationHorizontalMargin + VLCStyle.margin_xsmall
                }
                
                spacing: VLCStyle.margin_normal

                model: ObjectModel {
                    Widgets.SearchBox {
                        id: searchBox

                        // set max width so that search field not overflows with small screens
                        // assumes all other sibling is a button of 'VLCStyle.bannerButton_width' width
                        maxSearchFieldWidth: root.width
                                             - (VLCStyle.bannerButton_width * globalToolbarRightContextGroup.count)
                                             - (globalToolbarRightContextGroup.spacing * (globalToolbarRightContextGroup.count - 1))
                                             - globalToolbarRightContextGroup.anchors.rightMargin
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

                Navigation.parentItem: root
                Navigation.leftItem: globalToolbarLeftContextGroup
                Navigation.rightItem: globalToolbarRight
            }
        }

        Loader {
            id: globalToolbarRight
            anchors {
                right: parent.right
                rightMargin: VLCStyle.applicationHorizontalMargin
            }
            height: VLCStyle.globalToolbar_height
            active: root._showCSD
            source: VLCStyle.palette.hasCSDImage
                      ? "qrc:///qt/qml/VLC/Widgets/CSDThemeButtonSet.qml"
                      : "qrc:///qt/qml/VLC/Widgets/CSDWindowButtonSet.qml"
        }

        Keys.priority: Keys.AfterItem
        Keys.onPressed: (event) => root.Navigation.defaultKeyAction(event)
    }
}
