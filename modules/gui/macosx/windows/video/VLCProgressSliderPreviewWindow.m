/*****************************************************************************
 * VLCProgressSliderPreviewWindow.m: MacOS X interface module
 *****************************************************************************
 * Copyright (C) 2025 VLC authors and VideoLAN
 *
 * Authors: Bob Moriasi <official.bobmoriasi@gmail.com>
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

#import "VLCProgressSliderPreviewWindow.h"

#import "QuartzCore/QuartzCore.h"

static const CGFloat kPreviewWindowWidth = 320.0;
static const CGFloat kPreviewWindowHeight = 180.0;
static const CGFloat kPreviewWindowMargin = 5.0;
static const CGFloat kTimeLabelHeight = 20.0;
static const CGFloat kTimeLabelWidth = 60.0;
static const CGFloat kTimeLabelMargin = 12.0;
static const CGFloat kCornerRadius = 12.0;
static const CGFloat kScreenEdgeMargin = 10.0;

@interface VLCProgressSliderPreviewWindow()
{
    NSRect  _targetFrame;
    NSRect  _targetTimeLabelFrame;
    CGFloat _timeSliderY;
}

@property (nonatomic, strong) NSVisualEffectView    *visualEffectView;
@property (nonatomic, strong) NSProgressIndicator   *progressIndicator;
@property (nonatomic, strong) NSImageView   *previewImageView;
@property (nonatomic, strong) NSTextField   *timeLabel;
@property (nonatomic, strong) NSWindow      *timeLabelWindow;

@end;

@implementation VLCProgressSliderPreviewWindow

- (instancetype)init
{
    NSRect contentRect = NSMakeRect(0, 0, kPreviewWindowWidth, kPreviewWindowHeight);

    self = [super initWithContentRect:contentRect
                            styleMask:NSWindowStyleMaskBorderless
                              backing:NSBackingStoreBuffered
                                defer:NO];

    if (self) {
        [self setupWindow];
        [self setupViews];
        [self setupProgressIndicator];
    }

    return self;
}

- (void)setupViews
{
    _visualEffectView = [[NSVisualEffectView alloc] initWithFrame:self.contentView.bounds];
    
    if (@available(macOS 10.14, *)) {
        _visualEffectView.material = NSVisualEffectMaterialHUDWindow;
    } else {
        _visualEffectView.material = NSVisualEffectMaterialDark;
    }

    _visualEffectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
    _visualEffectView.state = NSVisualEffectStateActive;
    _visualEffectView.wantsLayer = YES;
    _visualEffectView.layer.cornerRadius = kCornerRadius;
    _visualEffectView.layer.masksToBounds = YES;
    [self.contentView addSubview:_visualEffectView];

    _previewImageView = [[NSImageView alloc] initWithFrame:self.contentView.bounds];
    _previewImageView.imageScaling = NSImageScaleAxesIndependently;
    _previewImageView.imageAlignment = NSImageAlignCenter;
    _previewImageView.wantsLayer = YES;
    _previewImageView.layer.contentsScale = 2.0;
    _previewImageView.layer.cornerRadius = kCornerRadius;
    _previewImageView.layer.masksToBounds = YES;
    [_visualEffectView addSubview:_previewImageView];

    [self setupTimeLabelWindow];
}

- (void)setupWindow
{
    self.alphaValue = 0.0;
    self.animationBehavior = NSWindowAnimationBehaviorNone;
    self.backgroundColor = [NSColor clearColor];
    self.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary;
    self.contentView.wantsLayer = YES;
    self.contentView.layer.cornerRadius = kCornerRadius;
    self.contentView.layer.masksToBounds = YES;
    self.hasShadow = YES;
    self.hidesOnDeactivate = NO;
    self.level = NSFloatingWindowLevel;
    self.opaque = NO;
}

- (void)setupProgressIndicator
{
    NSRect progressFrame = NSMakeRect(0, 0, kPreviewWindowWidth, 1.0);

    _progressIndicator = [[NSProgressIndicator alloc] initWithFrame:progressFrame];
    _progressIndicator.controlSize = NSControlSizeRegular;
    _progressIndicator.displayedWhenStopped = NO;
    _progressIndicator.indeterminate = YES;
    _progressIndicator.style = NSProgressIndicatorStyleBar;
    _progressIndicator.wantsLayer = YES;
    _progressIndicator.layer.cornerRadius = 0;

    [_visualEffectView addSubview:_progressIndicator];
}

- (void)setupTimeLabelWindow
{
    NSRect timeLabelRect = NSMakeRect(0, 0, kTimeLabelWidth, kTimeLabelHeight);
    _timeLabelWindow = [[NSWindow alloc] initWithContentRect:timeLabelRect 
                                                   styleMask:NSWindowStyleMaskBorderless 
                                                     backing:NSBackingStoreBuffered 
                                                       defer:NO];
    _timeLabelWindow.alphaValue = 0.0;
    _timeLabelWindow.backgroundColor = [NSColor clearColor];
    _timeLabelWindow.collectionBehavior = NSWindowCollectionBehaviorCanJoinAllSpaces | NSWindowCollectionBehaviorStationary;
    _timeLabelWindow.hasShadow = NO;
    _timeLabelWindow.level = NSFloatingWindowLevel + 1;
    _timeLabelWindow.opaque = NO;  

    _timeLabel = [[NSTextField alloc] initWithFrame:_timeLabelWindow.contentView.bounds];
    _timeLabel.alignment = NSTextAlignmentCenter;
    _timeLabel.backgroundColor = [NSColor clearColor];
    _timeLabel.bezeled = NO;
    _timeLabel.editable = NO;
    _timeLabel.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightMedium];
    _timeLabel.drawsBackground = NO;
    _timeLabel.selectable = NO;
    _timeLabel.stringValue = @"00:00";
    _timeLabel.textColor = [NSColor whiteColor];
    [_timeLabelWindow.contentView addSubview:_timeLabel];
}

- (NSPoint)constrainPointToScreen:(NSPoint)point
{
    NSScreen *screen = [NSScreen mainScreen];
    if (!screen) {
        return point;
    }
    
    NSRect screenFrame = screen.visibleFrame;
    CGFloat halfWidth = kPreviewWindowWidth / 2.0;
    
    CGFloat minX = screenFrame.origin.x + halfWidth + kScreenEdgeMargin;
    CGFloat maxX = screenFrame.origin.x + screenFrame.size.width - halfWidth - kScreenEdgeMargin;
    
    NSPoint constrainedPoint = point;
    constrainedPoint.x = MAX(minX, MIN(maxX, point.x));
    
    return constrainedPoint;
}

- (void)showAtPoint:(NSPoint)point timeString:(NSString *)timeString
{
    self.previewImageView.image = nil;
    [_progressIndicator startAnimation:nil];

    if (timeString) {
        self.timeLabel.stringValue = timeString;
    }

    NSPoint constrainedPoint = [self constrainPointToScreen:point];
    _timeSliderY = point.y;

    _targetFrame = NSMakeRect(0, 0, kPreviewWindowWidth, kPreviewWindowHeight);
    _targetFrame.origin.x = constrainedPoint.x - (_targetFrame.size.width / 2);
    _targetFrame.origin.y = point.y + kPreviewWindowMargin;
    _targetTimeLabelFrame = _timeLabelWindow.frame;
    _targetTimeLabelFrame.origin.x = constrainedPoint.x - (_targetTimeLabelFrame.size.width / 2);
    _targetTimeLabelFrame.origin.y = point.y + kPreviewWindowHeight + kTimeLabelMargin;

    NSRect initialFrame = _targetFrame;
    initialFrame.size.height = 1.0;
    initialFrame.origin.y = point.y + kPreviewWindowMargin;

    [self setFrame:initialFrame display:YES];
    [_timeLabelWindow setFrame:_targetTimeLabelFrame display:YES];

    self.alphaValue = 1.0;
    self.timeLabelWindow.alphaValue = 0.0;
    
    [self orderFront:nil];
    [_timeLabelWindow orderFront:nil];
    
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 0.15;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];

        [self.animator setFrame:_targetFrame display:YES];
        self.timeLabelWindow.animator.alphaValue = 1.0;
    } completionHandler:nil];
}

- (void)updateImage:(NSImage *)image timeString:(NSString *)timeString
{
    [_progressIndicator stopAnimation:nil];

    if (image) {
        self.previewImageView.image = image;
    }

    if (timeString) {
        self.timeLabel.stringValue = timeString;
    }
}

- (void)updatePosition:(NSPoint)point timeString:(NSString *)timeString completion:(void (^)(void))completion
{
    if (timeString) {
        self.timeLabel.stringValue = timeString;
    }

    NSPoint constrainedPoint = [self constrainPointToScreen:point];

    NSRect newTargetFrame = _targetFrame;
    newTargetFrame.origin.x = constrainedPoint.x - (newTargetFrame.size.width / 2);
    newTargetFrame.origin.y = point.y + kPreviewWindowMargin;

    NSRect newTargetTimeLabelFrame = _targetTimeLabelFrame;
    newTargetTimeLabelFrame.origin.x = constrainedPoint.x - (newTargetTimeLabelFrame.size.width / 2);
    newTargetTimeLabelFrame.origin.y = point.y + kPreviewWindowHeight + kTimeLabelMargin;

    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 0.2;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseOut];
        context.allowsImplicitAnimation = YES;

        _targetFrame = newTargetFrame;
        _targetTimeLabelFrame = newTargetTimeLabelFrame;

        [self.animator setFrame:newTargetFrame display:YES];
        [self.timeLabelWindow.animator setFrame:newTargetTimeLabelFrame display:YES];

    } completionHandler:completion];
}

- (void)hideWithAnimation
{
    [_progressIndicator stopAnimation:nil];
    
    NSRect currentFrame = self.frame;
    NSRect shrinkFrame = currentFrame;
    shrinkFrame.size.height = 1.0;
    shrinkFrame.origin.y = currentFrame.origin.y;
    
    [NSAnimationContext runAnimationGroup:^(NSAnimationContext *context) {
        context.duration = 0.15;
        context.timingFunction = [CAMediaTimingFunction functionWithName:kCAMediaTimingFunctionEaseIn];

        self.timeLabelWindow.animator.alphaValue = 0.0;
        [self.animator setFrame:shrinkFrame display:YES];
    } completionHandler:^{
        [self orderOut:nil];
        [self.timeLabelWindow orderOut:nil];
    }];
}

@end
