/*****************************************************************************
 * VLCProgressSliderPreviewController.m: MacOS X interface module
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

#import "VLCProgressSliderPreviewController.h"

#import "extensions/NSString+Helpers.h"
#import "library/VLCInputItem.h"
#import "library/VLCThumbnailCache.h"
#import "playqueue/VLCPlayerController.h"
#import "windows/video/VLCProgressSliderPreviewWindow.h"

@interface VLCProgressSliderPreviewController ()
{
    VLCPlayerController *_playerController;
    VLCProgressSliderPreviewWindow *_previewWindow;
    VLCThumbnailCache *_thumbnailCache;
    BOOL _isTimeSliderBeingDragged;
}
@end

@implementation VLCProgressSliderPreviewController

- (instancetype)initWithPlayerController:(VLCPlayerController *)playerController
{
    self = [super init];
    if (self) {
        _playerController = playerController;
        _thumbnailCache = [VLCThumbnailCache sharedCache];
        _isTimeSliderBeingDragged = NO;
    }
    return self;
}

- (void)setTimeSliderBeingDragged:(BOOL)isDragging
{
    _isTimeSliderBeingDragged = isDragging;
}

#pragma mark - VLCPlaybackProgressSliderPreviewDelegate

- (void)slider:(VLCPlaybackProgressSlider *)slider showPreviewAtPosition:(float)position mouseLocation:(NSPoint)mouseLocation
{
    if (_isTimeSliderBeingDragged) {
        return;
    }

    if (!_playerController.currentMedia || !_playerController.seekable) {
        return;
    }

    vlc_tick_t duration = _playerController.durationOfCurrentMediaItem;
    if (duration <= 0) {
        return;
    }

    vlc_tick_t previewTime = duration * position;
    NSString *timeString = [NSString stringWithDuration:duration currentTime:previewTime negative:NO];
    NSPoint sliderOrigin;

    if (@available(macOS 10.12, *)) {
        sliderOrigin = [slider.window convertRectToScreen:slider.frame].origin;
    } else {
        sliderOrigin = [slider.window convertBaseToScreen:slider.frame.origin];
    }

    NSPoint previewWindowPoint = NSMakePoint(mouseLocation.x, sliderOrigin.y + slider.frame.size.height + 10);
    [self showPreviewWindowAtPoint:previewWindowPoint timeString:timeString];

    NSString *mediaURL = _playerController.currentMedia ? _playerController.currentMedia.MRL : @"";
    [_thumbnailCache getThumbnailForMedia:mediaURL
                               atPosition:position
                         playerController:_playerController
                               completion:^(NSImage *previewImage) {
        if (previewImage) {
            [_previewWindow updateImage:previewImage timeString:timeString];
        }
    }];
}

- (void)slider:(VLCPlaybackProgressSlider *)slider updatePreviewAtPosition:(float)position mouseLocation:(NSPoint)mouseLocation
{
    if (_isTimeSliderBeingDragged) {
        return;
    }

    if (!_playerController.currentMedia || !_playerController.seekable) {
        return;
    }

    vlc_tick_t duration = _playerController.durationOfCurrentMediaItem;
    if (duration <= 0) {
        return;
    }

    vlc_tick_t previewTime = duration * position;
    NSString *timeString = [NSString stringWithDuration:duration currentTime:previewTime negative:NO];
    NSPoint sliderOrigin;

    if (@available(macOS 10.12, *)) {
        sliderOrigin = [slider.window convertRectToScreen:slider.frame].origin;
    } else {
        sliderOrigin = [slider.window convertBaseToScreen:slider.frame.origin];
    }

    NSPoint previewWindowPoint = NSMakePoint(mouseLocation.x, sliderOrigin.y + slider.frame.size.height + 10);
    NSString *mediaURL = _playerController.currentMedia ? _playerController.currentMedia.MRL : @"";

    [_previewWindow updatePosition:previewWindowPoint timeString:timeString completion:^{
        [_thumbnailCache getThumbnailForMedia:mediaURL
                                   atPosition:position
                             playerController:_playerController
                                   completion:^(NSImage *previewImage) {
            if (previewImage) {
                [_previewWindow updateImage:previewImage timeString:timeString];
            }
        }];
    }];
}

- (void)sliderHidePreview:(VLCPlaybackProgressSlider *)slider
{
    if (_previewWindow) {
        [_previewWindow hideWithAnimation];
    }
}

#pragma mark - Private Methods

- (void)showPreviewWindowAtPoint:(NSPoint)point timeString:(NSString *)timeString
{
    if (!_previewWindow) {
        _previewWindow = [[VLCProgressSliderPreviewWindow alloc] init];
    }
    
    [_previewWindow showAtPoint:point timeString:timeString];
}

@end