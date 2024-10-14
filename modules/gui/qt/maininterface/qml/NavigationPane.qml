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
    leftPadding: 0

    implicitWidth: VLCStyle.expandNavigationPaneWidth
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    signal itemClicked(sectionUri : string, modelUri: string)

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    enum SidebarButtonState {
        Expanded = 1,
        Collapsed = 2,
        Unexpandable = 4,
        Inactive = 8,
        Active = 16
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
            state: NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive,
            group: "root"
        },
        {
            name: qsTr("Videos"),
            icon: VLCIcons.topbar_video,
            uri: "video",
            sectionUri: "undefined",
            state: NavigationPane.SidebarButtonState.Collapsed | NavigationPane.SidebarButtonState.Inactive,
            group: "root"
        },
        {
            name: qsTr("All"),
            uri: "all",
            sectionUri: "video",
            state: NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive,
            group: "Videos"
        },
        {
            name: qsTr("Playlists"),
            uri: "playlists",
            sectionUri: "video",
            state: NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive,
            group: "Videos"
        },
        {
            name: qsTr("Music"),
            icon: VLCIcons.topbar_music,
            uri: "music",
            sectionUri: "undefined",
            state: NavigationPane.SidebarButtonState.Collapsed | NavigationPane.SidebarButtonState.Inactive,
            group: "root"
        },
        {
            name: qsTr("Artists"),
            uri: "artists",
            sectionUri: "music",
            state: NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive,
            group: "Music"
        },
        {
            name: qsTr("Albums"),
            uri: "albums",
            sectionUri: "music",
            state: NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive,
            group: "Music"
        },
        {
            name: qsTr("Tracks"),
            uri: "tracks",
            sectionUri: "music",
            state: NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive,
            group: "Music"
        },
        {
            name: qsTr("Genres"),
            uri: "genres",
            sectionUri: "music",
            state: NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive,
            group: "Music"
        },
        {
            name: qsTr("Playlists"),
            uri: "playlists",
            sectionUri: "music",
            state: NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive,
            group: "Music"
        },
        {
            name: qsTr("Browse"),
            icon: VLCIcons.topbar_network,
            uri: "network",
            sectionUri: "undefined",
            state: NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive,
            group: "root"
        },
        {
            name: qsTr("Discover"),
            icon: VLCIcons.topbar_discover,
            uri: "discover",
            sectionUri: "undefined",
            state: NavigationPane.SidebarButtonState.Collapsed | NavigationPane.SidebarButtonState.Inactive,
            group: "root"
        },
        {
            name: qsTr("Services"),
            uri: "services",
            sectionUri: "discover",
            state: NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive,
            group: "Discover"
        },
        {
            name: qsTr("URL"),
            uri: "url",
            sectionUri: "discover",
            state: NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive,
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
                        state: obj.state
                    })
                }
            })
        }

        function expandSection(name, parentIndex) {
            displayModel.setProperty(parentIndex, "state", NavigationPane.SidebarButtonState.Expanded | NavigationPane.SidebarButtonState.Active)

            // Retract and deselect previously expanded and unexpandable elements
            let removalName = ""
            let removalIndex = -1
            let removalCount = 0
            let initialSize = displayModel.count
            for (let index = 0; index < initialSize; index++) {
                let obj = displayModel.get(index)
                if (obj.name !== name && obj.state & NavigationPane.SidebarButtonState.Expanded) {
                    displayModel.setProperty(index, "state", NavigationPane.SidebarButtonState.Collapsed | NavigationPane.SidebarButtonState.Inactive)
                    removalName = obj.name
                    removalIndex = index+1
                }
                else if (obj.group === removalName) {
                    displayModel.setProperty(index, "state", NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive)
                    removalCount++
                }

                if (obj.name !== name && obj.state & NavigationPane.SidebarButtonState.Active) {
                    displayModel.setProperty(index, "state", NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive)
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
            displayModel.setProperty(parentIndex, "state", NavigationPane.SidebarButtonState.Collapsed | NavigationPane.SidebarButtonState.Inactive)
            let size = displayModel.count
            let count = 0
            for (let index = parentIndex+1; index < size; index++) {
                let obj = displayModel.get(index)
                if (obj.group === name) {
                    count++
                }
                else {
                    break
                }
            }
            displayModel.remove(parentIndex+1, count)
        }

        function autoUnexpandSection(name, parentIndex) {
            let size = displayModel.count
            for (let index = 0; index < size; index++) {
                let obj = displayModel.get(index)
                let count = 0

                // Collapses expandable buttons
                if (obj.name !== name && obj.state & NavigationPane.SidebarButtonState.Expanded) {
                    for (let subIndex = index+1; subIndex < size; subIndex++) {
                        let subObj = displayModel.get(subIndex)
                        if (subObj.group === obj.name) {
                            if (subObj.state & NavigationPane.SidebarButtonState.Active) {
                                displayModel.setProperty(subIndex, "state", NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive)
                            }
                            count++
                        }
                        else
                            break;
                    }
                    
                    displayModel.setProperty(index, "state", NavigationPane.SidebarButtonState.Collapsed | NavigationPane.SidebarButtonState.Inactive)
                    displayModel.remove(index+1, count)
                    break;
                }

                // Deselects unexpandable buttons
                if (obj.name !== name && obj.state & NavigationPane.SidebarButtonState.Active && obj.state & NavigationPane.SidebarButtonState.Unexpandable) {
                    displayModel.setProperty(index, "state", NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive)
                    break;
                }
            }

            // Sets the selected button as active
            size = displayModel.count
            for (let index = 0; index < size; index++) {
                let obj = displayModel.get(index)
                if (obj.name === name) {
                    displayModel.setProperty(index, "state", NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Active)
                    break;
                }
            }
        }

        function toggleSection(name) {
            let size = displayModel.count
            for (let index = 0; index < size; index++) {
                let obj = displayModel.get(index)

                if (obj.name === name && obj.state & NavigationPane.SidebarButtonState.Collapsed && obj.group === "root") {
                    displayModel.expandSection(name, index)
                    return
                }
                else if (obj.name === name && obj.state & NavigationPane.SidebarButtonState.Expanded && obj.group === "root") {
                    displayModel.retractSection(name, index)
                    return
                }
                else if (obj.name === name && obj.state & NavigationPane.SidebarButtonState.Unexpandable && obj.group === "root") {
                    displayModel.autoUnexpandSection(name, index)
                    return
                }

                //handles child buttons of collapsible sections
                else if (obj.name === name && obj.state & NavigationPane.SidebarButtonState.Unexpandable && obj.group !== "root") {
                    for (let subIndex = 0; subIndex < size; subIndex++) {
                        let obj = displayModel.get(subIndex)
                        if (obj.name !== name && obj.state & NavigationPane.SidebarButtonState.Active && obj.group !== "root") {
                            displayModel.setProperty(subIndex, "state", NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Inactive)
                            break;
                        }
                    }
                    displayModel.setProperty(index, "state", NavigationPane.SidebarButtonState.Unexpandable | NavigationPane.SidebarButtonState.Active)
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
            Layout.bottomMargin: VLCStyle.margin_normal

            Widgets.BannerCone {
                id: logo
                Layout.leftMargin: VLCStyle.vlcIconLeftMarginNavigationPane

                sourceSize.width: VLCStyle.vlcHeaderIconNavigationPane
                sourceSize.height: VLCStyle.vlcHeaderIconNavigationPane
                color: theme.accent
            }

            Widgets.SubtitleLabel {
                topPadding: VLCStyle.margin_small 
                text: qsTr("VLC")
                font.pixelSize: VLCStyle.vlcHeaderTextNavigationPane
                color: theme.fg.primary
            }
        }

        Widgets.ListViewExt {
            id: listView

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: VLCStyle.expandNavigationPaneWidth

            clip: true

            model: displayModel

            delegate: Widgets.BannerTabButton {
                centerContent: false
                width: ListView.view.contentWidth
                height: VLCStyle.buttonHeightNavigationPane

                iconTxt: model.group === "root" ? model.icon : ""
                text: model.name
                leftPadding: model.group === "root" ? VLCStyle.leftPaddingRootNavigationPane : VLCStyle.leftPaddingChildNavigationPane 
                selected: model.state & NavigationPane.SidebarButtonState.Active
                underlineIndicator: false

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
