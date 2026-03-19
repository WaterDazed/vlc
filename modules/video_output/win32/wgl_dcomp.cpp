/*****************************************************************************
 * wgl.c: WGL extension for OpenGL
 *****************************************************************************
 * Copyright © 2009-2016 VLC authors and VideoLAN
 *
 * Authors: Adrien Maglo <magsoft@videolan.org>
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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_vout_display.h>
#include <vlc_opengl.h>

#include <GL/glew.h>
#include <GL/wglew.h>

#include <windows.h>
#define D3D11_NO_HELPERS
#include <dcommon.h>
#include <d3d11_1.h>
#include <dcomp.h>
#include <comdef.h>

#include "../../video_chroma/d3d11_fmt.h"
#include "dxgi_swapchain.h"

using Microsoft::WRL::ComPtr;

typedef struct vout_display_sys_t
{
    HDC                   hGLDC;
    HGLRC                 hGLRC;
    HMODULE               hOpengl;
    vlc_gl_t              *gl;

    IDXGISurface1 *dxgi_surface;
    dxgi_swapchain *swapchain;
} vout_display_sys_t;

#define VLC_PFD_INITIALIZER []{ \
    PIXELFORMATDESCRIPTOR d {}; \
    d.nSize = sizeof(PIXELFORMATDESCRIPTOR); \
    d.nVersion = 1; \
    d.dwFlags = PFD_DRAW_TO_WINDOW | PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER_DONTCARE; \
    d.iPixelType = PFD_TYPE_RGBA; \
    d.cColorBits = 24; \
    d.iLayerType = PFD_MAIN_PLANE; \
    return d; \
}()

static void Swap(vlc_gl_t *gl)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t*>(gl->sys);
    DXGI_LocalSwapchainSwap(sys->swapchain);
    msg_Info(gl, "SWAP BUFFERS");

    msg_Info(gl, "GetSwapchain");
    ComPtr<IDXGISwapChain1> dxgiswapchain = DXGI_GetSwapChain1(sys->swapchain);

    HRESULT hr;

    msg_Info(gl, "GetBuffer");
    void *p_dxgi_surface = NULL;
    hr = dxgiswapchain->GetBuffer(0, __uuidof(IDXGISurface1), &p_dxgi_surface);
    assert( !FAILED(hr) );
    sys->dxgi_surface = static_cast<IDXGISurface1 *>(p_dxgi_surface);
}

static void *OurGetProcAddress(vlc_gl_t *gl, const char *name)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t*>(gl->sys);

    /* See https://www.khronos.org/opengl/wiki/Load_OpenGL_Functions */
    void *f= (void *)wglGetProcAddress(name);
    if(f == 0 || (f == (void*)0x1) || (f == (void*)0x2) ||
      (f == (void*)0x3) || (f == (void*)-1) )
    {
        f = (void *)GetProcAddress(sys->hOpengl, name);
    }
    return f;
}

static int MakeCurrent(vlc_gl_t *gl)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t*>(gl->sys);

    /* After painting with a common DC, the ReleaseDC function must be called
     * to release the DC. Class and private DCs do not have to be released.
     * ReleaseDC must be called from the same thread that called GetDC. The
     * number of DCs is limited only by available memory. */

    HDC winDC;
    sys->dxgi_surface->GetDC(false, &winDC);
    sys->hGLDC = winDC;
    bool success = wglMakeCurrent(winDC, sys->hGLRC);

    if (likely(success))
        return VLC_SUCCESS;

    /* vlc_gl_MakeCurrent should never fail. */

    DWORD dw = GetLastError();
    msg_Err(gl, "Cannot make wgl current, error %lx", dw);

    return VLC_EGENERIC;
}

static void ReleaseCurrent(vlc_gl_t *gl)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t*>(gl->sys);
    SwapBuffers(sys->hGLDC);
    wglMakeCurrent(sys->hGLDC, NULL);
    sys->dxgi_surface->ReleaseDC(NULL);
}

static void Resize(vlc_gl_t *gl, unsigned width, unsigned height)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t*>(gl->sys);
return;

    auto *dec_device = static_cast<d3d11_decoder_device_t*>(gl->device->opaque);
    d3d11_device_t *d3d_device = &dec_device->d3d_dev;

    HRESULT hr;

    ComPtr<IDXGIDevice> pDXGIDevice;
    hr = d3d_device->d3ddevice->QueryInterface(IID_PPV_ARGS(pDXGIDevice.GetAddressOf()));
    assert(!FAILED(hr));

    ComPtr<IDXGIAdapter> dxgiadapter;
    hr = pDXGIDevice->GetAdapter(dxgiadapter.GetAddressOf());
    assert(!FAILED(hr));

    libvlc_video_render_cfg_t cfg {};
    cfg.width = width;
    cfg.height = height;
    cfg.bitdepth = 8;
    cfg.full_range = true;

    const d3d_format_t *newPixelFormat = NULL;
    newPixelFormat = FindD3D11Format(gl, d3d_device, 0, DXGI_RGB_FORMAT,
            8, 0, 0, DXGI_CHROMA_CPU, D3D11_FORMAT_SUPPORT_DISPLAY );
    assert(newPixelFormat);
    msg_Info(gl, "FORMAT: %s : %4.4s", newPixelFormat->name, &newPixelFormat->fourcc);

    msg_Info(gl, "UpdateSwapchain %ux%u", width, height);
    if (!DXGI_UpdateSwapChain(sys->swapchain, dxgiadapter.Get(),
                              d3d_device->d3ddevice, newPixelFormat, &cfg))
    {
        vlc_assert_unreachable();
    }
}

static void Close(vlc_gl_t *gl)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t*>(gl->sys);

    if (sys->hGLRC)
        wglDeleteContext(sys->hGLRC);
    // TODO RELEASE DC
    if (sys->hOpengl)
        FreeLibrary(sys->hOpengl);

    free(sys);
}

static int OpenFromDComp(vlc_gl_t *gl, unsigned width, unsigned height)
{
    vout_display_sys_t *sys = static_cast<vout_display_sys_t*>(gl->sys);

    msg_Info(gl, "OPENING FROM DCOMP");
    vout_window_t *wnd = gl->surface;
    if (wnd->type != VOUT_WINDOW_TYPE_DCOMP || gl->device == NULL)
    {
        return VLC_EGENERIC;
    }

    if (gl->device->type != VLC_DECODER_DEVICE_D3D11VA)
    {
        msg_Info(gl, "WRONG DECDEV FOR DCOMP");
     //&& gl->device->type != VLC_DECODER_DEVICE_DXVA2)
        return VLC_EGENERIC;
    }
        
    auto *dec_device = static_cast<d3d11_decoder_device_t*>(gl->device->opaque);
    d3d11_device_t *d3d_device = &dec_device->d3d_dev;

    auto *device = static_cast<IDCompositionDevice *>(wnd->display.dcomp_device);
    auto *visual = static_cast<IDCompositionVisual *>(wnd->handle.dcomp_visual);
    HRESULT hr = S_OK;

    msg_Info(gl, "CreateSwapchain");
    dxgi_swapchain *sc = DXGI_CreateLocalSwapchainHandleDComp(VLC_OBJECT(gl),
        (void*)device, (void*)visual);

    msg_Info(gl, "Query interface");
    ComPtr<IDXGIDevice> pDXGIDevice;
    hr = d3d_device->d3ddevice->QueryInterface(IID_PPV_ARGS(pDXGIDevice.GetAddressOf()));
    assert(!FAILED(hr));
    if (FAILED(hr)) return VLC_EGENERIC;

    msg_Info(gl, "Get adapter");
    ComPtr<IDXGIAdapter> dxgiadapter;
    hr = pDXGIDevice->GetAdapter(dxgiadapter.GetAddressOf());
    assert(!FAILED(hr));
    if (FAILED(hr)) return VLC_EGENERIC;

    const d3d_format_t *newPixelFormat = NULL;
    newPixelFormat = FindD3D11Format(gl, d3d_device, 0, DXGI_RGB_FORMAT,
            8, 0, 0, DXGI_CHROMA_CPU, D3D11_FORMAT_SUPPORT_DISPLAY );
    assert(newPixelFormat);
    
    msg_Info(gl, "FORMAT: %s : %4.4s", newPixelFormat->name, &newPixelFormat->fourcc);
    assert(newPixelFormat->formatTexture == DXGI_FORMAT_B8G8R8A8_UNORM);
    libvlc_video_render_cfg_t cfg {};
    cfg.width = width;
    cfg.height = height;
    cfg.bitdepth = 8;
    cfg.full_range = true;

    msg_Info(gl, "UpdateSwapchain");
    if (!DXGI_UpdateSwapChain(sc, dxgiadapter.Get(), d3d_device->d3ddevice, newPixelFormat, &cfg))
    {
        assert(!"CANNOT UPDATE SWAPCHAIN");
        return VLC_EGENERIC;
    }
#if 0
    IDCompositionSurface *surface;
    hr = device->CreateSurface(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
                          DXGI_ALPHA_MODE_IGNORE, &surface);
    if (hr != S_OK) return VLC_EGENERIC;;

    void *p_dxgi_surface = NULL;
    POINT pointOffset = { 0, 0 };
    surface->BeginDraw(NULL, __uuidof(IDXGISurface1), &p_dxgi_surface,
                       &pointOffset);
    auto *dxgi_surface = static_cast<IDXGISurface1 *>(p_dxgi_surface);
#endif

    msg_Info(gl, "GetSwapchain");
    ComPtr<IDXGISwapChain1> dxgiswapchain = DXGI_GetSwapChain1(sc);

    msg_Info(gl, "GetBuffer");
    void *p_dxgi_surface = NULL;
    hr = dxgiswapchain->GetBuffer(0, __uuidof(IDXGISurface1), &p_dxgi_surface);
    assert( !FAILED(hr) );
    sys->dxgi_surface = static_cast<IDXGISurface1 *>(p_dxgi_surface);
    
    msg_Info(gl, "GetDC");
    hr = sys->dxgi_surface->GetDC(false, &sys->hGLDC);
    //assert( !FAILED(hr) );
    if (FAILED(hr))
    {
        _com_error err(hr);
        LPCTSTR errMsg = err.ErrorMessage();
        MessageBox(0, errMsg, errMsg, MB_ICONINFORMATION);
        msg_Info(gl, "hr=%X", hr); 
        assert( !FAILED(hr) );
    }
    
    msg_Info(gl, "OK ALMOST"); 

    sys->swapchain = sc;
    
    return VLC_SUCCESS;
}

static int Open(vlc_gl_t *gl, unsigned width, unsigned height)
{
    vout_display_sys_t *sys;

    vout_window_t *wnd = gl->surface;

    if (wnd->type != VOUT_WINDOW_TYPE_DCOMP)
        return VLC_EGENERIC;

    /* Allocate structure */
    gl->sys = sys = static_cast<vout_display_sys_t *>(calloc(1, sizeof(*sys)));
    if (!sys)
        return VLC_ENOMEM;

    if (OpenFromDComp(gl, width, height) != VLC_SUCCESS)
    {
        Close(gl);
        return VLC_EGENERIC;
    }

    assert(sys->hGLDC != NULL);
    sys->hOpengl = LoadLibraryA("opengl32.dll");

    /* Set the pixel format for the DC */
    PIXELFORMATDESCRIPTOR pfd = VLC_PFD_INITIALIZER;
    SetPixelFormat(sys->hGLDC, ChoosePixelFormat(sys->hGLDC, &pfd), &pfd);

    /* Create the context. */
    sys->hGLRC = wglCreateContext(sys->hGLDC);
    if (sys->hGLRC == NULL)
    {
        msg_Err(gl, "Could not create the OpenGL rendering context");
        return VLC_EGENERIC;
    }

    
    wglMakeCurrent(sys->hGLDC, sys->hGLRC);

#ifdef WGL_EXT_swap_control
    /* Create an GPU Affinity DC */
    const char *extensions = (const char*)glGetString(GL_EXTENSIONS);
    if (vlc_gl_StrHasToken(extensions, "WGL_EXT_swap_control")) {
        PFNWGLSWAPINTERVALEXTPROC SwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
        if (SwapIntervalEXT)
            SwapIntervalEXT(1);
    }
#endif

    wglMakeCurrent(sys->hGLDC, NULL);
    sys->dxgi_surface->ReleaseDC(NULL);

    gl->make_current = MakeCurrent;
    gl->release_current = ReleaseCurrent;
    gl->resize = Resize;
    gl->swap = Swap;
    gl->get_proc_address = OurGetProcAddress;
    //gl->close = Close;

    (void) width; (void) height;
    return VLC_SUCCESS;
}

vlc_module_begin()
    set_shortname("WGL DirectComposition")
    set_description(N_("WGL extension for OpenGL through DirectComposition"))
    set_subcategory(SUBCAT_VIDEO_VOUT)

    set_capability("opengl", 51)
    set_callback(Open)
    add_shortcut("wgl")
vlc_module_end()


