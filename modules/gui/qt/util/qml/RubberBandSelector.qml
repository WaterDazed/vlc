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

import VLC.MainInterface
import VLC.Util
import VLC.Style

QtObject {
    id: root

    required property Flickable flickable
    required property ItemSelectionModel selectionModel

    readonly property list<Item> items: [
        RubberBandRectangle {
            id: rubberBandRectangle

            flickable: root.flickable
            selectionModel: root.selectionModel
        },

        RubberBandMouseArea {
            id: rubberBandMouseArea

            rubberBandRectangle: rubberBandRectangle
        }
    ]

    property alias rectangle: rubberBandRectangle
    property alias mouseArea: rubberBandMouseArea

    component RubberBandRectangle : Rectangle {
        id: root

        parent: _contentItem
        z: 100
        visible: false

        color: VLCStyle.rubberBandColor
        border.width: VLCStyle.rubberBandBorderWidth
        border.color: Qt.darker(color)
        radius: VLCStyle.rubberBandRadius

        required property Flickable flickable

        // Only linear models are supported, but the model does not
        // have to be `ListSelectionModel`.
        property ItemSelectionModel selectionModel: flickable?.selectionModel ?? null
        property int mode: MainCtx.ClearAndSelect
        property alias updateInterval: updateTimer.interval
        updateInterval: 20 // ms, if 0 it will be the same as `Qt.callLater()`

        function updateSelection() {
            if (!root.selectionModel)
                return

            // This is intentionally not implemented here using JS, do not move
            // it here:
            MainCtx.updateSelection(root._contentItem,
                                    root.selectionModel,
                                    root.mode,
                                    Qt.rect(root.x, root.y, root.width, root.height))
        }

        readonly property Item _contentItem: flickable?.contentItem ?? null

        onWidthChanged: {
            if (visible)
                updateTimer.start()
        }

        onHeightChanged: {
            if (visible)
                updateTimer.start()
        }

        onVisibleChanged: {
            if (visible) {
                if (!root.selectionModel) {
                    console.error(root, ": selection model is missing!")
                    return
                }

                // Note that temporal modes need the begin signal:
                MainCtx.updateSelection(root._contentItem,
                                        root.selectionModel,
                                        (root.mode === MainCtx.ClearAndSelect) ? MainCtx.Clear
                                                                               : MainCtx.Begin)

                if (width > 0.0 && height > 0.0)
                    root.updateSelection()
            } else {
                updateTimer.stop()
            }
        }

        Timer {
            id: updateTimer

            repeat: false

            onTriggered: {
                root.updateSelection()
            }
        }

        Instantiator {
            // It is intentional that view.count is not used.
            model: ((root.flickable instanceof GridView) || (root.flickable instanceof ListView)) ? root._contentItem?.children.length
                                                                                                  : undefined

            delegate: Binding {
                required property int index

                readonly property QtObject targetObject: {
                    const targetItem = root._contentItem.children[index]
                    if (!targetItem)
                        return null

                    // If the target is not a delegate instance, this won't
                    // matter anyway:
                    if (root.flickable instanceof GridView)
                        return targetItem.GridView // attached GridView
                    else if (root.flickable instanceof ListView)
                        return targetItem.ListView // attached ListView
                    else
                        return targetItem // last chance, for custom views
                }

                target: targetObject

                property: "delayRemove"

                when: root.visible && (targetObject?.hasOwnProperty("delayRemove") ?? false)

                value: true
            }
        }

        Binding {
            target: root.flickable

            when: root.visible && root.flickable.hasOwnProperty("reuseItems")

            property: "reuseItems"

            value: false
        }
    }

    component RubberBandMouseArea : MouseArea {
        // Using mouse area here is fine since rubber band selection
        // is intended to be used with a mouse. That being said, if
        // wanted, another driver can be used instead of this type,
        // for example one that is implemented with `DragHandler`.
        // Currently it is not planned to implement other drivers.
        id: root

        parent: _flickable.contentItem ?? null

        x: _flickable?.contentX ?? 0.0
        y: _flickable?.contentY ?? 0.0
        height: _flickable?.height ?? 0.0
        width: _flickable?.width ?? 0.0

        required property RubberBandRectangle rubberBandRectangle

        property bool autoScroll: true
        property real autoScrollVelocity: VLCStyle.dp(600, VLCStyle.scale)
        property bool showContextMenu: true // Only relevant with right button

        property bool horizontalLock: (_flickable instanceof ListView && _flickable.orientation === ListView.Vertical)
        property bool verticalLock: (_flickable instanceof ListView && _flickable.orientation === ListView.Horizontal)

        propagateComposedEvents: true // Should not be necessary, but just in case

        preventStealing: false

        acceptedButtons: (Qt.LeftButton | Qt.RightButton)

        readonly property Flickable _flickable: rubberBandRectangle?.flickable ?? null
        property point _beginPos

        readonly property point currentPos: (pressed && _flickable) ? Qt.point(mouseX - _flickable.contentItem.x,
                                                                               mouseY - _flickable.contentItem.y)
                                                                    : Qt.point(0, 0) // No need to calculate otherwise

        Loader {
            active: root.autoScroll

            sourceComponent: ViewDragAutoScrollHandler {
                view: root._flickable

                velocity: root.autoScrollVelocity

                margin: 0

                dragging: root.pressed &&
                          rubberBandRectangle.visible &&
                          (rubberBandRectangle.width > 0.0 && rubberBandRectangle.height > 0.0)

                dragPosProvider: function() {
                    return Qt.point(root.currentPos.x - root._flickable.contentX,
                                    root.currentPos.y - root._flickable.contentY)
                }
            }
        }

        onCurrentPosChanged: {
            if (!pressed)
                return

            if (horizontalLock) {
                rubberBandRectangle.x = _beginPos.x // 0
                rubberBandRectangle.width = width
            } else {
                if (currentPos.x > _beginPos.x) {
                    rubberBandRectangle.x = _beginPos.x
                    rubberBandRectangle.width = currentPos.x - _beginPos.x
                } else {
                    rubberBandRectangle.x = currentPos.x
                    rubberBandRectangle.width = _beginPos.x - currentPos.x
                }
            }

            if (verticalLock) {
                rubberBandRectangle.y = _beginPos.y // 0
                rubberBandRectangle.height = height
            } else {
                if (currentPos.y > _beginPos.y) {
                    rubberBandRectangle.y = _beginPos.y
                    rubberBandRectangle.height = currentPos.y - _beginPos.y
                } else {
                    rubberBandRectangle.y = currentPos.y
                    rubberBandRectangle.height = _beginPos.y - currentPos.y
                }
            }
        }

        onPressed: function(mouse) {
            mouse.accepted = true

            focus = true // Grab the focus

            _beginPos = Qt.point((horizontalLock ? 0 : mouseX) - _flickable.contentItem.x,
                                 (verticalLock ? 0 : mouseY) - _flickable.contentItem.y)

            switch (mouse.modifiers) {
                case Qt.ShiftModifier:
                    rubberBandRectangle.mode = MainCtx.Select
                    break
                case Qt.ControlModifier:
                    rubberBandRectangle.mode = MainCtx.Toggle
                    break
                default:
                    rubberBandRectangle.mode = MainCtx.ClearAndSelect
                    break
            }

            rubberBandRectangle.visible = true
        }

        onReleased: function(mouse) {
            mouse.accepted = true

            rubberBandRectangle.visible = false

            if (root.showContextMenu && (mouse.button === Qt.RightButton)) {
                let contextMenuThroughDelegate = false

                // Try delegate instance first:
                if (root._flickable.itemAt !== undefined) {
                    let pos = Qt.point(mouse.x - _flickable.contentItem.x,
                                       mouse.y - _flickable.contentItem.y)
                    let delegateInstance = root._flickable.itemAt(pos.x, pos.y)
                    if (delegateInstance && (delegateInstance.contextMenuButtonClicked !== undefined)) {
                        // Margins:
                        const marginTarget = delegateInstance?.background ?? delegateInstance?.contentItem

                        if (marginTarget) {
                            pos.x -= _flickable.contentX
                            pos.y -= _flickable.contentY

                            if ((pos.x > marginTarget.x) && (pos.x < (marginTarget.x + marginTarget.width)) &&
                                (pos.y > marginTarget.y) && (pos.y < (marginTarget.y + marginTarget.height))) {
                                contextMenuThroughDelegate = true
                            }
                        } else {
                            contextMenuThroughDelegate = true
                        }

                        if (contextMenuThroughDelegate) {
                            delegateInstance.contextMenuButtonClicked(null, mapToGlobal(mouse.x, mouse.y))
                        }
                    }
                }

                // If not possible, try the view:
                if (!contextMenuThroughDelegate && (root._flickable.showContextMenu !== undefined))
                    root._flickable.showContextMenu(mapToGlobal(mouse.x, mouse.y))
            }
        }
    }
}
