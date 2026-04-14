/*****************************************************************************
 * VLCPlaybackProgressSlider.m
 *****************************************************************************
 * Copyright (C) 2017 VLC authors and VideoLAN
 *
 * Authors: Marvin Scholz <epirat07 at gmail dot com>
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

#import "VLCPlaybackProgressSlider.h"

#import "vlc_common.h"

#import "extensions/NSView+VLCAdditions.h"
#import "views/VLCPlaybackProgressSliderCell.h"

@interface VLCPlaybackProgressSlider ()
{
    BOOL             _isShowingPreview;
    NSTimer         *_previewTimer;
    NSTrackingArea  *_trackingArea;
}
@end

@implementation VLCPlaybackProgressSlider

+ (Class)cellClass
{
    return VLCPlaybackProgressSliderCell.class;
}

- (instancetype)initWithCoder:(NSCoder *)coder
{
    self = [super initWithCoder:coder];

    if (self) {
        NSAssert([self.cell isKindOfClass:[VLCPlaybackProgressSlider cellClass]],
                 @"VLCPlaybackProgressSlider cell is not a VLCPlaybackProgressSliderCell");
        self.scrollable = YES;
        if (@available(macOS 10.14, *)) {
            [self viewDidChangeEffectiveAppearance];

            if (@available(macOS 13, *)) {
                // Apple's documentation says the clipping default changed in macOS 13.
                // Use a dynamic call so this still builds with SDKs that don't declare the selector.
                SEL clipsToBoundsSelector = NSSelectorFromString(@"setClipsToBounds:");
                if ([self respondsToSelector:clipsToBoundsSelector]) {
                    void (*setClipsToBoundsImp)(id, SEL, BOOL) =
                        (void (*)(id, SEL, BOOL))[self methodForSelector:clipsToBoundsSelector];
                    setClipsToBoundsImp(self, clipsToBoundsSelector, YES);
                }
            }
        } else {
            [(VLCPlaybackProgressSliderCell*)self.cell setSliderStyleLight];
        }

        [self setupTrackingArea];
    }
    return self;
}

- (void)dealloc
{
    [self removeTrackingArea];
}

- (void)setupTrackingArea
{
    [self removeTrackingArea];
    _trackingArea = [[NSTrackingArea alloc]
        initWithRect:self.bounds
             options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved |
                      NSTrackingActiveInKeyWindow)
               owner:self
            userInfo:nil];
    [self addTrackingArea:_trackingArea];
}

- (void)removeTrackingArea
{
    if (_trackingArea) {
        [self removeTrackingArea:_trackingArea];
        _trackingArea = nil;
    }
}

- (void)updateTrackingAreas
{
    [super updateTrackingAreas];
    [self setupTrackingArea];
}

- (void)scrollWheel:(NSEvent *)event
{
    if (!self.scrollable) {
        return [super scrollWheel:event];
    }

    double increment;
    CGFloat deltaY = [event scrollingDeltaY];
    double range = [self maxValue] - [self minValue];

    // Scroll less for high precision, else it's too fast
    if (event.hasPreciseScrollingDeltas) {
        increment = (range * 0.002) * deltaY;
    } else {
        if (deltaY == 0.0) {
            return;
        }
        increment = (range * 0.01 * deltaY);
    }

    // If scrolling is inversed, increment in other direction
    if (!event.isDirectionInvertedFromDevice) {
        increment = -increment;
    }

    self.doubleValue = self.doubleValue - increment;
    [self sendAction:self.action to:self.target];
}

// Workaround for 10.7
// http://stackoverflow.com/questions/3985816/custom-nsslidercell
- (void)setNeedsDisplayInRect:(NSRect)invalidRect
{
    [super setNeedsDisplayInRect:self.bounds];
}

- (BOOL)indefinite
{
    return [(VLCPlaybackProgressSliderCell*)self.cell indefinite];
}

- (void)setIndefinite:(BOOL)indefinite
{
    [(VLCPlaybackProgressSliderCell*)self.cell setIndefinite:indefinite];
}

- (BOOL)knobHidden
{
    return [(VLCPlaybackProgressSliderCell*)self.cell knobHidden];
}

- (void)setKnobHidden:(BOOL)knobHidden
{
    [(VLCPlaybackProgressSliderCell*)self.cell setKnobHidden:knobHidden];
}

- (BOOL)isFlipped
{
    return NO;
}

- (void)viewDidChangeEffectiveAppearance
{
    if (self.shouldShowDarkAppearance) {
        [(VLCPlaybackProgressSliderCell*)self.cell setSliderStyleDark];
    } else {
        [(VLCPlaybackProgressSliderCell*)self.cell setSliderStyleLight];
    }
}

#pragma mark -
#pragma mark Preview

- (void)mouseExited:(NSEvent *)event
{
    [self hidePreview];
}

- (void)mouseMoved:(NSEvent *)event
{
    if (!self.previewDelegate) {
        return;
    }
    
    if ([NSApp currentEvent].type == NSEventTypeLeftMouseDragged) {
        return;
    }
    
    if (_isShowingPreview) {
        [self cancelPreviewTimer];
        _previewTimer = [NSTimer scheduledTimerWithTimeInterval:0.05
                                                         target:self
                                                       selector:@selector(showPreviewFromTimer:)
                                                       userInfo:@{@"event": event}
                                                        repeats:NO];
    } else {
        [self showPreviewForMouseEvent:event];
    }
}

- (void)showPreviewFromTimer:(NSTimer *)timer
{
    NSEvent *event = timer.userInfo[@"event"];
    if (event) {
        [self showPreviewForMouseEvent:event];
    }
    _previewTimer = nil;
}

- (void)showPreviewForMouseEvent:(NSEvent *)event 
{
    if (!self.previewDelegate) {
        return;
    }

    NSPoint mouseLocation = [self convertPoint:event.locationInWindow fromView:nil];
    float position = [self positionForMouseLocation:mouseLocation];

    NSPoint windowMouseLocation = event.locationInWindow;
    NSPoint screenMouseLocation;

    if (@available(macOS 10.12, *)) {
        screenMouseLocation = [self.window convertPointToScreen:windowMouseLocation];
    } else {
        screenMouseLocation = [self.window convertBaseToScreen:windowMouseLocation];
    }

    if (_isShowingPreview) {
        [self.previewDelegate slider:self updatePreviewAtPosition:position mouseLocation:screenMouseLocation];
    } else {
        _isShowingPreview = YES;
        [self.previewDelegate slider:self showPreviewAtPosition:position mouseLocation:screenMouseLocation];
    }
}

- (float)positionForMouseLocation:(NSPoint)mouseLocation
{
    NSRect trackRect = [self.cell trackRect];
    
    if (trackRect.size.width <= 0) {
        return 0.0f;
    }
    
    float relativeX = (mouseLocation.x - trackRect.origin.x) / trackRect.size.width;
    relativeX = VLC_CLIP(relativeX, 0.0f, 1.0f);

    return relativeX;
}

- (void)hidePreview
{
    [self cancelPreviewTimer];
    _isShowingPreview = NO;
    
    if (self.previewDelegate) {
        [self.previewDelegate sliderHidePreview:self];
    }
}

- (void)cancelPreviewTimer
{
    if (_previewTimer) {
        [_previewTimer invalidate];
        _previewTimer = nil;
    }
}

- (void)mouseDown:(NSEvent *)event
{
    [self hidePreview];

    NSPoint mouseLocation = [self convertPoint:event.locationInWindow fromView:nil];
    float position = [self positionForMouseLocation:mouseLocation];

    self.floatValue = (self.maxValue - self.minValue) * position + self.minValue;

    [super mouseDown:event];
}

@end
