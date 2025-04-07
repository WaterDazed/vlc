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
import QtQuick.Window

import VLC.MainInterface

import org.kde.layershell as LayerShell

MainInterface {
    Window.onWindowChanged: {
        if (Window.window) {
            console.warn("Attempting to use layer shell, most features available with xdg shell will not be available.")
            Window.window.LayerShell // at this point, QML engine should create the attached layer shell
            if (MainCtx.interfaceAlwaysOnTop) {
                // Video always on top:
                Window.window.LayerShell.Window.layer = LayerShell.Window.LayerTop
            }
        }
    }
}
