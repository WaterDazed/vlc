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
import QtQuick
import QtQuick.Controls
import QtQuick.Templates as T
import QtQml.Models
import QtQuick.Layouts

import VLC.MediaLibrary

import VLC.Util
import VLC.Widgets as Widgets
import VLC.MainInterface
import VLC.Style

//should we use a ScrollView here?
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
           return albumsListView.currentIndex
    }

    property real rightPadding

    property alias _currentView: loader.item
    property Item albumsListView: albumsLoader.status === Loader.Ready ? albumsLoader.item.albumsListView: null

    property var _artist: ({})

    property real _headerYEndPos: albumsLoader.status === Loader.Ready ? (albumsLoader.y + albumsLoader.height) : (artistBanner.y + artistBanner.height + floatingHeader.height)

    function navigationShowHeader(y, height) {
        const newContentY = Helpers.flickablePositionContaining(mainFlickable, y, height, 0, 0)

        if (newContentY !== mainFlickable.contentY)
            mainFlickable.contentY = newContentY
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
        if (root.albumsListView) {
            root.albumsListView.currentIndex = initialIndex
            root.albumsListView.positionViewAtIndex(initialIndex, ItemView.Contain)
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
            selectionModel: albumSelectionModel
            model: albumModel

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            //bidirectionnal mapping between outter flickable and list flickable position
            property real requestedContentY: Math.max(
                                                 gridView_id.originY,
                                                 (mainFlickable.contentY + floatingHeader.height)  - root._headerYEndPos + gridView_id.originY
                                             )
            property bool updateYFromParent: false
            property bool updateYFromContent: false

            onRequestedContentYChanged: {
                if (updateYFromContent)
                    return
                updateYFromParent = true
                contentY = requestedContentY
                updateYFromParent = false
            }
            onContentYChanged: {
                if (updateYFromParent)
                    return
                updateYFromContent = true
                mainFlickable.contentY = contentY - floatingHeader.height + root._headerYEndPos - gridView_id.originY
                updateYFromContent = false
            }

            //we use the the outter scrollable
            interactive: false
            gridScrollBar.policy: ScrollBar.AlwaysOff

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
            Navigation.upItem: root.albumsListView ? root.albumsListView : floatingHeader
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
        id: tableComponent

        Widgets.TableViewExt {
            id: tableView_id

            model: trackModel

            onActionForSelection: (selection) => {
                model.addAndPlay(selection)
            }

            //bidirectionnal mapping between outter flickable and list flickable position
            property real requestedContentY: Math.max(
                                                 tableView_id.originY,
                                                 (mainFlickable.contentY + floatingHeader.height) - root._headerYEndPos + tableView_id.originY
                                             )
            property bool updateYFromParent: false
            property bool updateYFromContent: false

            onRequestedContentYChanged: {
                if (updateYFromContent)
                    return
                updateYFromParent = true
                contentY = requestedContentY
                updateYFromParent = false
            }
            onContentYChanged: {
                if (updateYFromParent)
                    return
                updateYFromContent = true
                mainFlickable.contentY = contentY + root._headerYEndPos - tableView_id.originY
                updateYFromContent = false
            }

            //we use the the outter scrollable
            interactive: false
            defaultScrollBar: null

            headerPositioning: ListView.OverlayHeader
            rowHeight: VLCStyle.tableCoverRow_height

            displayMarginBeginning: root.displayMarginBeginning
            displayMarginEnd: root.displayMarginEnd

            fadingEdge.enableBeginningFade: root.enableBeginningFade
            fadingEdge.enableEndFade: root.enableEndFade

            property var _modelSmall: [{
                weight: 1,

                model: {
                    criteria: "title",

                    subCriterias: [ "duration", "album_title" ],

                    text: qsTr("Title"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate
                }
            }]

            property var _modelMedium: [{
                weight: 1,

                model: {
                    criteria: "title",

                    text: qsTr("Title"),

                    headerDelegate: tableColumns.titleHeaderDelegate,
                    colDelegate: tableColumns.titleDelegate
                }
            }, {
                weight: 1,

                model: {
                    criteria: "album_title",

                    text: qsTr("Album")
                }
            }, {
                size: 1,

                model: {
                    criteria: "duration",

                    text: qsTr("Duration"),

                    showSection: "",

                    headerDelegate: tableColumns.timeHeaderDelegate,
                    colDelegate: tableColumns.timeColDelegate
                }
            }]

            sortModel: (availableRowWidth < VLCStyle.colWidth(4)) ? _modelSmall
                                                                  : _modelMedium

            dragItem: tableDragItem

            rowContextMenu: trackContextMenu

            Navigation.parentItem: root
            Navigation.upItem: root.albumsListView ? root.albumsListView : floatingHeader
            Navigation.cancelAction: root._onNavigationCancel

            onItemDoubleClicked: MediaLib.addAndPlay(model.id)
            onRightClick: trackContextMenu.popup(tableView_id.selectionModel.selectedIndexes, globalMousePos)

            onDragItemChanged: console.assert(tableView_id.dragItem === tableDragItem)

            selectionModel: trackSelectionModel


            ListSelectionModel {
                id: trackSelectionModel
                model: trackModel
            }

            Widgets.MLDragItem {
                id: tableDragItem

                mlModel: trackModel

                indexes: indexesFlat ? tableView_id.selectionModel.selectedIndexesFlat
                                     : tableView_id.selectionModel.selectedIndexes
                indexesFlat: !!tableView_id.selectionModel.selectedIndexesFlat

                defaultCover: VLCStyle.noArtArtistCover
            }

            Widgets.MLTableColumns {
                id: tableColumns

                showCriterias: (tableView_id.sortModel === tableView_id._modelSmall)
            }
        }
    }


    Flickable {
        id: mainFlickable

        anchors.fill: parent
        anchors.rightMargin: root.rightPadding

        contentHeight: artistBanner.height + floatingHeader.height +  albumsLoader.height + loader.item.contentHeight
        contentWidth: width


        ScrollBar.vertical: Widgets.ScrollBarExt { }

        DefaultFlickableScrollHandler { }

        flickableDirection: Flickable.AutoFlickIfNeeded

        boundsBehavior: Flickable.StopAtBounds

        Component.onCompleted: {
            // Flickable filters child mouse events for flicking (even when
            // the delegate is grabbed). However, this is not a useful
            // feature for non-touch cases, so disable it here and enable
            // it if touch is detected through the hover handler:
            MainCtx.setFiltersChildMouseEvents(this, false)
        }

        HoverHandler {
            acceptedDevices: PointerDevice.TouchScreen

            onHoveredChanged: {
                if (hovered)
                    MainCtx.setFiltersChildMouseEvents(mainFlickable, true)
                else
                    MainCtx.setFiltersChildMouseEvents(mainFlickable, false)
            }
        }

        ArtistTopBanner {
            id: artistBanner

            focus: true
            width: parent.width
            y: 0

            rightPadding: root.rightPadding

            artist: root._artist

            onActiveFocusChanged: {
                // make sure content is visible with activeFocus
                if (activeFocus)
                    root.navigationShowHeader(0, height)
            }

            Navigation.parentItem: root
            Navigation.downItem: floatingHeader
        }

        Widgets.DefaultPageHeader {
            id: floatingHeader
            width: parent.width
            y: Math.max(artistBanner.y + artistBanner.height, mainFlickable.contentY)
            z: 2

            leftPadding: VLCStyle.dynamicAppMargins(width) + VLCStyle.layout_left_margin
            rightPadding: VLCStyle.dynamicAppMargins(width) + VLCStyle.layout_right_margin

            Navigation.upItem: artistBanner
            Navigation.downItem: root.albumsListView ? root.albumsListView : loader.item

            text: qsTr("Albums")
        }

        Loader {
            id: albumsLoader

            // floatingHeader doesn't have a fixed position, anchors to artist banner instead
            y: artistBanner.y + artistBanner.height + floatingHeader.height

            active: !MainCtx.gridView
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

                    displayMarginBeginning: root._contentLeftMargin
                    displayMarginEnd: root._contentRightMargin

                    focus: true

                    model: albumModel
                    selectionModel: albumSelectionModel
                    orientation: ListView.Horizontal
                    spacing: VLCStyle.column_spacing
                    buttonMargin: (gridHelper.cellHeight - gridHelper.textHeight - buttonLeft.height) / 2 +
                                  VLCStyle.gridItemSelectedBorder

                    Navigation.parentItem: root
                    Navigation.upItem: floatingHeader
                    Navigation.downItem: loader.item

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

                        fillMode: Image.PreserveAspectCrop

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

        Loader {
            id: loader

            y: Math.max(root._headerYEndPos, mainFlickable.contentY + floatingHeader.height)

            anchors.leftMargin: parent.leftMargin
            anchors.rightMargin: parent.rightMargin

            //height may rapidly change when scrolling near the junction point
            //using a delayed binding avoid "binding loop" warning
            Binding {
            target:  loader
            delayed: true
            property: "height"
            value: Math.min(
                mainFlickable.height - floatingHeader.height,
                mainFlickable.height - (root._headerYEndPos - mainFlickable.contentY)
                )
            }

            width: mainFlickable.width

            focus: albumModel.count !== 0
            sourceComponent: MainCtx.gridView ? gridComponent : tableComponent
        }
    }
}
