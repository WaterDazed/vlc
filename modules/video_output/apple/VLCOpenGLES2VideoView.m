/*****************************************************************************
 * VLCOpenGLES2VideoView.m: iOS OpenGL ES provider through CAEAGLLayer
 *****************************************************************************
 * Copyright (C) 2001-2024 VLC authors and VideoLAN
 * Copyright (C) 2024 Videolabs
 *
 * Authors: Pierre d'Herbemont <pdherbemont at videolan dot org>
 *          Felix Paul Kühne <fkuehne at videolan dot org>
 *          David Fuhrmann <david dot fuhrmann at googlemail dot com>
 *          Rémi Denis-Courmont
 *          Laurent Aimar <fenrir _AT_ videolan _DOT_ org>
 *          Eric Petit <titer@m0k.org>
 *          Alexandre Janniaux <ajanni@videolabs.io>
 *          Maxime Chapelet <umxprime at videolabs dot io>
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

/*****************************************************************************
 * Preamble
 *****************************************************************************/

#import <UIKit/UIKit.h>
#import <OpenGLES/EAGL.h>
#import <OpenGLES/ES2/gl.h>
#import <OpenGLES/ES2/glext.h>
#import <QuartzCore/QuartzCore.h>
#import <dlfcn.h>

#ifdef HAVE_CONFIG_H
# import "config.h"
#endif

#import <vlc_common.h>
#import <vlc_plugin.h>
#import <vlc_vout_display.h>
#import <vlc_opengl.h>
#import <vlc_dialog.h>
#import "../opengl/vout_helper.h"
#import "../opengl/gl_api.h"

#import "VLCVoutWindow.h"

@interface VLCOpenGLESDisplay : NSObject
@property(nonatomic, readonly) EAGLContext *eaglContext;
- (id)initWithFrame:(CGRect)frame gl:(vlc_gl_t*)gl;
- (id<VLCVoutWindow>)window;
- (CGRect)initialFrame;
- (BOOL)makeCurrent;
- (void)releaseCurrent;
- (void)swap;
- (void)resize:(CGSize)size;
- (void)close;
@end

@interface VLCOpenGLESDisplayView : UIView
- (id)initWithDisplay:(VLCOpenGLESDisplay *)display;
- (void)resize:(CGSize)size context:(EAGLContext *)context;
@end

/*****************************************************************************
 * vlc_gl_t callbacks
 *****************************************************************************/
static void *GetSymbol(vlc_gl_t *gl, const char *name)
{
    VLC_UNUSED(gl);
    return dlsym(RTLD_DEFAULT, name);
}

static int MakeCurrent(vlc_gl_t *gl)
{
    VLCOpenGLESDisplay *display = (__bridge VLCOpenGLESDisplay *)gl->sys;

    if (![display makeCurrent])
        return VLC_EGENERIC;
    return VLC_SUCCESS;
}

static void ReleaseCurrent(vlc_gl_t *gl)
{
    VLCOpenGLESDisplay *display = (__bridge VLCOpenGLESDisplay *)gl->sys;
    [display releaseCurrent];
}

static void Swap(vlc_gl_t *gl)
{
    VLCOpenGLESDisplay *display = (__bridge VLCOpenGLESDisplay *)gl->sys;
    [display swap];
}

static void Resize(vlc_gl_t *gl, unsigned width, unsigned height)
{
    /* Use the parent frame size for now, resize is smoother and called
     * automatically from the main thread queue. */
    VLCOpenGLESDisplay *display = (__bridge VLCOpenGLESDisplay *)gl->sys;
    [display resize:CGSizeMake(width, height)];
}

static void Close(vlc_gl_t *gl)
{
    /* Transfer ownership back from VLC to ARC so that it can be released. */
    VLCOpenGLESDisplay *display = (__bridge_transfer VLCOpenGLESDisplay *)gl->sys;

    /* We need to detach because the superview has a reference to our view. */
    [display close];
}

@implementation VLCOpenGLESDisplay {
    VLCOpenGLESDisplayView *_view;

    CGRect _initialFrame;

    vlc_gl_t *_gl;

    EAGLContext *_previousEaglContext;
}

- (id)initWithFrame:(CGRect)frame gl:(vlc_gl_t*)gl {
    self = [super init];
    if (!self)
        return nil;

    _initialFrame = frame;
    _gl = gl;

    _eaglContext = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES2];
    _previousEaglContext = nil;

    static const struct vlc_gl_operations gl_ops =
    {
        .make_current = MakeCurrent,
        .release_current = ReleaseCurrent,
        .resize = Resize,
        .swap = Swap,
        .get_proc_address = GetSymbol,
        .close = Close,
    };
    gl->ops = &gl_ops;

    [self view:^(VLCOpenGLESDisplayView *view){(void)view;}];

    return self;
}

- (void)view:(void(^)(VLCOpenGLESDisplayView *))completion {
    __block VLCOpenGLESDisplay *display = self;
    dispatch_block_t block = ^{
        if (!display)
            return;
        if (!display->_view)
            display->_view = [[VLCOpenGLESDisplayView alloc] initWithDisplay:display];
        completion(display->_view);
        display = nil;
    };
    dispatch_async(dispatch_get_main_queue(), block);
}

- (CGRect)initialFrame {
    return _initialFrame;
}

- (BOOL)makeCurrent {
    assert(![NSThread isMainThread]);

    _previousEaglContext = [EAGLContext currentContext];

    assert(_eaglContext);
    BOOL result = [EAGLContext setCurrentContext:_eaglContext];
    assert(result == YES);

    return YES;
}

- (void)releaseCurrent {
    [EAGLContext setCurrentContext:_previousEaglContext];
    _previousEaglContext = nil;
}

- (void)swap {
    [_eaglContext presentRenderbuffer:GL_RENDERBUFFER];
}

- (void)resize:(CGSize)size {
    EAGLContext *context = _eaglContext;
    [self view:^(VLCOpenGLESDisplayView *view){
        [view resize:size context:context];
    }];
}

- (void)close {
    EAGLContext *previousEaglContext = [EAGLContext currentContext];
    if ([EAGLContext setCurrentContext:_eaglContext])
        glFinish();
    [EAGLContext setCurrentContext:previousEaglContext];
    _eaglContext = nil;
    [self view:^(VLCOpenGLESDisplayView *view){
        [view removeFromSuperview];
    }];
}

- (id<VLCVoutWindow>)window {
    @try {
        id<VLCVoutWindow> window = (__bridge id<VLCVoutWindow>)_gl->surface->handle.nsobject;
        if (unlikely(window == nil)) {
            msg_Err(_gl, "provided view container is nil");
            return nil;
        }

        if (unlikely(![window respondsToSelector:@selector(conformsToProtocol:)])) {
            msg_Err(_gl, "void pointer not an ObjC object");
            return nil;
        }

        if (unlikely(![window conformsToProtocol:@protocol(VLCVoutWindow)])) {
            msg_Err(_gl, "passed ObjC object not of class UIView");
            return nil;
        }
        return window;
    } @catch (NSException *exception) {
        msg_Err(_gl, "Handling the view container failed due to an Obj-C exception (%s, %s", [exception.name UTF8String], [exception.reason UTF8String]);
    }
    return nil;
}

@end

@implementation VLCOpenGLESDisplayView {
    CAEAGLLayer *_layer;

    GLuint _renderBuffer;
    GLuint _frameBuffer;
}

- (id)initWithDisplay:(VLCOpenGLESDisplay *)display {
    self = [super initWithFrame:[display initialFrame]];
    if (!self)
        return nil;

    _layer = (CAEAGLLayer *)self.layer;
    _layer.drawableProperties = [NSDictionary dictionaryWithObject:kEAGLColorFormatRGBA8 forKey: kEAGLDrawablePropertyColorFormat];
    _layer.opaque = YES;

    self.contentMode = UIViewContentModeScaleToFill;

    id<VLCVoutWindow> window = [display window];
    if (!window)
        return nil;

    __weak UIView *displayView = self;
    [window view:^(id<VLCVoutWindowView> windowView){
        [windowView addSubview:displayView];
        NSLog(@"VLCOpenGLDisplay is visible !");
    }];
    
    [self prepareBuffersWithContext:display.eaglContext];

    return self;
}

- (BOOL)prepareBuffersWithContext:(EAGLContext *)context
{
    if (_frameBuffer != 0)
    {
        /* clear frame buffer */
        glDeleteFramebuffers(1, &_frameBuffer);
        _frameBuffer = 0;
    }

    if (_renderBuffer != 0)
    {
        /* clear render buffer */
        glDeleteRenderbuffers(1, &_renderBuffer);
        _renderBuffer = 0;
    }

    glGenFramebuffers(1, &_frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer);

    glGenRenderbuffers(1, &_renderBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, _renderBuffer);

    NSLog(@"Size: %fx%f", self.bounds.size.width, self.bounds.size.height);
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:_layer];

    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, _renderBuffer);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        NSLog(@"Failed to make complete framebuffer object %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
        return NO;
    }
    return YES;
}

- (void)resize:(CGSize)size context:(EAGLContext *)context {
    UIScreen* screen = self.window.screen;
    if (screen == nil)
        screen = [UIScreen mainScreen];
    CGFloat scaleFactor = screen.nativeScale;
    self.contentScaleFactor = scaleFactor;

    CGRect rect = self.frame;
    rect.size = size;

    rect.size.width /= scaleFactor;
    rect.size.height /= scaleFactor;

    self.frame = rect;

    if (size.width != 0 && size.height != 0)
    {
        EAGLContext *previousContext = [EAGLContext currentContext];
        [EAGLContext setCurrentContext:context];
        [self prepareBuffersWithContext:context];
        [EAGLContext setCurrentContext:previousContext];
    }
}

- (void)didMoveToWindow {
    self.contentScaleFactor = self.window.screen.scale;
}

- (BOOL)isOpaque
{
    return YES;
}

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
    /* Disable events for this view, as the vout_window view will be the one
     * handling them. */
    return nil;
}

+ (Class)layerClass {
    return [CAEAGLLayer class];
}

@end

static int Open(vlc_gl_t *gl, unsigned width, unsigned height)
{
    vlc_window_t *wnd = gl->surface;
    
    /* We only support UIView container window. */
    if (wnd->type != VLC_WINDOW_TYPE_NSOBJECT)
        return VLC_EGENERIC;

    @autoreleasepool {
        /* NOTE: we're using CFRunLoopPerformBlock with the "vlc_runloop" tag
         * to avoid deadlocks between the window module (main thread) and the
         * display module, which would happen when using dispatch_sycn here. */

        gl->sys = (__bridge_retained void*)[[VLCOpenGLESDisplay alloc]
                                            initWithFrame:CGRectMake(0.,0.,width,height) gl:gl];

        if (gl->sys == NULL)
        {
            msg_Err(gl, "Creating OpenGL ES 2 view failed");
            return VLC_EGENERIC;
        }
    }

    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_shortname (N_("CAEAGL"))
    set_description (N_("CAEAGL provider for OpenGL"))
    set_subcategory (SUBCAT_VIDEO_VOUT)
    set_capability ("opengl es2", 51)
    set_callback(Open)
    add_shortcut ("caeagl")
vlc_module_end ()
