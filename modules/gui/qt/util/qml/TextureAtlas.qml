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

import VLC.MainInterface
import VLC.Util

// `TextureAtlas` is useful to generate a custom texture atlas.
// `subTextureProviders`, which is a list of texture providers
// can be used to get texture providers that represent sub-
// textures in the atlas. A texture atlas is useful in view
// situations where delegate instance scene graph nodes should
// be batch rendered.
Item {
    id: root

    // Without this property, releasing unused graphics resources
    // would not work as intended, because once `ShaderEffectSource`
    // `sourceItem` changes, the layer will no longer be available
    // even if `live` is set to false. Note that it is required to
    // change `sourceItem` to decrease the reference counter Qt has
    // so that the resources can be released. You should set this
    // to false if you plug in an effect, because  in that case
    // solely the final layer should do that instead.
    property bool breakAtlasLayerReference: breakLayerReference
    property bool breakLayerReference: releaseUnusedGraphicsResourcesAutomatically

    // Note that the graphics backend may not support too big texture sizes.
    // Maximum atlas size in item's coordinates (actual size would be times dpr)
    property size maximumAtlasSize: Qt.size(1024, 1024)
    readonly property size effectiveAtlasSize: observer.nativeTextureSize

    property alias atlasTextureReady: observer.isValid

    // Texture provider for the whole atlas. Note that downcasting
    // to `ShaderEffectSource` is not evil but not recommended:
    readonly property Item textureAtlasProvider: atlasLayer

    // Texture providers for sub-textures:
    // TODO: `list<Item>` gives syntax error with Qt 6.2.
    readonly property var /* list<Item> */ subTextureProviders: {
        let list = []

        if (!available)
            return list

        if (!provideSubTextureProviders)
            return list

        // It is intentional that both `children` and `childAt()` are used.
        for (let i in subTextureProviderParent.children) {
            const item = subTextureProviderRepeater.itemAt(i)
            if (item && !(item instanceof Repeater)) {
                list.push(item)
            }
        }

        return list
    }

    // You may want to override this if you are handling automatic resource
    // cleanup where there is another layer that depends on the atlas layer,
    // particularly a use case with effect cases:
    property var funcOnAtlasTextureReady: function() {
        if (root.releaseUnusedGraphicsResourcesAutomatically) {
            Qt.callLater(root.releaseDelegateResources)
        }
    }

    property bool provideSubTextureProviders: true

    // This is useful to override, if for example you want to plug in an effect.
    property Item targetTextureProviderForSubTextureProviders: textureAtlasProvider

    // This is false by default because of Qt bug. Instead, call
    // `releaseUnusedResources()` manually.
    property bool releaseUnusedGraphicsResourcesAutomatically: false

    readonly property bool available: (GraphicsInfo.shaderType === GraphicsInfo.RhiShader)

    // If `releaseUnusedGraphicsResourcesAutomatically` is set, live will be turned off
    // when the resources are released. Continuous `live` does not make much sense for
    // atlas textures, but leaving it `true` is not a problem if the source does not
    // change constantly. Even if the source is animated (such as delegate changing its
    // visual), it is still possible to have live texture atlas, but then obviously
    // you can not release the unused resources anymore. If you use effect on top of
    // that, not releasing the unused resources can consume a lot of video memory, so
    // it is not recommended to have such scenario. Also note that although `scheduleUpdate()`
    // was implemented in the draft version, it turned out to be not so good for
    // maintenance, so it is removed. You can simply turn on `live` instead, and
    // either turn it off manually or let `releaseUnusedGraphicsResourcesAutomatically`
    // do that instead:
    live: true
    property alias live: atlasLayer.live

    property real spacing: 0.0

    // Model can be a generic QML model, or a string. If it is a string, it will be treated
    // as an url to an image file and an `Image` will be used to load the image. In this
    // case the `delegate` here will not be respected. It is currently a to-do for now to
    // recognize image metadata and provide sub-texture providers, this is intended to be
    // useful for thumbnail atlas.
    property var model
    property Component delegate

    property Item _atlasProvider
    property Item _flow
    property Item _repeater

    // Why do we have two separate release functions? It is because if another effect is
    // used on top of the atlas texture, we can release the atlas texture in addition to
    // the textures the delegate instances allocated. So, if an effect is plugged in, it
    // is recommended to use `releaseResources()` for greater saving.

    // Set `live` or call `acquireResources()` to re-initialize.
    // Note that you should use `breakAtlasLayerReference` to use this.
    function releaseResources() {
        root.live = false

        releaseAtlasLayer()
        releaseDelegateResources()
    }

    // Set `live` or call `acquireResources()` to re-initialize.
    // Note that you should use `breakAtlasLayerReference` to use this.
    function releaseDelegateResources() {
        root.live = false

        // https://doc.qt.io/qt-6/qquickitem.html#graphics-resource-handling
        if (root._flow)
            console.debug(root, ": releasing the delegate instances and textures, expect the video and system memory consumption to drop.")
        else
            console.debug(root, ": releasing the loaded image for atlas, expect the video memory to drop.") // also releases the `Image` instance, but that's negligible

        loader.active = false
    }

    // Note that if you hold a reference to the layer (such as with `sourceItem`),
    // the layer is going to be released only when all the references are cleared.
    function releaseAtlasLayer() {
        root.live = false

        // https://doc.qt.io/qt-6/qquickitem.html#graphics-resource-handling
        console.debug(root, ": releasing the atlas texture, expect the video memory consumption to drop.")

        atlasLayer.parent = null
    }

    function acquireAtlasResources() {
        loader.active = true
        atlasLayer.parent = root
    }

    // These functions should be overridden in derivatives to accomadate the
    // additional unused resources, such as when you plug in an effect:
    property var releaseUnusedResources: function() {
        root.releaseDelegateResources()
    }

    property var acquireResources: function() {
        root.acquireAtlasResources()
    }

    onLiveChanged: {
        if (live) {
            acquireAtlasResources()
        }
    }

    Loader {
        id: loader

        active: true

        Binding on active {
            when: !root.available
            value: false
        }

        sourceComponent: Item {
            Loader {
                active: !!root.model
                sourceComponent: (typeof root.model === 'string' || typeof root.model === 'url') ? imageComponent
                                                                                                 : flowComponent
            }

            Component {
                id: imageComponent

                // `Image` is inherently a texture provider, no need for indirect `ShaderEffectSource`:
                Image {
                    source: root.model

                    sourceSize: root.maximumAtlasSize

                    asynchronous: true

                    visible: false

                    Component.onCompleted: {
                        root._atlasProvider = this
                    }

                    Component.onDestruction: {
                        root._atlasProvider = null
                    }
                }
            }

            Component {
                id: flowComponent

                Item {
                    Flow {
                        id: flow

                        width: root.maximumAtlasSize.width
                        height: Math.min(implicitHeight, root.maximumAtlasSize.height)

                        spacing: root.spacing

                        readonly property bool populated: (repeater.count === root.model.rowCount()) && (children.length === (repeater.count + 1)) // + 1 is for Repeater itself

                        visible: false

                        onPopulatedChanged: {
                            if (populated) {
                                flow.forceLayout()

                                if (flow.height < flow.implicitHeight) {
                                    console.warn(root, ": atlas size is not big enough!")
                                }

                                if (root.releaseUnusedGraphicsResourcesAutomatically)
                                    root.live = true
                            } else {
                                if (root.releaseUnusedGraphicsResourcesAutomatically)
                                    root.releaseUnusedResources()
                            }
                        }

                        Component.onCompleted: {
                            console.assert(root._flow === null)
                            root._flow = this
                        }

                        Component.onDestruction: {
                            console.assert(root._flow === this)
                            root._flow = null
                        }

                        Repeater {
                            id: repeater

                            delegate: root.delegate
                            model: root.model

                            Component.onCompleted: {
                                console.assert(root._repeater === null)
                                root._repeater = this
                            }

                            Component.onDestruction: {
                                console.assert(root._repeater === this)
                                root._repeater = null
                            }
                        }
                    }

                    ShaderEffectSource {
                        id: atlasIndirectLayer

                        implicitWidth: flow.width
                        implicitHeight: flow.height

                        // Trim excess width:
                        sourceRect: Qt.rect(0, 0, flow.implicitWidth, implicitHeight)

                        width: (sourceRect.width > 0.0) ? sourceRect.width : implicitWidth
                        height: (sourceRect.height > 0.0) ? sourceRect.height : implicitHeight

                        hideSource: true

                        visible: false

                        sourceItem: root.breakAtlasLayerReference ? flow : null
                        parent: root.breakAtlasLayerReference ? root : null

                        onSourceItemChanged: {
                            // Many items need to have `visible` set for proper layouting and decoration. That being said, setting
                            // `visible` does not mean that the item will actually be visible, provided that `hideSource` is used.
                            if (sourceItem === flow)
                                flow.visible = true
                            else
                                flow.visible = false
                        }

                        Component.onCompleted: {
                            root._atlasProvider = this
                        }

                        Component.onDestruction: {
                            root._atlasProvider = null
                        }
                    }
                }
            }
        }
    }

    Item {
        id: subTextureProviderParent

        property real _eDPR: MainCtx.effectiveDevicePixelRatio(Window.window) || 1.0

        Connections {
            target: MainCtx

            function onIntfDevicePixelRatioChanged() {
                subTextureProviderParent._eDPR = MainCtx.effectiveDevicePixelRatio(root.Window.window) || 1.0
            }
        }

        Repeater {
            id: subTextureProviderRepeater

            // TODO: Support pre-generated atlas (loading the atlas image directly from file and mapping sub-textures).

            Binding on model {
                when: root._flow && root._repeater
                value: ((root.provideSubTextureProviders && root._flow?.populated) ? root._repeater?.count : undefined)
                restoreMode: Binding.RestoreNone
            }

            delegate: TextureProviderIndirection {
                id: delegate

                required property int index

                source: root.targetTextureProviderForSubTextureProviders

                property Item targetItem: root._flow?.children, (root._repeater?.itemAt(index) ?? null)

                Binding on textureSubRect {
                    when: delegate.targetItem
                    restoreMode: Binding.RestoreNone
                    value: Qt.rect(delegate.targetItem?.x * subTextureProviderParent._eDPR,
                                   delegate.targetItem?.y * subTextureProviderParent._eDPR,
                                   delegate.targetItem?.width * subTextureProviderParent._eDPR,
                                   delegate.targetItem?.height * subTextureProviderParent._eDPR)
                }

                property int status: Image.Error

                Binding on status {
                    when: delegate.targetItem
                    restoreMode: Binding.RestoreNone
                    value: {
                        if (delegate.targetItem?.status !== undefined) {
                            if (delegate.targetItem.status === Image.Ready)
                                return observer.isValid ? Image.Ready : Image.Loading
                            else
                                return delegate.targetItem.status
                        } else {
                            return observer.isValid ? Image.Ready : Image.Loading
                        }
                    }
                }

                Binding on implicitWidth {
                    when: delegate.targetItem
                    value: delegate.targetItem?.width
                    restoreMode: Binding.RestoreNone
                }

                Binding on implicitHeight {
                    when: delegate.targetItem
                    value: delegate.targetItem?.height
                    restoreMode: Binding.RestoreNone
                }
            }
        }
    }

    ShaderEffect {
        id: atlasIndirectLayerEffect

        property Item source: root.breakAtlasLayerReference ? root._atlasProvider
                                                            : null

        width: atlasLayer.width
        height: atlasLayer.height

        visible: false
    }

    ShaderEffectSource {
        id: atlasLayer

        hideSource: (sourceItem === root._flow)

        sourceItem: root.breakAtlasLayerReference ? atlasIndirectLayerEffect
                                                  : (root?._flow ?? root?._atlasProvider)

        // Trim excess width:
        sourceRect: (root._flow && sourceItem === root._flow) ? Qt.rect(0, 0, root._flow.implicitWidth, implicitHeight)
                                                              : Qt.rect(0, 0, 0, 0)

        width: (sourceRect.width > 0.0) ? sourceRect.width : implicitWidth
        height: (sourceRect.height > 0.0) ? sourceRect.height : implicitHeight

        onSourceItemChanged: {
            if (!!root._flow) {
                // Many items need to have `visible` set for proper layouting and decoration. That being said, setting
                // `visible` does not mean that the item will actually be visible, provided that `hideSource` is used.
                if (sourceItem === root._flow)
                    root._flow.visible = true
                else
                    root._flow.visible = false
            }
        }

        // Only acts as a texture provider:
        visible: false

        TextureProviderObserver {
            id: observer
            source: atlasLayer

            notifyAllChanges: true

            onComparisonKeyChanged: {
                if (observer.isValid) {
                    if (root.funcOnAtlasTextureReady) {
                        root.funcOnAtlasTextureReady()
                    }
                }
            }
        }

        readonly property int status: observer.isValid ? Image.Ready : Image.Loading

        Binding on implicitWidth {
            when: root._atlasProvider
            value: root._atlasProvider?.width
            restoreMode: Binding.RestoreNone
        }

        Binding on implicitHeight {
            when: root._atlasProvider
            value: root._atlasProvider?.height
            restoreMode: Binding.RestoreNone
        }
    }
}
