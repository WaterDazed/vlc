/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
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
import QtQuick.Controls
import QtQuick
import QtQml.Models
import QtQuick.Layouts

import VLC.MediaLibrary

import VLC.Util
import VLC.Widgets as Widgets
import VLC.MainInterface
import VLC.Style

FocusScope {
    id: root

    required property var artistId

    readonly property int _extraMargin: VLCStyle.dynamicAppMargins(width)
    readonly property int _contentLeftMargin: VLCStyle.layout_left_margin + _extraMargin
    readonly property int _contentRightMargin: VLCStyle.layout_right_margin + _extraMargin

    property int displayMarginBeginning: 0
    property int displayMarginEnd: 0

    // Currently only respected by the list view:
    property bool enableBeginningFade: true
    property bool enableEndFade: true

    //the index to "go to" when the view is loaded
    property int initialIndex: 0

    property Item headerItem: _currentView ? _currentView.headerItem : null

    property bool isSearchable: true

    property alias searchPattern: albumModel.searchPattern
    property alias sortOrder: albumModel.sortOrder
    property alias sortCriteria: albumModel.sortCriteria

    // current index of album model
    readonly property int currentIndex: {
        if (!_currentView)
           return -1
        else if (MainCtx.gridView)
           return _currentView.currentIndex
        else
            return headerItem.albumsListView.currentIndex
    }

    property real rightPadding

    property alias _currentView: loader.item

    property var _artist: ({})

    function navigationShowHeader(y, height) {
        const newContentY = Helpers.flickablePositionContaining(_currentView, y, height, 0, 0)

        if (newContentY !== _currentView.contentY)
            _currentView.contentY = newContentY
    }

    property Component header: FocusScope {
        id: headerFs

        property Item albumsListView: loader.status === Loader.Ready ? loader.item.albumsListView: null

        focus: true
        height: col.height
        width: root.width

        function setCurrentItemFocus(reason) {
            if (MainCtx.gridView && albumsListView)
                albumsListView.setCurrentItemFocus(reason);
            else
                artistBanner.setCurrentItemFocus(reason);
        }

        Column {
            id: col

            height: implicitHeight
            width: headerFs.width

            ArtistTopBanner {
                id: artistBanner

                focus: true
                width: headerFs.width

                rightPadding: root.rightPadding

                artist: root._artist

                onActiveFocusChanged: {
                    // make sure content is visible with activeFocus
                    if (activeFocus)
                        root.navigationShowHeader(0, height)
                }

                Navigation.parentItem: root
                Navigation.downAction: function() {
                    if (albumsListView)
                        albumsListView.setCurrentItemFocus(Qt.TabFocusReason);
                    else
                        _currentView.setCurrentItemFocus(Qt.TabFocusReason);
                }
            }

            Widgets.ViewHeader {
                view: root

                leftPadding: root._contentLeftMargin
                bottomPadding: VLCStyle.layoutTitle_bottom_padding -
                               (MainCtx.gridView ? 0 : VLCStyle.gridItemSelectedBorder)

                text: qsTr("Albums")
                visible: MainCtx.gridView
            }
        }
    }

    focus: true

    onInitialIndexChanged: resetFocus()

    onArtistIdChanged: fetchArtistData()

    function setCurrentItemFocus(reason) {
        if (loader.item === null) {
            Qt.callLater(setCurrentItemFocus, reason)
            return
        }
        loader.item.setCurrentItemFocus(reason);
    }

    function resetFocus() {
        if (albumModel.count === 0) {
            return
        }
        let initialIndex = root.initialIndex
        if (initialIndex >= albumModel.count)
            initialIndex = 0
        albumSelectionModel.select(initialIndex, ItemSelectionModel.ClearAndSelect)
        const albumsListView = MainCtx.gridView ? _currentView : null
        if (albumsListView) {
            albumsListView.currentIndex = initialIndex
            albumsListView.positionViewAtIndex(initialIndex, ItemView.Contain)
        }
    }

    function _actionAtIndex(index, model, selectionModel) {
        if (selectionModel.selectedIndexes.length > 1) {
            model.addAndPlay( selectionModel.selectedIndexes )
        } else {
            model.addAndPlay( new Array(index) )
        }
    }

    function _onNavigationCancel() {
        if (_currentView.currentIndex <= 0) {
            root.Navigation.defaultNavigationCancel()
        } else {
            _currentView.currentIndex = 0;
            _currentView.positionViewAtIndex(0, ItemView.Contain)
        }

        if (tableView_id.currentIndex <= 0)
            root.Navigation.defaultNavigationCancel()
        else
            tableView_id.currentIndex = 0;
    }

    function fetchArtistData() {
        if (!artistId)
            return

        if (artistModel.loading)
            return

        artistModel.getDataById(artistId)
            .then((artistData) => {
                root._artist = artistData
            })
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }


    MLArtistModel {
        id: artistModel
        ml: MediaLib

        onLoadingChanged: {
            if (!loading)
                fetchArtistData()
        }
    }

    MLAlbumModel {
        id: albumModel

        ml: MediaLib
        parentId: artistId
        searchPattern: root.searchPattern

        onCountChanged: {
            if (albumModel.count > 0 && !albumSelectionModel.hasSelection) {
                root.resetFocus()
            }
        }
    }

    ListSelectionModel {
        id: albumSelectionModel
        model: albumModel
    }

    Widgets.MLDragItem {
        id: albumDragItem

        mlModel: albumModel
        indexes: indexesFlat ? albumSelectionModel.selectedIndexesFlat
                             : albumSelectionModel.selectedIndexes
        indexesFlat: !!albumSelectionModel.selectedIndexesFlat
        defaultCover: VLCStyle.noArtAlbumCover
    }

    MLAudioModel {
        id: trackModel

        ml: MediaLib
        parentId: albumModel.parentId

        searchPattern: root.searchPattern
    }

    MLContextMenu {
        id: contextMenu

        model: albumModel
    }

    MLContextMenu {
        id: trackContextMenu

        model: trackModel
    }

    Component {
        id: gridComponent

        Widgets.ExpandGridItemView {
            id: gridView_id

            basePictureWidth: VLCStyle.gridCover_music_width
            basePictureHeight: VLCStyle.gridCover_music_height

            focus: true
            activeFocusOnTab:true
            headerDelegate: root.header
            selectionModel: albumSelectionModel
            model: albumModel

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            Connections {
                target: albumModel
                // selectionModel updates but doesn't trigger any signal, this forces selection update in view
                function onParentIdChanged() {
                    currentIndex = -1
                }
            }

            delegate: AudioGridItem {
                id: audioGridItem

                width: gridView_id.cellWidth
                height: gridView_id.cellHeight

                pictureWidth: gridView_id.maxPictureWidth
                pictureHeight: gridView_id.maxPictureHeight

                opacity: gridView_id.expandIndex !== -1 && gridView_id.expandIndex !== audioGridItem.index ? .7 : 1
                dragItem: albumDragItem

                onItemClicked : (modifier) => {
                    gridView_id.leftClickOnItem(modifier, index)
                }

                onItemDoubleClicked: {
                    gridView_id.switchExpandItem(index)
                }

                onContextMenuButtonClicked: (_, globalMousePos) => {
                    gridView_id.rightClickOnItem(index)
                    contextMenu.popup(albumSelectionModel.selectedIndexes, globalMousePos, { "information" : index})
                }

                Behavior on opacity {
                    NumberAnimation {
                        duration: VLCStyle.duration_short
                    }
                }
            }

            expandDelegate: MusicAlbumsGridExpandDelegate {
                id: expandDelegateId

                x: 0
                width: gridView_id.width
                onRetract: gridView_id.retract()
                Navigation.parentItem: root

                Navigation.cancelAction: function() {
                    gridView_id.setCurrentItemFocus(Qt.TabFocusReason);
                }

                Navigation.upAction: function() {
                    gridView_id.setCurrentItemFocus(Qt.TabFocusReason);
                }

                Navigation.downAction: function() {}
            }

            onActionAtIndex: (index) => {
                if (albumSelectionModel.selectedIndexes.length === 1) {
                    switchExpandItem(index);

                    expandItem.setCurrentItemFocus(Qt.TabFocusReason);
                } else {
                    _actionAtIndex(index, albumModel, albumSelectionModel);
                }
            }

            Navigation.parentItem: root

            Navigation.upAction: function() {
                headerItem.setCurrentItemFocus(Qt.TabFocusReason);
            }

            Navigation.cancelAction: root._onNavigationCancel

            Connections {
                target: contextMenu
                function onShowMediaInformation(index) {
                    gridView_id.switchExpandItem( index )
                }
            }
        }

    }

    Component {
        id: listComponent

        Item {
            property Item headerItem: headerLoader.status === Loader.Ready ? headerLoader.item : null
            property alias currentIndex: listView_id.currentIndex
            property alias albumsListView: listView_id

            function setCurrentItemFocus(focus_reason) {
                listView_id.setCurrentItemFocus(focus_reason)
            }

            Loader {
                id: headerLoader
                sourceComponent: root.header
            }

            MusicTrackListDisplay {
                id: listView_id

                anchors {
                    top: headerLoader.bottom
                    left: parent.left
                    right: parent.right
                    bottom: parent.bottom
                    margins: root._extraMargin
                    rightMargin: root._extraMargin / 2
                }

                function positionViewAtSection(direction, section = null) {
                    const sectionProp = listView_id.section.property
                    const model = listView_id.model
                    const count = listView_id.count
                    const currentSection = section || listView_id.currentSection
                    if (count === 0 || !model || !sectionProp)
                        return

                    const sectionIndexes = []

                    // Collect starting indexes of each section
                    for (let i = 0; i < count; ++i) {
                        const albumId = model.getDataAt(i)[sectionProp]
                        if (sectionIndexes.length === 0 || sectionIndexes[sectionIndexes.length - 1].album_id !== albumId) {
                            sectionIndexes.push({ index: i, album_id: albumId })
                        }
                    }

                    const currentIndex = currentSection ? sectionIndexes.findIndex(s => s.album_id === currentSection) : 0

                    let targetSection = null
                    if (direction > 0 && currentIndex < sectionIndexes.length - 1) {
                        targetSection = sectionIndexes[currentIndex + 1]
                    } else if (direction < 0 && currentIndex > 0) {
                        targetSection = sectionIndexes[currentIndex - 1]
                    }

                    if (targetSection) {
                        listView_id.positionViewAtIndex(targetSection.index, ListView.SnapPosition)
                        // This is for keyboard navigation, when user jumps to a section and wants to browse the tracks.
                        listView_id.currentIndex = targetSection.index
                        // Qt.callLater(() => {
                        //     const newY = listView_id.contentY + VLCStyle.cover_small + VLCStyle.margin_xsmall * 2
                        //     listView_id.contentY = Helpers.clamp(newY, 0, listView_id.contentHeight - listView_id.height)
                        // })
                    }
                }

                displayMarginBeginning: root.displayMarginBeginning
                displayMarginEnd: root.displayMarginEnd

                fadingEdge.enableBeginningFade: root.enableBeginningFade
                fadingEdge.enableEndFade: root.enableEndFade

                readonly property var _titleModel: [{
                    weight: 1,

                    model: {
                        criteria: "title",

                        visible: true,

                        text: VLCStyle.isScreenSmall ? qsTr("Tracks") : qsTr("Title"),

                        showSection: "",

                        subCriterias: ["track_number", "duration"],

                        colDelegate: tableColumns.titleTextDelegate,
                        headerDelegate: tableColumns.titleTextHeaderDelegate
                    }
                }]

                readonly property var _allModel: [
                {
                    size: .2,

                    model: {
                        criteria: "track_number",

                        visible: true,

                        text: qsTr("#"),

                        showSection: "",

                        hCenterText: true
                    }
                },
                    ..._titleModel,
                {
                    size: 1,

                    model: {
                        criteria: "duration",

                        visible: true,

                        text: qsTr("Duration"),

                        showSection: "",

                        colDelegate: tableColumns.timeColDelegate,
                        headerDelegate: tableColumns.timeHeaderDelegate
                    }
                }]

                Widgets.MLTableColumns {
                    id: tableColumns

                    showCriterias: VLCStyle.isScreenSmall
                }

                width: parent.width
                height: parent.height

                headerItemAlwaysVisible: true
                hideSectionTextFromLabel: true
                preventSortingFromHeader: true

                property real eDPR: MainCtx.effectiveDevicePixelRatio(Window.window)

                Connections {
                    target: MainCtx

                    function onIntfDevicePixelRatioChanged() {
                        // Update the DPR:
                        // Normally, this is not done, as we display the images at the size we
                        // want, and we don't want to re-load all images on DPR change. But
                        // in this case we depend on the implicit size, so we should re-load
                        // the image with the updated DPR:
                        listView_id.eDPR = MainCtx.effectiveDevicePixelRatio(Window.window)
                    }
                }

                section.property: "album_id"
                section.delegate: MusicAlbumSectionDelegate {
                    width: listView_id.width
                    height: implicitHeight

                    prevAlbumBtnVisible: true
                    nextAlbumBtnVisible: true

                    eDPR: listView_id.eDPR

                    onRequestAlbumChange: (direction) => {
                        listView_id.positionViewAtSection(direction, section)
                    }
                }

                property bool showHeader: listView_id.headerPositioning === ListView.OverlayHeader
                        && listView_id.contentHeight > listView_id.height

                header: MusicAlbumPinnedSectionDelegate {
                    width: listView_id.width
                    height: listView_id.showHeader ? implicitHeight : 0
                    rightPadding: VLCStyle.margin_normal

                    section: listView_id.currentSection || ""
                    visible: listView_id.showHeader

                    prevAlbumBtnVisible: true
                    nextAlbumBtnVisible: true

                    onRequestAlbumChange: (direction) => {
                        listView_id.positionViewAtSection(direction)
                    }

                    Navigation.parentItem: root

                    Navigation.upAction: function() {
                        headerLoader.item.setCurrentItemFocus(Qt.TabFocusReason);
                    }

                    Navigation.downAction: function() {
                        listView_id.setCurrentItemFocus(Qt.TabFocusReason);
                    }
                }
                headerTopPadding: VLCStyle.margin_small

                sortModel: VLCStyle.isScreenSmall
                           ? _titleModel // use criterias text with small screens
                           : _allModel

                focus: true
                clip: true
                activeFocusOnTab: true

                // Need to make sure that both models are in sync otherwise content menu acts on the wrong tracks
                parentId: albumModel.parentId
                searchPattern: root.searchPattern
                hasStrictSectionProperty: true

                sortCriteria: {
                    switch (root.sortCriteria) {
                        case "title":
                            return "album_title"
                        // TODO: Need to do implement this in the model, might have to change ML backend?
                        // case "release_year":
                        //     return "release_year"
                        default:
                            return null
                    }
                }
                sortOrder: root.sortOrder

                Navigation.parentItem: root

                Navigation.upAction: function() {
                    listView_id.headerItem.setCurrentItemFocus(Qt.TabFocusReason);
                }

                Navigation.cancelAction: function() {
                    listView_id.headerItem.setCurrentItemFocus(Qt.TabFocusReason);
                }

                Keys.priority:  Keys.AfterItem
                Keys.onPressed: (event) =>  root.Navigation.defaultKeyAction(event)
            }
        }
    }

    Loader {
        id: loader

        anchors.fill: parent
        anchors.rightMargin: root.rightPadding

        focus: albumModel.count !== 0
        sourceComponent: MainCtx.gridView ? gridComponent : listComponent
    }
}
