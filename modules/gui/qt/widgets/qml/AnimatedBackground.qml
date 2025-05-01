/*****************************************************************************
 * Copyright (C) 2021 VLC authors and VideoLAN
 *
 * Authors: Prince Gupta <guptaprince8832@gmail.com>
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

Rectangle {
    id: root

    //---------------------------------------------------------------------------------------------
    // Settings
    //---------------------------------------------------------------------------------------------

    property int animationDuration: VLCStyle.duration_long

    readonly property bool animationRunning: borderAnimation.running || bgAnimation.running

    //---------------------------------------------------------------------------------------------
    // Implementation
    //---------------------------------------------------------------------------------------------

    color: "transparent"

    border.width: VLCStyle.focus_border

    //---------------------------------------------------------------------------------------------
    // Animations
    //---------------------------------------------------------------------------------------------

    Behavior on border.color {
        enabled: root.enabled

        ColorAnimation {
            id: borderAnimation

            duration: root.animationDuration
        }
    }

    Behavior on color {
        enabled: root.enabled
        
        ColorAnimation {
            id: bgAnimation

            duration: root.animationDuration
        }
    }

    ShaderEffect {
        // Temporal dithering during color alpha animation
        id: temporalNoise

        anchors.fill: parent
        anchors.margins: root.border.width

        // Only with dark colors:
        visible: root.animationRunning && (root.color.hslLightness < 0.5)

        // The slower the animation, the strengthier the dithering.
        // Manually calibrated for the default duration (200 ms).
        // This is probably not supposed to be linear (for example,
        // premultiplied alpha from 0.01 to 0.05, and 0.05 to 0.09
        // may not cause the same amount of information loss), but
        // for now it should be enough. In the future this can also
        // be made discrete with regard to 8-bit information loss.
        readonly property real strength: (1.0 - root.color.a) * (root.animationDuration / 200.0 / 100.0)

        property real seed: 0.0

        UniformAnimator on seed {
            running: temporalNoise.visible
            from: 0
            to: 1
            loops: Animation.Infinite
            duration: 1000
        }

        fragmentShader: "qrc:///shaders/Noise.frag.qsb"
    }
}
