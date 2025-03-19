/*****************************************************************************
 * SurfaceTextureListener.java
 *****************************************************************************
 * Copyright © 2020 VideoLabs
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

package org.videolan.vlccore;

import android.graphics.SurfaceTexture;
import android.util.Log;

@SuppressWarnings("unused, JniMissingFunction")
public class SurfaceTextureListener
    implements SurfaceTexture.OnFrameAvailableListener {

    @SuppressWarnings("unused") /* Used from JNI */
    public SurfaceTextureListener(long listener) {
        nativeNew(listener); }

    @Override
    public void onFrameAvailable(SurfaceTexture surfaceTexture)
    {
        nativeOnFrameAvailable(surfaceTexture);
    }

    public native void nativeOnFrameAvailable(SurfaceTexture surfacetexture);

    @SuppressWarnings("unused") /* Used from JNI */
    public native void nativeNew(long listener);

    private long mListener;
}
