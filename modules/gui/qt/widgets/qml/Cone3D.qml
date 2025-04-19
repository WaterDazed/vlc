/*****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
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

import VLC.Style

// This item uses 3D SDF and raymarching to render the cone.
// Since everything is computed in the fragment shader, which runs per pixel,
// try to avoid having a large size/coverage in this item. Modern
// graphics processors should be fine to display it even full screen, though.
ShaderEffect {
    id: effect

    implicitWidth: VLCStyle.dp(48, VLCStyle.scale)
    implicitHeight: VLCStyle.dp(48, VLCStyle.scale)

    property color color: "#FF8800" // accent color by default, alpha is not respected (opacity can be used)

    // TODO: Rotation in degrees instead of time seed:
    property real time: 0.0 // seed

    property alias animating: animator.running

    readonly property size size: Qt.size(width, height) // aspect ratio is preserved

    UniformAnimator on time {
        id: animator

        loops: Animation.Infinite
        from: 0
        // I tried to manually find the proper period. When rotation
        // property is introduced, we could use 360 degrees.
        to: 62.8
        duration: 20000 // I did not see point in adjusting the speed, so it is fixed for now.
    }

    blending: true

    // We are not rotating the QML item in 3D. The render plane is 2D, so
    // we can have back face culling. However Qt is being problematic with
    // culling with offscreen rendering (such as, this item or any of its
    // ancestors have layering):
    // cullMode: ShaderEffect.BackFaceCulling

    fragmentShader: "qrc:///shaders/Cone3D.frag.qsb"
}
