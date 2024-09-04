/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
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
import QtQuick.Controls as T
import QtQml.Models

import VLC.Style
import VLC.Widgets as Widgets

T.Pane {
    id: root

    topPadding: VLCStyle.margin_normal
    bottomPadding: VLCStyle.margin_normal

    implicitWidth: VLCStyle.expandNavigationPaneWidth
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    signal itemClicked(sectionUri : string, modelUri: string)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    enum ExpansionState {
        Expanded,
        Collapsed,
        Unexpandable
    }

    //TODO: update background properties accordingly
    background: Widgets.AcrylicBackground {
        tintColor: theme.bg.primary
    }

    // TODO: replace this with a C++ model
    readonly property var sidebarModel: [
        {
            name: qsTr("Home"),
            icon: VLCIcons.home,
            uri: "home",
            sectionUri: "undefined",
            isExpanded: NavigationPane.ExpansionState.Unexpandable,
            group: "root"
        },
        {
            name: qsTr("Browse"),
            icon: VLCIcons.topbar_network,
            uri: "network",
            sectionUri: "undefined",
            isExpanded: NavigationPane.ExpansionState.Unexpandable,
            group: "root"
        },
        {
            name: qsTr("Music"),
            icon: VLCIcons.topbar_music,
            uri: "music",
            sectionUri: "undefined",
            isExpanded: NavigationPane.ExpansionState.Collapsed,
            group: "root"
        },
        {
            name: qsTr("Artists"),
            uri: "artists",
            sectionUri: "music",
            isExpanded: NavigationPane.ExpansionState.Unexpandable,
            group: "Music"
        },
        {
            name: qsTr("Albums"),
            uri: "albums",
            sectionUri: "music",
            isExpanded: NavigationPane.ExpansionState.Unexpandable,
            group: "Music"
        },
        {
            name: qsTr("Tracks"),
            uri: "tracks",
            sectionUri: "music",
            isExpanded: NavigationPane.ExpansionState.Unexpandable,
            group: "Music"
        },
        {
            name: qsTr("Genres"),
            uri: "genres",
            sectionUri: "music",
            isExpanded: NavigationPane.ExpansionState.Unexpandable,
            group: "Music"
        },
        {
            name: qsTr("Playlists"),
            uri: "playlists",
            sectionUri: "music",
            isExpanded: NavigationPane.ExpansionState.Unexpandable,
            group: "Music"
        },
        {
            name: qsTr("Videos"),
            icon: VLCIcons.topbar_video,
            uri: "video",
            sectionUri: "undefined",
            isExpanded: NavigationPane.ExpansionState.Collapsed,
            group: "root"
        },
        {
            name: qsTr("All"),
            uri: "all",
            sectionUri: "video",
            isExpanded: NavigationPane.ExpansionState.Unexpandable,
            group: "Videos"
        },
        {
            name: qsTr("Playlists"),
            uri: "playlists",
            sectionUri: "video",
            isExpanded: NavigationPane.ExpansionState.Unexpandable,
            group: "Videos"
        },
        {
            name: qsTr("Discover"),
            icon: VLCIcons.topbar_discover,
            uri: "discover",
            sectionUri: "undefined",
            isExpanded: NavigationPane.ExpansionState.Collapsed,
            group: "root"
        },
        {
            name: qsTr("Services"),
            uri: "services",
            sectionUri: "discover",
            isExpanded: NavigationPane.ExpansionState.Unexpandable,
            group: "Discover"
        },
        {
            name: qsTr("URL"),
            uri: "url",
            sectionUri: "discover",
            isExpanded: NavigationPane.ExpansionState.Unexpandable,
            group: "Discover"
        }
    ]

    ListModel {
        id: displayModel

        function initializeModel() {
            sidebarModel.forEach((obj) => {
                if (obj.group === "root") {
                    displayModel.append({
                        name: obj.name,
                        group: obj.group,
                        uri: obj.uri,
                        sectionUri: obj.sectionUri,
                        icon: obj.icon,
                        isExpanded: obj.isExpanded
                    })
                }
            })
        }

        function expandSection(name, parentIndex) {
            // displayModel.setProperty(parentIndex, "isExpanded", "true")
            displayModel.setProperty(parentIndex, "isExpanded", NavigationPane.ExpansionState.Expanded)

            // Retract previously expanded elements
            let removalName = ""
            let removalIndex = -1
            let removalCount = 0
            let initialSize = displayModel.count
            for (let index = 0; index < initialSize; index++) {
                let obj = displayModel.get(index)
                // if (obj.name !== name && obj.isExpanded === "true") {
                if (obj.name !== name && obj.isExpanded === NavigationPane.ExpansionState.Expanded) {
                    // displayModel.setProperty(index, "isExpanded", "false")
                    displayModel.setProperty(index, "isExpanded", NavigationPane.ExpansionState.Collapsed)
                    removalName = obj.name
                    removalIndex = index+1
                }
                else if (obj.group === removalName) {
                    removalCount++
                }
            }
            if (removalCount > 0 && removalIndex !== -1)
                displayModel.remove(removalIndex, removalCount)

            // Expand new elements
            let insertionIndex = -1
            let size = displayModel.count
            for (let index = 0; index < size; index++) {
                let obj = displayModel.get(index)
                if (obj.name === name) {
                    insertionIndex = index+1
                    break
                }
            }

            size = sidebarModel.length
            for(let index = 0; index < size; index++) {
                if (sidebarModel[index].group === name) {
                    displayModel.insert(insertionIndex, sidebarModel[index])
                    insertionIndex++
                }
            }
        }

        function retractSection(name, parentIndex) {
            // displayModel.setProperty(parentIndex, "isExpanded", "false")
            displayModel.setProperty(parentIndex, "isExpanded", NavigationPane.ExpansionState.Collapsed)
            let size = displayModel.count
            let count = 0
            for (let index = parentIndex; index < size; index++) {
                let obj = displayModel.get(index)
                if (obj.group === name) {
                    count++
                }
            }
            displayModel.remove(parentIndex+1, count)
        }

        function autoUnexpandSection(name, parentIndex) {
            let size = displayModel.count
            for (let index = parentIndex; index < size; index++) {
                let obj = displayModel.get(index)
                let count = 0
                // if (obj.isExpanded === "true" && obj.group === "root") {
                if (obj.isExpanded === NavigationPane.ExpansionState.Expanded && obj.group === "root") {
                    for (let subIndex = index+1; subIndex < size; subIndex++) {
                        let subObj = displayModel.get(subIndex)
                        if (subObj.group !== "root")
                            count++
                        else
                            break;
                    }
                    // displayModel.setProperty(index, "isExpanded", "false")
                    displayModel.setProperty(index, "isExpanded", NavigationPane.ExpansionState.Collapsed)
                    displayModel.remove(index+1, count)
                    break;
                }
            }
        }

        function toggleSection(name) {
            let size = displayModel.count
            for (let index = 0; index < size; index++) {
                let obj = displayModel.get(index)
                if (obj.name === name && obj.isExpanded === NavigationPane.ExpansionState.Collapsed) {
                    displayModel.expandSection(name, index)
                    return
                }
                else if (obj.name === name && obj.isExpanded === NavigationPane.ExpansionState.Expanded) {
                    displayModel.retractSection(name, index)
                    return
                }
                else if (obj.name === name && obj.isExpanded === NavigationPane.ExpansionState.Unexpandable) {
                    displayModel.autoUnexpandSection(name, index)
                    return
                }
            }
        }

        Component.onCompleted: {
            displayModel.initializeModel()
        }
    }

    contentItem: ColumnLayout {
        spacing: 2
        RowLayout {
            id: vlcBanner
            Layout.fillWidth: true

            Widgets.BannerCone {
                id: logo
                Layout.leftMargin: VLCStyle.margin_normal
                sourceSize.width: VLCStyle.icon_banner
                sourceSize.height: VLCStyle.icon_banner
                color: theme.accent
            }

            Widgets.SubtitleLabel {
                text: qsTr("VLC")
                font.pixelSize: VLCStyle.vlcHeaderNavigationPane
                color: theme.fg.primary
            }
        }

        Widgets.ListViewExt {
            id: listView

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: VLCStyle.expandNavigationPaneWidth
            Layout.leftMargin: VLCStyle.leftMarginNavigationPane

            clip: true

            model: displayModel

            spacing: VLCStyle.margin_small

            delegate: Widgets.BannerTabButton {
                centerContent: false
                topPadding: model.group != "root" ? VLCStyle.margin_xxsmall : 0
                bottomPadding: model.group != "root" ? VLCStyle.margin_xxsmall : 0

                iconTxt: model.group === "root" ? model.icon : ""
                text: model.name

                onClicked: {
                    itemClicked(model.sectionUri, model.uri)
                    displayModel.toggleSection(model.name)
                }

            }
        }
    }

    // Loader {}
    //
    // states: []
    //
    // transitions: []
}
