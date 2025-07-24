/*****************************************************************************
 * VLCThumbnailCache.m: MacOS X interface module
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

#import "VLCThumbnailCache.h"

#import "playqueue/VLCPlayerController.h"

@interface VLCThumbnailCacheRequest : NSObject
@property (nonatomic, copy) void (^callback)(NSImage *_Nullable);
@property (nonatomic, assign) NSUInteger generation;
@end

@implementation VLCThumbnailCacheRequest
@end

typedef NSMutableDictionary<NSString *, NSMutableArray<VLCThumbnailCacheRequest *> *> VLCByPositionMap;

@interface VLCThumbnailCache () {
    dispatch_queue_t _cacheQueue;
    NSUInteger _globalGeneration;
}

@property (nonatomic, strong) NSCache<NSString *, NSImage *> *imageCache;
@property (nonatomic, strong) NSMutableDictionary<NSString *, NSNumber *> *mediaGenerations;
@property (nonatomic, strong) NSMutableDictionary<NSString *, VLCByPositionMap *> *pendingRequestsByMedia;

@end

@implementation VLCThumbnailCache

+ (instancetype)sharedCache
{
    static VLCThumbnailCache *sharedInstance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[self alloc] init];
    });
    return sharedInstance;
}

- (instancetype)init
{
    self = [super init];
    if (self) {
        _imageCache = [[NSCache alloc] init];
        _imageCache.countLimit = 3000;
        _imageCache.name = @"VLC Thumbnail Cache";

        _mediaGenerations       = [[NSMutableDictionary alloc] init];
        _pendingRequestsByMedia = [[NSMutableDictionary alloc] init];

        _cacheQueue = dispatch_queue_create("com.videolan.vlc.thumbnailcache", DISPATCH_QUEUE_SERIAL);
        _globalGeneration = 0;
    }
    return self;
}

- (NSString *)positionKeyForPosition:(float)position
{
    return [NSString stringWithFormat:@"%.3f", position];
}

- (NSString *)cacheKeyForMedia:(NSString *)mediaURL position:(float)position
{
    return [NSString stringWithFormat:@"%@_%@", mediaURL, [self positionKeyForPosition:position]];
}

- (void)getThumbnailForMedia:(NSString *)mediaURL
                  atPosition:(float)position
            playerController:(VLCPlayerController *)playerController
                  completion:(void (^)(NSImage *_Nullable image))completion
{
    NSParameterAssert(completion);

    NSString *positionKey = [self positionKeyForPosition:position];
    NSString *cacheKey    = [self cacheKeyForMedia:mediaURL position:position];

    dispatch_async(_cacheQueue, ^{
        NSImage *cachedImage = [self->_imageCache objectForKey:cacheKey];
        if (cachedImage) {
            dispatch_async(dispatch_get_main_queue(), ^{ completion(cachedImage); });
            return;
        }

        NSNumber *currentGen = self->_mediaGenerations[mediaURL];
        if (!currentGen) {
            currentGen = @(++self->_globalGeneration);
            self->_mediaGenerations[mediaURL] = currentGen;
        }

        NSMutableDictionary<NSString *, NSMutableArray<VLCThumbnailCacheRequest *> *> *byPosition =
            self->_pendingRequestsByMedia[mediaURL];
        if (!byPosition) {
            byPosition = [[NSMutableDictionary alloc] init];
            self->_pendingRequestsByMedia[mediaURL] = byPosition;
        }

        NSMutableArray<VLCThumbnailCacheRequest *> *callbacks = byPosition[positionKey];
        VLCThumbnailCacheRequest *req = [[VLCThumbnailCacheRequest alloc] init];
        req.callback   = completion;
        req.generation = currentGen.unsignedIntegerValue;

        if (callbacks) {
            [callbacks addObject:req];
            return;
        }

        callbacks = [[NSMutableArray alloc] initWithObjects:req, nil];
        byPosition[positionKey] = callbacks;

        dispatch_async(dispatch_get_main_queue(), ^{
            [playerController getPreviewFrameAtPosition:position
                                             completion:^(NSImage *previewImage) {
                dispatch_async(self->_cacheQueue, ^{
                    if (previewImage) {
                        [self->_imageCache setObject:previewImage forKey:cacheKey];
                    } else {
                        NSLog(@"[VLCThumbnailCache] No image received for %@ at position %@",
                              mediaURL.lastPathComponent ?: @"unknown", positionKey);
                    }

                    NSMutableDictionary *byPos = self->_pendingRequestsByMedia[mediaURL];
                    if (!byPos) return;

                    NSMutableArray<VLCThumbnailCacheRequest *> *completionCallbacks = byPos[positionKey];
                    [byPos removeObjectForKey:positionKey];
                    if (byPos.count == 0) {
                        [self->_pendingRequestsByMedia removeObjectForKey:mediaURL];
                    }

                    NSNumber *mediaGenNum = self->_mediaGenerations[mediaURL];
                    NSUInteger currentGeneration = mediaGenNum ? mediaGenNum.unsignedIntegerValue : 0;

                    if (completionCallbacks.count > 0) {
                        dispatch_async(dispatch_get_main_queue(), ^{
                            for (VLCThumbnailCacheRequest *r in completionCallbacks) {
                                if (r.generation == currentGeneration) {
                                    r.callback(previewImage);
                                }
                            }
                        });
                    }
                });
            }];
        });
    });
}

- (void)cancelPendingRequestsForMedia:(NSString *)mediaURL
{
    dispatch_async(_cacheQueue, ^{
        self->_mediaGenerations[mediaURL] = @(++self->_globalGeneration);
        [self->_pendingRequestsByMedia removeObjectForKey:mediaURL];
    });
}

- (void)clearCache
{
    dispatch_async(_cacheQueue, ^{
        [self->_imageCache removeAllObjects];
        [self->_pendingRequestsByMedia removeAllObjects];
        [self->_mediaGenerations removeAllObjects];
    });
}

@end
