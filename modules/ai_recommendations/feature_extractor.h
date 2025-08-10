/*****************************************************************************
 * feature_extractor.h: AI-Powered Media Recommendation feature extractor
 *****************************************************************************
 * Copyright (C) 2023-2025 VLC authors and VideoLAN
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

#ifndef VLC_AI_FEATURE_EXTRACTOR_H
#define VLC_AI_FEATURE_EXTRACTOR_H

#include <vlc_common.h>
#include <vlc_input_item.h>
#include "ai_recommendations.h"

/**
 * @file
 * This file implements feature extraction for the AI recommendation engine
 */

/* Feature extraction modes */
#define FEATURE_EXTRACTION_LIGHT 0   /* Metadata only */
#define FEATURE_EXTRACTION_STANDARD 1 /* Metadata + basic content */
#define FEATURE_EXTRACTION_DEEP 2     /* Comprehensive analysis */

/* Feature types */
typedef enum {
    FEATURE_TYPE_METADATA = 0,
    FEATURE_TYPE_AUDIO = 1,
    FEATURE_TYPE_VIDEO = 2,
    FEATURE_TYPE_SUBTITLE = 3,
    FEATURE_TYPE_COMBINED = 4,
    FEATURE_TYPE_COUNT
} ai_feature_type_t;

/* Feature vector types */
typedef enum {
    FEATURE_VECTOR_GENRE = 0,
    FEATURE_VECTOR_MOOD = 1,
    FEATURE_VECTOR_TEMPO = 2,
    FEATURE_VECTOR_TOPIC = 3,
    FEATURE_VECTOR_VISUAL = 4,
    FEATURE_VECTOR_SEMANTIC = 5,
    FEATURE_VECTOR_COUNT
} ai_feature_vector_type_t;

/* Feature extraction options */
typedef struct {
    int extraction_mode;          /* Processing depth */
    bool analyze_subtitles;       /* Whether to analyze subtitle content */
    int max_memory_mb;            /* Maximum memory usage */
    vlc_tick_t max_processing_time; /* Maximum processing time per item */
} ai_feature_extraction_options_t;

/**
 * Extract features from a media item
 *
 * @param recommender The recommender module instance
 * @param item The media item to analyze
 * @return Extracted features or NULL on error
 */
ai_media_feature_t *ExtractMediaFeatures(ai_recommender_t *recommender, input_item_t *item);

/**
 * Extract metadata features from a media item
 *
 * @param item The media item to analyze
 * @param options Feature extraction options
 * @return Extracted metadata features or NULL on error
 */
ai_media_feature_t *ExtractMetadataFeatures(input_item_t *item, ai_feature_extraction_options_t *options);

/**
 * Extract audio features from a media item
 *
 * @param item The media item to analyze
 * @param options Feature extraction options
 * @return Extracted audio features or NULL on error
 */
ai_media_feature_t *ExtractAudioFeatures(input_item_t *item, ai_feature_extraction_options_t *options);

/**
 * Extract video features from a media item
 *
 * @param item The media item to analyze
 * @param options Feature extraction options
 * @return Extracted video features or NULL on error
 */
ai_media_feature_t *ExtractVideoFeatures(input_item_t *item, ai_feature_extraction_options_t *options);

/**
 * Extract subtitle features from a media item
 *
 * @param item The media item to analyze
 * @param options Feature extraction options
 * @return Extracted subtitle features or NULL on error
 */
ai_media_feature_t *ExtractSubtitleFeatures(input_item_t *item, ai_feature_extraction_options_t *options);

/**
 * Combine multiple feature sets into a single feature
 *
 * @param features Array of feature sets to combine
 * @param count Number of feature sets in the array
 * @return Combined feature set or NULL on error
 */
ai_media_feature_t *CombineFeatures(ai_media_feature_t **features, int count);

/**
 * Free a media feature structure
 *
 * @param feature The feature to free
 */
void FreeMediaFeature(ai_media_feature_t *feature);

/**
 * Find subtitle file for a media item
 *
 * @param item The media item to find subtitles for
 * @return Path to subtitle file or NULL if not found, caller must free with free()
 */
char *FindSubtitleFile(input_item_t *item);

/**
 * Calculate similarity between two feature sets
 *
 * @param feature1 First feature set
 * @param feature2 Second feature set
 * @param weights Feature weights to apply (can be NULL for equal weights)
 * @return Similarity score between 0.0 and 1.0
 */
float CalculateFeatureSimilarity(ai_media_feature_t *feature1, 
                                ai_media_feature_t *feature2,
                                float *weights);

/**
 * Detect genre from audio features
 *
 * @param audio_data Audio data buffer
 * @param audio_size Size of audio data buffer
 * @return Detected genre or NULL if unknown, caller must free with free()
 */
char *DetectGenreFromAudio(uint8_t *audio_data, size_t audio_size);

/**
 * Extract themes from subtitle text
 *
 * @param subtitle_text Subtitle text content
 * @param max_themes Maximum number of themes to extract
 * @return Array of theme strings or NULL on error, caller must free each string and the array
 */
char **ExtractThemesFromSubtitles(const char *subtitle_text, int max_themes);

/**
 * Parse subtitle file into plain text
 *
 * @param subtitle_path Path to subtitle file
 * @return Plain text content or NULL on error, caller must free with free()
 */
char *ParseSubtitleToText(const char *subtitle_path);

#endif /* VLC_AI_FEATURE_EXTRACTOR_H */