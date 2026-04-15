/*****************************************************************************
 * Copyright (C) 2026 VLC authors and VideoLAN
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

import VLC.MainInterface
import VLC.Style
import VLC.Util

GridView {
    id: root

    property int leftPadding: 0
    property int rightPadding: 0

    // NOTE: These are not applicable at the moment, they are left here because `ViewHeader` uses it:
    property real contentLeftMargin: leftPadding
    property real contentRightMargin: rightPadding

    pixelAligned: (MainCtx.qtVersion() >= MainCtx.qtVersionCheck(6, 2, 5)) // QTBUG-103996
                  && (Screen.pixelDensity >= VLCStyle.highPixelDensityThreshold) // no need for sub-pixel alignment with high pixel density


    // We also have a custom implementation of following current item, see `followCurrentItem`.
    highlightFollowsCurrentItem: true

    // Custom follow current item implementation, necessary when expansion is active.
    // Note that Qt 6.2 seems to be bugged with this, so following current item is not
    // possible when expansion is active with old Qt versions.
    property bool followCurrentItem: interactive && (contentHeight > cellHeight)

    Binding on followCurrentItem {
        when: root.highlightFollowsCurrentItem ||
              (MainCtx.qtVersion() < MainCtx.qtVersionCheck(6, 8, 3)) // ### I am not sure if 6.8 is correct, with 6.11 it behaves correctly
        value: false // force disable
    }

    Binding on highlightFollowsCurrentItem {
        when: expandLoader.active
        value: false // force disable
    }

    activeFocusOnTab: true

    focus: true

    reuseItems: true

    // Content height is calculated automatically by the view.
    // FIXME: It is not calculated correctly when expansion is active,
    //        see the note in <expansion>.
    contentWidth: width - (leftMargin + rightMargin)

    // key navigation is reimplemented for item selection
    keyNavigationEnabled: false
    keyNavigationWraps: false

    flickableDirection: Flickable.AutoFlickIfNeeded

    highlightMoveDuration: 300 //ms

    boundsBehavior: Flickable.StopAtBounds

    Accessible.role: Accessible.Table

    property ListSelectionModel selectionModel: ListSelectionModel {
        model: root.model
    }

    property bool isAnimating: expandAnimation.running

    property bool _releaseActionButtonPressed

    property int _currentFocusReason: Qt.OtherFocusReason

    // Expansion properties:
    property alias expandDelegate: expandLoader.sourceComponent
    property alias expandItem: expandLoader.item
    property alias expandLoadAsynchronously: expandLoader.asynchronous
    expandLoadAsynchronously: true
    property alias animateExpansion: expandLoaderHeightBehavior.enabled
    animateExpansion: true

    readonly property int expandIndex: expandLoader.index
    readonly property Item expandee: itemAtIndex(expandIndex)

    function retract() : bool {
        return expandLoader.retract()
    }

    function expand(index : int) : bool {
        return switchExpandItem(index)
    }

    //signals emitted when selected items is updated from keyboard
    signal selectAll()
    signal actionAtIndex(int index)

    signal showContextMenu(point globalPos)

    onCurrentItemChanged: {
        if (followCurrentItem) {
            positionViewAtIndexAnimated(currentIndex, GridView.Contain)
        }
    }

    property Component defaultScrollBar: Component {
        ScrollBarExt { }
    }

    ScrollBar.vertical: {
        if (root.defaultScrollBar)
            return root.defaultScrollBar.createObject() // rely on JS/QML engine's garbage collection
        return null
    }

    Behavior on contentY {
        id: contentYBehavior

        enabled: false

        // NOTE: Usage of `SmoothedAnimation` is intentional here.
        SmoothedAnimation {
            duration: VLCStyle.duration_veryLong
            easing.type: Easing.InOutSine
        }
    }

    Component.onCompleted: {
        // Flickable filters child mouse events for flicking (even when
        // the delegate is grabbed). However, this is not a useful
        // feature for non-touch cases, so disable it here and enable
        // it if touch is detected through the hover handler:
        MainCtx.setFiltersChildMouseEvents(root, false)
    }

    HoverHandler {
        acceptedDevices: PointerDevice.TouchScreen

        onHoveredChanged: {
            if (hovered)
                MainCtx.setFiltersChildMouseEvents(root, true)
            else
                MainCtx.setFiltersChildMouseEvents(root, false)
        }
    }

    // FIXME: Qt does not provide the `contentY` with `positionViewAtIndex()` for us
    //        to animate. For that reason, we capture the new `contentY`, adjust
    //        `contentY` to it is old value then enable the animation and set `contentY`
    //        to its new value.
    function positionViewAtIndexAnimated(index : int, mode : int) {
        const oldContentY = root.contentY
        let newContentY = 0

        // We calculate the compensation before `positionViewAtIndex()`
        // call because we should spend as little time as possible:
        const item = itemAtIndex(index)
        if (item)
            newContentY = (_effectiveYForItem(item) - item.y)

        root.positionViewAtIndex(index, mode)
        newContentY += root.contentY

        if (Math.abs(oldContentY - newContentY) >= Number.EPSILON) {
            contentYBehavior.enabled = false
            root.contentY = oldContentY
            contentYBehavior.enabled = true
            root.contentY = newContentY
            contentYBehavior.enabled = false
        }
    }

    function leftClickOnItem(modifier, index) {
        selectionModel.updateSelection(modifier, currentIndex, index)
        if (selectionModel.isSelected(index))
            currentIndex = index
        else if (currentIndex === index) {
            itemAtIndex(currentIndex).focus = false
            currentIndex = -1
        }

        // NOTE: We make sure to clear the keyboard focus.
        forceActiveFocus()
    }

    function rightClickOnItem(index) {
        if (!selectionModel.isSelected(index)) {
            leftClickOnItem(Qt.NoModifier, index)
        }
    }

    function setCurrentItemFocus(reason) {

        // NOTE: Saving the focus reason for later.
        _currentFocusReason = reason;

        if (!model || model.count === 0) {
            // NOTE: By default we want the focus on the flickable.
            forceActiveFocus(reason);
            return;
        }

        if (currentIndex === -1)
            currentIndex = 0

        if (currentIndex < count)
            Helpers.enforceFocus(currentItem, reason);
        else
            forceActiveFocus(reason);

        // NOTE: We make sure the current item is fully visible.
        positionViewAtIndex(currentIndex, ItemView.Contain);
    }

    Keys.onPressed: (event) => {
        let newIndex = -1
        const nbItemPerRow = Math.floor(contentWidth / cellWidth)
        const _count = root.count
        if (KeyHelper.matchRight(event)) {
            if ((currentIndex + 1) % nbItemPerRow !== 0) {//are we not at the end of line
                newIndex = Math.min(_count - 1, currentIndex + 1)
            }
        } else if (KeyHelper.matchLeft(event)) {
            if (currentIndex % nbItemPerRow !== 0) {//are we not at the beginning of line
                newIndex = Math.max(0, currentIndex - 1)
            }
        } else if (KeyHelper.matchDown(event)) {
            const lastIndex = _count - 1
            // we are not on the last line
            if (Math.floor(currentIndex / nbItemPerRow)
                !==
                Math.floor(lastIndex / nbItemPerRow)) {
                newIndex = Math.min(lastIndex, currentIndex + nbItemPerRow)
            }
        } else if (KeyHelper.matchPageDown(event)) {
            newIndex = Math.min(_count - 1, currentIndex + nbItemPerRow * 5)
        } else if (KeyHelper.matchUp(event)) {
            if (Math.floor(currentIndex / nbItemPerRow) !== 0) { //we are not on the first line
                newIndex = Math.max(0, currentIndex - nbItemPerRow)
            }
        } else if (KeyHelper.matchPageUp(event)) {
            newIndex = Math.max(0, currentIndex - nbItemPerRow * 5)
        } else if (event.matches(StandardKey.SelectAll)) {
            event.accepted = true
            selectionModel.selectAll()
        }

        if (KeyHelper.matchOk(event)) {
            // Keep activation handling on release to avoid double interpretation.
            event.accepted = true
            _releaseActionButtonPressed = true
        } else {
            _releaseActionButtonPressed = false
        }

        if (newIndex !== -1 && newIndex !== currentIndex) {
            event.accepted = true;

            const oldIndex = currentIndex;
            currentIndex = newIndex;
            if (selectionModel)
                selectionModel.updateSelection(event.modifiers, oldIndex, newIndex)

            // Not necessary to do with custom follow current item implementation:
            if (root.highlightFollowsCurrentItem && !root.followCurrentItem) {
                if (root.headerItem && (root.headerPositioning !== ListView.InlineHeader)) {
                    if (root.currentItem.y < (root.headerItem.y + root.headerItem.height))
                        positionViewAtIndex(currentIndex, ItemView.Contain);
                }

                if (root.footerItem && (root.footerPositioning !== ListView.InlineFooter)) {
                    if (root.currentItem.y > root.footerItem.y)
                        positionViewAtIndex(currentIndex, ItemView.Contain);
                }
            }

            if (oldIndex < currentIndex)
                Helpers.enforceFocus(currentItem, Qt.TabFocusReason);
            else
                Helpers.enforceFocus(currentItem, Qt.BacktabFocusReason);
        }

        if (!event.accepted) {
            Navigation.defaultKeyAction(event)
        }
    }

    Keys.onReleased: (event) => {
        if (!_releaseActionButtonPressed)
            return

        if ( KeyHelper.matchOk(event) ) {
            event.accepted = true
            actionAtIndex(currentIndex)
        }
        _releaseActionButtonPressed = false
    }

    // Returns y for the item, compensating for Translate:
    function _effectiveYForItem(item : Item) : real {
        if (!expandLoader.active)
            return item.y

        for (const i in item.transform) {
            if (item.transform[i].objectName === "expansionCompensateTranslate")
                return (item.y + item.transform[i].y)
        }
    }

    readonly property ColorContext colorContext: ColorContext {
        id: theme
        colorSet: ColorContext.View
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        grabPermissions: PointerHandler.TakeOverForbidden

        gesturePolicy: TapHandler.ReleaseWithinBounds

        onTapped: (eventPoint, button) => {
            initialAction()

            if (button === Qt.RightButton) {
                root.showContextMenu(parent.mapToGlobal(eventPoint.position.x, eventPoint.position.y))
            }
        }

        Component.onCompleted: {
            canceled.connect(initialAction)
        }

        function initialAction() {
            if (root.currentItem)
                root.currentItem.focus = false // Grab the focus from delegate
            root.forceActiveFocus(Qt.MouseFocusReason) // Re-focus the list

            if (!(point.modifiers & (Qt.ShiftModifier | Qt.ControlModifier))) {
                if (root.selectionModel)
                    root.selectionModel.clearSelection()
            }

            root.retract()
        }
    }

    property Component implicitFlickableScrollHandler: DefaultFlickableScrollHandler { }

    // NOTE: This property can be set to null to prevent using a scroll handler:
    property FlickableScrollHandler scrollHandler: {
        // Make sure the JS engine destroys the scroll handler right after (potential) change:
        Qt.callLater(gc) // `QJSEngine::GarbageCollectionExtension` is installed by default

        if (interactive) {
            // JS ownership:
            return implicitFlickableScrollHandler.createObject(null, { target: root })
        } else {
            return null
        }
    }

    /// <expansion>

    Binding {
        target: root.expandee?.GridView ?? null
        property: "delayRemove"
        when: expandLoader.active
        value: true
    }

    Connections {
        target: root.expandee?.GridView ?? null

        // These should not happen because we set `GridView.delayRemove` on the expandee,
        // but it is still good to have this:

        function onRemove() {
            expandLoader.retract(false)
        }

        function onPooled() {
            expandLoader.retract(false)
        }
    }

    onModelChanged: {
        expandLoader.retract(false)
    }

    Connections {
        target: root.model

        function onRowsInserted() { expandLoader.retract(false) }
        function onRowsRemoved() { expandLoader.retract(false) }
        function onModelReset() { expandLoader.retract(false) }
        function onLayoutChanged() { expandLoader.retract(false) }
    }

    Loader {
        id: expandLoader

        parent: root.contentItem

        anchors.left: parent.left
        anchors.right: parent.right

        active: false
        height: 0

        clip: expandAnimation.running && (height < implicitHeight)

        Navigation.parentItem: root

        Navigation.upAction: function() {
            if (root.expandee) {
                root.expandee.forceActiveFocus(Qt.BacktabFocusReason)
                root.currentIndex = root.expandIndex
                return
            }

            root.Navigation.defaultNavigationUp()
        }

        Navigation.downAction: function() {
            for (let i = root.expandIndex + 1; i < root.count; ++i) {
                const item = root.itemAtIndex(i)
                if (item && (item.y > root.expandee.y)) {
                    item.forceActiveFocus(Qt.TabFocusReason)
                    root.currentIndex = item.index
                    return
                }
            }

            root.Navigation.defaultNavigationDown()
        }

        Behavior on height {
            id: expandLoaderHeightBehavior

            SmoothedAnimation {
                id: expandAnimation

                duration: VLCStyle.duration_long
                easing.type: Easing.InOutSine
            }
        }

        function retract(animate /* : bool */ = true) : bool {
            if (!active)
                return false

            const behaviorEnabled = expandLoaderHeightBehavior.enabled
            if (!animate) {
                expandLoaderHeightBehavior.enabled = false
            }

            height = 0.0
            active = Qt.binding(() => (expandLoader.height > 0.0))

            if (!animate) {
                expandLoaderHeightBehavior.enabled = behaviorEnabled
            }

            return true
        }

        function expand(animate /* : bool */ = true) : bool {
            if (active)
                return false

            if (!sourceComponent)
                return false

            const behaviorEnabled = expandLoaderHeightBehavior.enabled
            if (!animate) {
                expandLoaderHeightBehavior.enabled = false
            }

            console.assert(index >= 0)
            active = true
            height = Qt.binding(() => (expandLoader.implicitHeight))

            if (!animate) {
                expandLoaderHeightBehavior.enabled = expandLoaderHeightBehavior.enabled
            }

            return true
        }

        y: root.expandee ? (root.expandee.y + root.cellHeight) : 0.0

        property int index: -1

        onActiveChanged: {
            _oldHeight = 0

            if (!active) {
                 expandLoader.index = -1

                if (root._pendingExpandIndex >= 0) {
                    root.switchExpandItem(root._pendingExpandIndex)
                    root._pendingExpandIndex = -1
                }
            }
        }

        property real _oldHeight

        onHeightChanged: {
            if ((height + y - root.contentY) > (root.height - root._baseDisplayMarginEnd)) {
                root.contentY += (height - _oldHeight)
            }

            _oldHeight = height
        }

        onLoaded: {
            // Can not use `!== undefined` because the value may be undefined even though it exists
            if (item.hasOwnProperty("model"))
                item.model = Qt.binding(() => root.model.getDataAt(root.expandIndex))
            if (item.index !== undefined)
                item.index = Qt.binding(() => expandLoader.index)
            if (item.view !== undefined)
                item.view = root

            if (item.leftPadding !== undefined)
                item.leftPadding = Qt.binding(function() { return VLCStyle.margin_large + VLCStyle.applicationHorizontalMargin })
            if (item.rightPadding !== undefined)
                item.rightPadding = Qt.binding(function() { return VLCStyle.margin_large + VLCStyle.applicationHorizontalMargin })

            if (item.retract !== undefined)
                item.retract.connect(root, root.retract)

            item.Navigation.parentItem = expandLoader

            item.focus = true // Loader itself is a focus scope, so we need this
        }
    }

    property real _baseContentHeight
    property real _baseBottomMargin
    property real _baseDisplayMarginEnd

    property int _pendingExpandIndex: -1

    function switchExpandItem(index : int) {
        if (!expandLoader.active) {
            expandLoader.index = index

            _baseContentHeight = root.contentHeight
            _baseBottomMargin = root.bottomMargin
            _baseDisplayMarginEnd = root.displayMarginEnd

            expandLoader.expand()
        } else {
            // If the new expandee is in the same row, we don't retract first but switch
            // immediately:
            if (expandLoader.index === index) {
                expandLoader.retract()
            } else if (root.expandee) {
                if (Math.abs(root.expandee.y - root.itemAtIndex(index).y) >= Number.EPSILON) {
                    expandLoader.retract()
                    root._pendingExpandIndex = index
                } else {
                    expandLoader.index = index
                }
            }
        }
    }

    // Qt overwrites the binding, so this is not effective at the moment.
    // As a workaround, we adjust the bottom margin instead.

    // Binding on contentHeight {
    //     when: expandLoader.active
    //     value: root._baseContentHeight + expandLoader.height
    // }

    // FIXME: Get rid of this when we can adjust the content height ourselves:
    Binding on bottomMargin {
        when: expandLoader.active
        value: root._baseBottomMargin + expandLoader.height
    }

    Binding on displayMarginEnd {
        when: expandLoader.active
        value: root._baseDisplayMarginEnd + expandLoader.height
    }

    Binding on displayMarginBeginning {
        when: expandLoader.active
        value: root._baseDisplayMarginEnd + expandLoader.height
    }

    Instantiator {
        // We deliberately do not want to deallocate/allocate
        // the Translate depending on expansion at the expense
        // of consuming slightly more memory, because this can
        // take quite some time with large models.

        // It is intentional that children is used instead of `count`
        model: root.contentItem.children.length

        delegate: Translate {
            objectName: "expansionCompensateTranslate"

            required property int index
            property Item targetItem

            y: {
                if (expandLoader.active && targetItem) {
                    if (targetItem.y > root.expandee.y) {
                        return expandLoader.height
                    }
                }

                return 0.0
            }

            Component.onCompleted: {
                targetItem = root.contentItem.children[index]

                console.assert(targetItem)
                if (targetItem !== expandLoader &&
                    targetItem !== root.headerItem &&
                    targetItem !== root.footerItem) {

                    // Note that `Item::transform` is a list

                    for (const i in targetItem.transform.length) {
                        if (targetItem.transform[i] === this)
                            return
                    }

                    targetItem.transform.push(this)
                }
            }
        }
    }

    /// </expansion>
}
