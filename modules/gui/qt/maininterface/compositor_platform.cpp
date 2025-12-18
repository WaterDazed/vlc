/*****************************************************************************
 * Copyright (C) 2024 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#include "compositor_platform.hpp"

#include <QApplication>
#include <QQuickView>
#include <QOperatingSystemVersion>

#include "maininterface/interface_window_handler.hpp"

#include <vlc_window.h>

#ifdef __APPLE__
#include <objc/runtime.h>
#endif

#ifdef __EMSCRIPTEN__
#if QT_VERSION >= QT_VERSION(6, 5, 0)
#define EMSCRIPTEN_SUPPORT
#include <emscripten/html5.h>
#endif
#endif

#ifdef QT_GUI_PRIVATE
#include <QtGui/qpa/qplatformnativeinterface.h>
#include <QtGui/qpa/qplatformwindow.h>
#include <QtGui/qpa/qplatformwindow_p.h>
#include <QtGui/qguiapplication_platform.h>

#ifndef X_DISPLAY_MISSING
#include <X11/Xlib.h>
#define X_ADJUST_DISPLAY
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
#if defined(Q_OS_UNIX)
#define QT_FEATURE_wayland 1
#else
#define QT_FEATURE_wayland -1
#endif
#endif

#if QT_CONFIG(wayland) && QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
// Don't bother with older versions, use CompositorWayland in that case.
#define WAYLAND_SUPPORT
#endif
#endif

using namespace vlc;


CompositorPlatform::CompositorPlatform(qt_intf_t *p_intf, QObject *parent)
    : CompositorVideo(p_intf, parent)
{

}

bool CompositorPlatform::init(bool enforce)
{
    // TODO: For now only qwindows and qdirect2d
    //       running on Windows 8+, and cocoa
    //       platforms are supported.

    const QString& platformName = qApp->platformName();

#ifdef _WIN32
    if ((QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows8) || Q_UNLIKELY(enforce))
    {
        if (platformName == QLatin1String("windows") || platformName == QLatin1String("direct2d"))
        {
            m_windowType = VLC_WINDOW_TYPE_HWND;
            return true;
        }
    }
#endif

#ifdef __APPLE__
    if (platformName == QLatin1String("cocoa"))
    {
        m_windowType = VLC_WINDOW_TYPE_NSOBJECT;
        return true;
    }
#endif

    if (Q_UNLIKELY(enforce))
    {
#ifdef EMSCRIPTEN_SUPPORT
        if (platformName == QLatin1String("wasm"))
        {
            m_windowType = VLC_WINDOW_TYPE_EMSCRIPTEN_WEBGL;
            return true;
        }
#endif

#ifdef WAYLAND_SUPPORT
        if (platformName.startsWith(QLatin1String("wayland")))
        {
            m_windowType = VLC_WINDOW_TYPE_WAYLAND;
            return true;
        }
#endif

        if (platformName == QLatin1String("xcb"))
        {
            m_windowType = VLC_WINDOW_TYPE_XID;
            return true;
        }
    }

    return false;
}

bool CompositorPlatform::makeMainInterface(MainCtx *mainCtx, std::function<void (QQuickWindow *)> aboutToShowQuickWindowCallback)
{
    m_mainCtx = mainCtx;

    m_rootWindow = std::make_unique<QWindow>();

    m_videoWindow = new QWindow(m_rootWindow.get());

#ifdef EMSCRIPTEN_SUPPORT
    if (m_windowType == VLC_WINDOW_TYPE_EMSCRIPTEN_WEBGL)
        m_videoWindow->setSurfaceType(QSurface::OpenGLSurface);
#endif

    m_quickWindow = new QQuickView(m_rootWindow.get());
    m_quickWindow->setResizeMode(QQuickView::SizeRootObjectToView);

    {
        // Transparency set-up:
        m_quickWindow->setColor(Qt::transparent);

        QSurfaceFormat format = m_quickWindow->format();
        format.setAlphaBufferSize(8);
        m_quickWindow->setFormat(format);
    }

    // Make sure the UI child window has the same size as the root parent window:
    connect(m_rootWindow.get(), &QWindow::widthChanged, m_quickWindow, &QWindow::setWidth);
    connect(m_rootWindow.get(), &QWindow::heightChanged, m_quickWindow, &QWindow::setHeight);

    m_quickWindow->create();

    const bool ret = commonGUICreate(m_rootWindow.get(), m_quickWindow, CompositorVideo::CAN_SHOW_PIP | CompositorVideo::HAS_ACRYLIC);

    m_quickWindow->setFlag(Qt::FramelessWindowHint);
    // Qt QPA Bug (qwindows, qdirect2d(?)): to trigger WS_EX_LAYERED set up.
    m_quickWindow->setOpacity(0.0);
    m_quickWindow->setOpacity(1.0);

    m_rootWindow->installEventFilter(this);

    if (aboutToShowQuickWindowCallback)
        aboutToShowQuickWindowCallback(m_quickWindow);

    m_rootWindow->setVisible(true);
    m_videoWindow->setVisible(true);
    m_quickWindow->setVisible(true);

    m_quickWindow->raise(); // Make sure quick window is above the video window.

    return ret;
}

void CompositorPlatform::destroyMainInterface()
{
    commonIntfDestroy();
}

void CompositorPlatform::unloadGUI()
{
    m_rootWindow->removeEventFilter(this);
    m_interfaceWindowHandler.reset();
    m_quickWindow->setSource(QUrl());
    commonGUIDestroy();
}

bool CompositorPlatform::setupVoutWindow(vlc_window_t *p_wnd, VoutDestroyCb destroyCb)
{
    if (m_wnd)
        return false;

    commonSetupVoutWindow(p_wnd, destroyCb);

    const auto setup = [&]() -> bool {
#ifdef __WIN32
        if (Q_LIKELY(m_windowType == VLC_WINDOW_TYPE_HWND))
        {
            p_wnd->handle.hwnd = reinterpret_cast<void*>(m_videoWindow->winId());
            return true;
        }
#endif

#ifdef __APPLE__
        if (Q_LIKELY(m_windowType == VLC_WINDOW_TYPE_NSOBJECT))
        {
            p_wnd->handle.nsobject = reinterpret_cast<id>(m_videoWindow->winId());
            return true;
        }
#endif

#ifdef EMSCRIPTEN_SUPPORT
        if (Q_LIKELY(m_windowType == VLC_WINDOW_TYPE_EMSCRIPTEN_WEBGL))
        {
            // VLC emscripten "window" is actually a OpenGL context, so
            // providing the DOM canvas is not enough, we need to create
            // the context. We could use `QOpenGLContext`, but with Qt 6
            // Qt 6 it does not seem to provide the native handle...

            const WId winId = m_videoWindow->winId();

            // Create OpenGL context, similar to how `QWasmOpenGLContext`
            // is created:
            const std::string canvas = "!qtwindow" + std::to_string(winId);

            EmscriptenWebGLContextAttributes attributes;
            emscripten_webgl_init_context_attributes(&attributes);
            attributes.explicitSwapControl = 1;

            const EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = emscripten_webgl_create_context(canvas.c_str(), &attributes);

            if (!context)
                return false;

            p_wnd->handle.em_context = reinterpret_cast<uint32_t>(context);

            return true;
        }
#endif

        if (m_windowType == VLC_WINDOW_TYPE_XID)
        {
            p_wnd->type = VLC_WINDOW_TYPE_XID;
            p_wnd->handle.xid = m_videoWindow->winId();
#ifdef X_ADJUST_DISPLAY
            assert(qGuiApp);
            const auto x11App = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
            assert(x11App);
            p_wnd->display.x11 = XDisplayString(x11App->display());
#endif
            return true;
        }

#ifdef WAYLAND_SUPPORT
        if (m_windowType == VLC_WINDOW_TYPE_WAYLAND)
        {
            assert(qGuiApp);
            const auto waylandApp = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
            assert(waylandApp);
            const auto waylandWindow = dynamic_cast<QNativeInterface::Private::QWaylandWindow *>(m_videoWindow->handle());
            assert(waylandWindow);

            p_wnd->handle.wl = waylandWindow->surface();
            p_wnd->display.wl = waylandApp->display();
        }
#endif

        return false;
    };

    if (setup())
    {
        p_wnd->type = m_windowType;
        return true;
    }

    return false;
}

QWindow *CompositorPlatform::interfaceMainWindow() const
{
    return m_rootWindow.get();
}

QQuickWindow *CompositorPlatform::quickWindow() const
{
    return m_quickWindow.get();
}

Compositor::Type CompositorPlatform::type() const
{
    return Compositor::PlatformCompositor;
}

QQuickItem *CompositorPlatform::activeFocusItem() const
{
    assert(m_quickWindow);
    return m_quickWindow->activeFocusItem();
}

bool CompositorPlatform::eventFilter(QObject *watched, QEvent *event)
{
    // Forward drag events to the child quick window,
    // as it is not done automatically by Qt with
    // nested windows:
    if (m_quickWindow && watched == m_rootWindow.get())
    {
        switch (event->type()) {
        case QEvent::DragEnter:
        case QEvent::DragLeave:
        case QEvent::DragMove:
        case QEvent::DragResponse:
        case QEvent::Drop:
            QApplication::sendEvent(m_quickWindow, event);
            return true;
        default:
            break;
        };
    }
    return false;
}

int CompositorPlatform::windowEnable(const vlc_window_cfg_t *)
{
    commonWindowEnable();
    return VLC_SUCCESS;
}

void CompositorPlatform::windowDisable()
{
    commonWindowDisable();
}

void CompositorPlatform::windowDestroy()
{
#ifdef EMSCRIPTEN_SUPPORT
    if (m_wnd && (m_wnd->type == VLC_WINDOW_TYPE_EMSCRIPTEN_WEBGL) && m_wnd->handle.em_context)
    {
        emscripten_webgl_destroy_context(reinterpret_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(m_wnd->handle.em_context));
        m_wnd->handle.em_context = 0;
    }
#endif

    CompositorVideo::windowDestroy();
}

void CompositorPlatform::onSurfacePositionChanged(const QPointF &position)
{
    const QPointF point = position / m_videoWindow->devicePixelRatio();
    m_videoWindow->setPosition({static_cast<int>(point.x()), static_cast<int>(point.y())});
}

void CompositorPlatform::onSurfaceSizeChanged(const QSizeF &size)
{
    const QSizeF area = (size / m_videoWindow->devicePixelRatio());
    m_videoWindow->resize({static_cast<int>(std::ceil(area.width())), static_cast<int>(std::ceil(area.height()))});
}
