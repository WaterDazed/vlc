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

import VLC.Util

// This derivative creates a new atlas after effect is applied on the virgin atlas.
TextureAtlas {
    id: root

    required property Component effect
    readonly property Item effectItem: _effectItem

    property string samplerName: "source"

    property bool breakEffectLayerReference: breakLayerReference
    // We do that here instead for the final (effect) layer:
    breakAtlasLayerReference: false

    property alias atlasTextureWithEffectReady: observer.isValid

    targetTextureProviderForSubTextureProviders: effectLayer

    releaseUnusedResources: function() {
        root.releaseEffectAndBaseResources()
    }

    acquireResources: function() {
        root.acquireEffectAndBaseResources()
    }

    // Set `live` or call `acquireResources()` to re-initialize.
    // Note that you should use `breakEffectLayerReference` to use this.
    function releaseEffectResources() {
        root.live = false

        // https://doc.qt.io/qt-6/qquickitem.html#graphics-resource-handling
        console.debug(root, ": releasing the effect instance and textures, expect the system and video memory consumption to drop.")

        loader.active = false
    }

    // Set `live` or call `acquireResources()` to re-initialize.
    // Note that you should use `breakEffectLayerReference` to use this.
    function releaseEffectAndBaseResources() {
        root.live = false

        releaseEffectResources() // Effect's intermediate layers (not its layer)
        releaseAtlasLayer() // Atlas layer
        releaseDelegateResources()
    }

    function acquireEffectResources() {
        loader.active = true
    }

    function acquireEffectAndBaseResources() {
        acquireAtlasResources()
        acquireEffectResources()
    }

    funcOnAtlasTextureReady: undefined

    onLiveChanged: {
        if (live) {
            acquireEffectResources()
        }
    }

    property Item _effectItem
    property Item _effectIndirectLayer

    property bool _effectVisible: false

    Loader {
        id: loader

        active: true

        Binding on active {
            when: !root.available
            value: false
        }

        sourceComponent: Item {
            Component.onCompleted: {
                console.assert(root._effectItem === null)
                // parent object is this, so it is destroyed automatically when loader is disabled:
                root._effectItem = root.effect.createObject(this, { [root.samplerName]: Qt.binding(() => { return root.textureAtlasProvider }),
                                                                    'visible': Qt.binding(() => { return root._effectVisible }),
                                                                    'live': Qt.binding(() => { return root.live }) })
            }

            Component.onDestruction: {
                root._effectItem = null // This is probably not necessary
            }

            ShaderEffectSource {
                id: effectIndirectLayer

                width: effectLayer.width
                height: effectLayer.height

                hideSource: true

                visible: false

                sourceItem: root.breakEffectLayerReference ? root.effectItem : null
                parent: root.breakEffectLayerReference ? root : null

                onSourceItemChanged: {
                    if (!!root.effectItem) {
                        if (sourceItem === root.effectItem)
                            root._effectVisible = true
                        else
                            root._effectVisible = false
                    }
                }

                Component.onCompleted: {
                    console.assert(root._effectIndirectLayer === null)
                    root._effectIndirectLayer = this
                }

                Component.onDestruction: {
                    console.assert(root._effectIndirectLayer === this)
                    root._effectIndirectLayer = null
                }
            }
        }
    }

    ShaderEffectSource {
        id: effectLayer

        hideSource: (sourceItem === root.effectItem)

        sourceItem: root.breakEffectLayerReference ? effectIndirectLayerEffect
                                                   : (root.effectItem ?? null)

        readonly property int status: observer.isValid ? Image.Ready : Image.Loading
        implicitWidth: textureAtlasProvider.width
        implicitHeight: textureAtlasProvider.height

        live: root.live

        visible: false

        onSourceItemChanged: {
            if (!!root.effectItem) {
                if (sourceItem === root.effectItem)
                    root._effectVisible = true
                else
                    root._effectVisible = false
            }
        }

        TextureProviderObserver {
            id: observer
            source: effectLayer

            notifyAllChanges: true

            onComparisonKeyChanged: {
               if (observer.isValid) {
                   if (root.releaseUnusedGraphicsResourcesAutomatically) {
                        Qt.callLater(root.releaseUnusedResources)
                   }
               }
            }
        }
    }

    ShaderEffect {
        id: effectIndirectLayerEffect

        property Item source: root.breakEffectLayerReference ? root._effectIndirectLayer
                                                             : null

        width: effectLayer.width
        height: effectLayer.height

        visible: false
    }
}
