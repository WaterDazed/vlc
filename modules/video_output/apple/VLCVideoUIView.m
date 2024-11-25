/*****************************************************************************
 * VLCVideoUIView.m: iOS vout window provider
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
#import <vlc_dialog.h>
#import <vlc_mouse.h>
#import <vlc_threads.h>
#import <vlc_window.h>

#import <assert.h>

#import "VLCVoutWindow.h"
#import "VLCDrawable.h"

@interface VLCVoutWindow : NSObject <VLCVoutWindow>
- (id)initWithWindow:(vlc_window_t *)wnd;
- (BOOL)getWindowLocked:(void(^)(vlc_window_t*))completion;
- (void)enable;
- (void)disable;
- (void)close;
@end

@interface VLCVoutWindowView : UIView <VLCVoutWindowView>
- (id)initWithWindow:(VLCVoutWindow *)window;
@end

@implementation VLCVoutWindow {
    vlc_window_t *_window;
    vlc_mutex_t _window_mutex;
    VLCVoutWindowView *_view;

    /* Window state */
    BOOL _enabled;
}

- (id)initWithWindow:(vlc_window_t *)window {
    self = [super init];
    if (!self)
        return nil;
    _window = window;

    vlc_mutex_init(&_window_mutex);

    return self;
}

- (BOOL)getWindowLocked:(void(^)(vlc_window_t*))completion {
    vlc_mutex_lock(&_window_mutex);
    if (!_window) {
        vlc_mutex_unlock(&_window_mutex);
        return NO;
    }
    completion(_window);
    vlc_mutex_unlock(&_window_mutex);
    return YES;
}

- (void)view:(void(^)(id<VLCVoutWindowView>))completion {
    __block VLCVoutWindow *window = self;
    dispatch_block_t block = ^{
        if (!window)
            return;
        if (!window->_view)
            window->_view = [[VLCVoutWindowView alloc] initWithWindow:(VLCVoutWindow *)window];
        completion(window->_view);
        window = nil;
    };
    dispatch_async(dispatch_get_main_queue(), block);
}

- (void)enable {
    assert(!_enabled);
    _enabled = YES;

    [self view:^(id<VLCVoutWindowView> view){
        [view enable];
    }];
}

- (void)disable {
    assert(_enabled);
    _enabled = NO;

    [self view:^(id<VLCVoutWindowView> view){
        [view disable];
    }];
}

- (void)close {
    vlc_mutex_lock(&_window_mutex);
    _window = NULL;
    vlc_mutex_unlock(&_window_mutex);
}

@end

@implementation VLCVoutWindowView {
    __weak VLCVoutWindow *_window;

    id<VLCDrawable> _viewContainer;

    /* Window observer for mouse-like events. */
    UITapGestureRecognizer *_tapRecognizer;

    /* Constraints */
    NSArray<NSLayoutConstraint*> *_constraints;
}

- (id)initWithWindow:(VLCVoutWindow *)window {
    id<VLCDrawable> superview = [self fetchViewContainer:window];
    if (superview == nil)
        return nil;

    self = [super initWithFrame:[superview frame]];
    if (!window || !self)
        return nil;

    _window = window;
    _viewContainer = superview;

    self.translatesAutoresizingMaskIntoConstraints = NO;

    [_window getWindowLocked:^(vlc_window_t *window){
        if (var_InheritBool( window, "mouse-events" ) == true) {
            _tapRecognizer = [[UITapGestureRecognizer alloc]
                initWithTarget:self action:@selector(tapRecognized:)];
            _tapRecognizer.cancelsTouchesInView = NO;
        }
    }];

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];

    CGSize size = superview.bounds.size;

    [_window getWindowLocked:^(vlc_window_t *window){
        vlc_window_ReportSize(window, size.width, size.height);
    }];

    return self;
}

- (id<VLCDrawable>)fetchViewContainer:(VLCVoutWindow *)window
{
    __block id<VLCDrawable> viewContainer = nil;
    [window getWindowLocked:^(vlc_window_t *window){
        @try {
            /* get the object we will draw into */
            id drawable = (__bridge id)var_InheritAddress (window, "drawable-nsobject");
            if (unlikely(drawable == nil)) {
                msg_Err(window, "provided view container is nil");
                return;
            }

            if (unlikely(![drawable respondsToSelector:@selector(isKindOfClass:)])) {
                msg_Err(window, "void pointer not an ObjC object");
                return;
            }

            if (unlikely(![drawable respondsToSelector:@selector(addSubview:)])) {
                msg_Err(window, "view container doesn't responds to addSubview:");
                return;
            }

            if (unlikely(![drawable respondsToSelector:@selector(bounds)])) {
                msg_Err(window, "view container doesn't responds to bounds");
                return;
            }

            if (unlikely(![drawable respondsToSelector:@selector(frame)])) {
                msg_Err(window, "view container doesn't responds to frame");
                return;
            }

            viewContainer = (id<VLCDrawable>)drawable;
        } @catch (NSException *exception) {
            msg_Err(window, "Handling the view container failed due to an Obj-C exception (%s, %s", [exception.name UTF8String], [exception.reason UTF8String]);
        }
    }];
    return viewContainer;
}

- (void)didMoveToSuperview {
    if ([self superview] == nil)
        return;

    if (_constraints != nil)
        return;

    _constraints = @[
        [self.centerXAnchor constraintEqualToAnchor:[[self superview] centerXAnchor]],
        [self.centerYAnchor constraintEqualToAnchor:[[self superview] centerYAnchor]],
        [self.widthAnchor constraintEqualToAnchor:[[self superview] widthAnchor]],
        [self.heightAnchor constraintEqualToAnchor:[[self superview] heightAnchor]],
    ];
    [NSLayoutConstraint activateConstraints:_constraints];
}

- (void)didMoveToWindow {
#if !defined(TARGET_OS_VISION) || !TARGET_OS_VISION
    self.contentScaleFactor = self.window.screen.scale;
#endif
}

- (void)layoutSubviews {
    [self reshape];
}

- (void)updateConstraints
{
    [super updateConstraints];
    [self reshape];
}

- (void)reshape
{
    assert([NSThread isMainThread]);

    CGSize viewSize = [self bounds].size;
    CGFloat scaleFactor = self.contentScaleFactor;

    [_window getWindowLocked:^(vlc_window_t *window){
        vlc_window_ReportSize(window, viewSize.width * scaleFactor, viewSize.height * scaleFactor);
    }];
}

- (void)tapRecognized:(UITapGestureRecognizer *)tapRecognizer
{
    UIGestureRecognizerState state = [tapRecognizer state];
    CGPoint touchPoint = [tapRecognizer locationInView:self];
    CGFloat scaleFactor = self.contentScaleFactor;

    [_window getWindowLocked:^(vlc_window_t *window){
        vlc_window_ReportMouseMoved(window,
                (int)touchPoint.x * scaleFactor, (int)touchPoint.y * scaleFactor);
        vlc_window_ReportMousePressed(window, MOUSE_BUTTON_LEFT);
        vlc_window_ReportMouseReleased(window, MOUSE_BUTTON_LEFT);
    }];
}

/* Subview are expected to fill the whole frame so tell the compositor
 * that it doesn't have to bother with what's behind the window. */
- (BOOL)isOpaque
{
    return YES;
}

/* Prevent the subviews (which are renderers only) to get events so that
 * they can be dispatched from this vout_window module. */
- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (void)enable {
    /**
     * Given -[UIView addGestureRecognizer:] can raise an exception if
     * tapRecognizer is nil and given tapRecognizer can be nil if
     * "mouse-events" var == false, then add tapRecognizer to the view only if
     * it's not nil
     */
    if (_tapRecognizer != nil)
        [self addGestureRecognizer:_tapRecognizer];

    [_viewContainer addSubview:self];
}

- (void)disable {
    [self removeFromSuperview];
    _constraints = nil;

    [_tapRecognizer.view removeGestureRecognizer:_tapRecognizer];
}

@end

/**
 * C core wrapper of the vout window operations for the ObjC module.
 */

static int Enable(vlc_window_t *wnd, const vlc_window_cfg_t *cfg)
{
    VLCVoutWindow *window = (__bridge VLCVoutWindow *)wnd->sys;
    [window enable];
    return VLC_SUCCESS;
}

static void Disable(vlc_window_t *wnd)
{
    VLCVoutWindow *window = (__bridge VLCVoutWindow *)wnd->sys;
    [window disable];
}

static void Close(vlc_window_t *wnd)
{
    VLCVoutWindow *sys = (__bridge_transfer VLCVoutWindow *)wnd->sys;
    [sys close];
}

static const struct vlc_window_operations window_ops =
{
    .enable = Enable,
    .disable = Disable,
    .destroy = Close,
};

static int Open(vlc_window_t *wnd)
{
    VLCVoutWindow *sys = [[VLCVoutWindow alloc] initWithWindow:wnd];
    wnd->sys = (__bridge_retained void *)sys;
    
    if (wnd->sys == NULL)
    {
        msg_Err(wnd, "Creating UIView window provider failed");
        return VLC_EGENERIC;
    }

    wnd->type = VLC_WINDOW_TYPE_NSOBJECT;
    wnd->handle.nsobject = wnd->sys;
    wnd->ops = &window_ops;

    return VLC_SUCCESS;
}

vlc_module_begin ()
    set_shortname("UIView")
    set_description("iOS UIView vout window provider")
    set_subcategory(SUBCAT_VIDEO_VOUT)
    set_capability("vout window", 300)
    set_callback(Open)

    add_shortcut("uiview", "ios")
vlc_module_end ()
