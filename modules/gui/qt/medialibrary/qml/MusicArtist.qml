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

    //the index to "go to" when the view is loaded
    property int initialIndex: 0

    property Item headerItem: _currentView && MainCtx.gridView ? _currentView.headerItem : null

    property bool isSearchable: true

    property var searchPattern
    property alias sortOrder: albumModel.sortOrder
    property alias sortCriteria: albumModel.sortCriteria

    // current index of album model
    readonly property int currentIndex: {
        if (!_currentView)
           return -1
        else
           return _currentView.currentIndex
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
        property Item albumsListView: albumsLoader.status === Loader.Ready && MainCtx.gridView ? albumsLoader.item.albumsListView: null

        focus: true
        height: col.height
        width: root.width

        function setCurrentItemFocus(reason) {
            if (albumsListView)
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

                // visible: MainCtx.gridView

                leftPadding: root._contentLeftMargin
                bottomPadding: VLCStyle.layoutTitle_bottom_padding -
                               (MainCtx.gridView ? 0 : VLCStyle.gridItemSelectedBorder)

                text: qsTr("Albums")
                visible: MainCtx.gridView
            }

            Loader {
                id: albumsLoader

                active: !MainCtx.gridView
                visible: MainCtx.gridView
                focus: true

                onActiveFocusChanged: {
                    // make sure content is visible with activeFocus
                    if (activeFocus)
                        root.navigationShowHeader(y, height)
                }

                sourceComponent: Column {
                    property alias albumsListView: albumsList

                    width: albumsList.width
                    height: implicitHeight

                    spacing: VLCStyle.tableView_spacing - VLCStyle.margin_xxxsmall

                    Widgets.ListViewExt {
                        id: albumsList

                        x: root._contentLeftMargin - VLCStyle.gridItemSelectedBorder

                        width: root.width - root.rightPadding - root._contentLeftMargin - root._contentRightMargin
                        height: gridHelper.cellHeight + topMargin + bottomMargin + VLCStyle.margin_xxxsmall

                        leftMargin: VLCStyle.gridItemSelectedBorder
                        rightMargin: leftMargin

                        topMargin: VLCStyle.gridItemSelectedBorder
                        bottomMargin: VLCStyle.gridItemSelectedBorder

                        focus: true

                        model: albumModel
                        selectionModel: albumSelectionModel
                        orientation: ListView.Horizontal
                        spacing: VLCStyle.column_spacing
                        buttonMargin: (gridHelper.cellHeight - gridHelper.textHeight - buttonLeft.height) / 2 +
                                      VLCStyle.gridItemSelectedBorder

                        Navigation.parentItem: root

                        Navigation.upAction: function() {
                            artistBanner.setCurrentItemFocus(Qt.TabFocusReason);
                        }

                        Navigation.downAction: function() {
                            root.setCurrentItemFocus(Qt.TabFocusReason);
                        }

                        GridSizeHelper {
                            id: gridHelper

                            availableWidth: albumsList.width
                            basePictureWidth: VLCStyle.gridCover_music_width
                            basePictureHeight: VLCStyle.gridCover_music_height
                        }

                        delegate: Widgets.GridItem {
                            id: gridItem

                            required property var model
                            required property int index

                            y: selectedBorderWidth

                            width: gridHelper.cellWidth
                            height: gridHelper.cellHeight

                            pictureWidth: gridHelper.maxPictureWidth
                            pictureHeight: gridHelper.maxPictureHeight

                            image: model.cover || ""
                            fallbackImage: VLCStyle.noArtAlbumCover

                            title: model.title || qsTr("Unknown title")
                            subtitle: model.release_year || ""
                            subtitleVisible: true
                            textAlignHCenter: true
                            dragItem: albumDragItem

                            // updates to selection is manually handled for optimization purpose
                            Component.onCompleted: _updateSelected()

                            onIndexChanged: _updateSelected()

                            onPlayClicked: play()
                            onItemDoubleClicked: play()

                            onItemClicked: (modifier) => {
                                albumsList.selectionModel.updateSelection( modifier , albumsList.currentIndex, index )
                                albumsList.currentIndex = index
                                albumsList.forceActiveFocus()
                            }

                            Connections {
                                target: albumsList.selectionModel

                                function onSelectionChanged(selected, deselected) {
                                    const idx = albumModel.index(gridItem.index, 0)
                                    const findInSelection = s => s.find(range => range.contains(idx)) !== undefined

                                    // NOTE: we only get diff of the selection
                                    if (findInSelection(selected))
                                        gridItem.selected = true
                                    else if (findInSelection(deselected))
                                        gridItem.selected = false
                                }
                            }

                            onContextMenuButtonClicked: (_, globalMousePos) => {
                                albumSelectionModel.updateSelection( Qt.NoModifier , albumsList.currentIndex, index )
                                contextMenu.popup(albumSelectionModel.selectedIndexes
                                                  , globalMousePos)
                            }

                            function play() {
                                if ( model.id !== undefined ) {
                                    MediaLib.addAndPlay( model.id )
                                }
                            }

                            function _updateSelected() {
                                selected = albumSelectionModel.isRowSelected(gridItem.index)
                            }
                        }

                        onActionAtIndex: (index) => { albumModel.addAndPlay( new Array(index) ) }
                    }

                    Widgets.ViewHeader {
                        view: root

                        leftPadding: root._contentLeftMargin
                        topPadding: 0

                        text: qsTr("Tracks")
                    }
                }
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
        const albumsListView = MainCtx.gridView ? _currentView : _currentView.albumListView
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
            width: parent.width
            height: parent.height

            // Not really relevant in list view?
            property alias currentIndex: listView_id.currentIndex
            property alias albumListView: listView_id

            function setCurrentItemFocus(focus_reason) {
                listView_id.setCurrentItemFocus(focus_reason)
            }

            //TODO: Flashes when switching between list/grid.
            // Move this outside of both the grid and list views to make switching between those views smoother
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

                property var currentVisibleSection: null
                property real sectionHeight: 0

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

                forceShowDefaultHeader: true
                headerAtBottom: true
                manualSortDisbled: true


                section.property: "album_id"
                section.delegate: MusicAlbumSectionDelegate {
                    width: listView_id.width
                    height: implicitHeight

                    onImplicitHeightChanged: {
                        listView_id.sectionHeight = implicitHeight
                    }

                    prevAlbumBtnVisible: true
                    nextAlbumBtnVisible: true

                    onChangeAlbum: (direction) => {
                        listView_id.navigateSection(direction > 0 ? "prev" : "next", parseInt(section))
                    }
                }

                function navigateSection(direction, currentSection) {
                    const sectionProp = listView_id.section.property
                    const model = listView_id.model
                    const count = listView_id.count
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

                    const currentIndex = currentSection == null ? 0 : sectionIndexes.findIndex(s => s.album_id === currentSection)

                    let targetSection = null
                    if (direction === "next" && currentIndex < sectionIndexes.length - 1) {
                        targetSection = sectionIndexes[currentIndex + 1]
                    } else if (direction === "prev" && currentIndex > 0) {
                        targetSection = sectionIndexes[currentIndex - 1]
                    }

                    if (targetSection) {
                        listView_id.positionViewAtIndex(targetSection.index, ListView.SnapPosition)
                        listView_id.currentVisibleSection = targetSection.album_id
                        listView_id.currentIndex = targetSection.index
                        Qt.callLater(() => {
                            const offset = listView_id.sectionHeight || 0
                            listView_id.contentY += offset + VLCStyle.margin_normal
                        })
                    }
                }

                header: MusicAlbumSectionDelegate {
                    width: listView_id.width
                    height: listView_id.currentVisibleSection ? implicitHeight : 0

                    section: listView_id.currentVisibleSection
                    visible: listView_id.currentVisibleSection != null

                    prevAlbumBtnVisible: true
                    nextAlbumBtnVisible: true

                    onChangeAlbum: (direction) => {
                        listView_id.navigateSection(direction > 0 ? "prev" : "next", section)
                    }
                }

                onContentYChanged: {
                    for (let i = 0; i < listView_id.count; ++i) {
                        const item = listView_id.itemAtIndex(i)
                        if (!item) continue

                        const item_y = Math.max(item.y - listView_id.sectionHeight - VLCStyle.margin_small, 0)
                        if (item_y <= listView_id.contentY && (item_y + item.height) > listView_id.contentY) {
                            const current = model.getDataAt(i).album_id
                            if (listView_id.currentVisibleSection !== current)
                                listView_id.currentVisibleSection = current
                            return
                        }
                    }
                    listView_id.currentVisibleSection = null
                }

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
                    headerItem.setCurrentItemFocus(Qt.TabFocusReason);
                }

                Navigation.cancelAction: root._onNavigationCancel

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
