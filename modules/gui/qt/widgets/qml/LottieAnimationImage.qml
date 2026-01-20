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

// If fine tuning is needed, `LottieAnimation` item can be used. This item only
// serves as an accessory to the image provider counterpart, which is useful
// only when constantly running animations are needed.
Image {
    id: image

    source: ("image://lottie/" + lottieSource)

    required property url lottieSource

    // Caching is done by the image provider/lottie module and is turned on by
    // default, since it can take a long time to load lottie animations. Image's
    // own caching can also work, but is not well suited for lottie animations.
    // In most cases it will simply reject caching the animation due to high
    // cost.
    cache: false

    Connections {
        target: image.Window.window
        enabled: image.visible &&
                 (image.status === Image.Ready) &&
                 (image.GraphicsInfo.shaderType === GraphicsInfo.RhiShader)

        function onAfterAnimating() {
            // For performance reasons, the underlying texture remains the same.
            // This is important, because allocating and deallocating textures
            // may consume time and resources. Instead, during animating the
            // texture data is updated. There is no signalling of such change
            // either, since queued signals may backlog. For that reason, the
            // `Image` or the node has no idea when the texture is dirty and
            // the node needs to be updated constantly to reflect the changes:
            image.update()
        }
    }
}
