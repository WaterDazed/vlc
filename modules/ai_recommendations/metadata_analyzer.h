/*****************************************************************************
 * metadata_analyzer.h: AI-Powered Media Recommendation metadata analyzer
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

#ifndef VLC_AI_METADATA_ANALYZER_H
#define VLC_AI_METADATA_ANALYZER_H

#include <vlc_common.h>
#include <vlc_input_item.h>
#include "ai_recommendations.h"

/**
 * @file
 * This file implements metadata analysis for the AI recommendation engine
 * 
 * The metadata analyzer provides functions to extract, analyze, and compare
 * media metadata from VLC items. It supports various features including:
 * - Basic metadata extraction (title, artist, genre, etc.)
 * - Genre classification and identification
 * - Year and creator extraction
 * - Similarity calculation between media items
 * - Text feature extraction for semantic analysis
 * - Audio fingerprinting for content identification
 */

/* Metadata categories */
typedef enum {
    METADATA_CATEGORY_AUDIO = 0,
    METADATA_CATEGORY_VIDEO = 1,
    METADATA_CATEGORY_GENERAL = 2,
    METADATA_CATEGORY_COUNT
} ai_metadata_category_t;

/* Genre classification */
typedef struct {
    char *name;
    float confidence;
} ai_genre_classification_t;

/* Media metadata structure */
typedef struct {
    /* Basic metadata */
    char *title;
    char *artist;
    char *album;
    char *genre;
    char *language;
    char *description;
    int year;
    int track_number;
    int duration;
    
    /* Extended metadata */
    struct {
        char *key;
        char *value;
    } *tags;
    int tags_count;
    ai_genre_classification_t *genres;
    int genre_count;
    
    /* Technical metadata */
    int audio_channels;
    int audio_rate;
    int video_width;
    int video_height;
    float video_fps;
    char *video_codec;
    char *audio_codec;
    
    /* Computed metadata */
    float energy_level;
    float speech_ratio;
    float music_ratio;
    char *dominant_color;
    float brightness;
    float contrast;
} ai_media_metadata_t;

/**
 * Extract metadata from a media item
 *
 * This function extracts all available metadata from a media item,
 * including basic fields (title, artist, genre), technical information,
 * and computes derived metadata like energy level and dominant color.
 *
 * @param item The media item to analyze
 * @return Extracted metadata structure or NULL on error
 */
ai_media_metadata_t *ExtractMediaMetadata(input_item_t *item);

/**
 * Free a media metadata structure
 *
 * Properly releases all resources associated with a metadata structure,
 * including strings, genre classifications, and dictionary tags.
 *
 * @param metadata The metadata structure to free
 */
void FreeMediaMetadata(ai_media_metadata_t *metadata);

/**
 * Classify genre from metadata
 *
 * Analyzes metadata to identify genres with confidence scores.
 * Uses the primary genre from metadata and supplemental information
 * from tags to create a comprehensive genre classification.
 *
 * @param metadata The metadata to analyze
 * @param max_genres Maximum number of genres to identify (limited to MAX_GENRE_COUNT)
 * @return Array of genre classifications or NULL on error, caller must free with free()
 */
ai_genre_classification_t *ClassifyGenreFromMetadata(ai_media_metadata_t *metadata, int max_genres);

/**
 * Extract year from media metadata or filename
 *
 * Attempts to find a valid year from metadata or by parsing the filename.
 * Uses regular expressions to extract years from filenames matching common
 * patterns like [2023] or (2023). Years are validated to be reasonable
 * (between 1900-2100).
 *
 * @param metadata The metadata to analyze
 * @param filepath The file path (used if metadata doesn't contain year)
 * @return Extracted year or 0 if unknown
 */
int ExtractYearFromMetadata(ai_media_metadata_t *metadata, const char *filepath);

/**
 * Extract artist/creator from metadata
 *
 * Extracts the creator information from metadata, checking multiple fields
 * in a priority order (artist, director, producer, etc.) to ensure the most
 * relevant creator is identified. Particularly useful for different media types
 * which store creator information in different fields.
 *
 * @param metadata The metadata to analyze
 * @return Artist/creator string or NULL if unknown, caller must free with free()
 */
char *ExtractCreatorFromMetadata(ai_media_metadata_t *metadata);

/**
 * Compare two metadata structures and calculate similarity
 *
 * Calculates a weighted similarity score between two metadata structures by
 * comparing multiple factors including genre, creator, year, energy level,
 * and duration. Each factor contributes differently to the final score:
 * - Genre similarity is weighted most heavily (3x)
 * - Creator/artist similarity is also significant (2x)
 * - Album, year, energy level, and duration similarity contribute additionally
 *
 * @param metadata1 First metadata structure
 * @param metadata2 Second metadata structure
 * @return Similarity score between 0.0 and 1.0
 */
float CalculateMetadataSimilarity(ai_media_metadata_t *metadata1, ai_media_metadata_t *metadata2);

/**
 * Calculate TF-IDF vectors for text metadata
 *
 * Converts text into a vector representation using term frequency-inverse document
 * frequency (TF-IDF) weighting. This enables semantic comparison of text content.
 * The implementation tokenizes text, counts term frequencies, normalizes the vector,
 * and uses a hashing function to map terms to vector positions for efficiency.
 *
 * @param text The text to analyze
 * @param vector_size The size of the vector to create
 * @return TF-IDF vector or NULL on error, caller must free with free()
 */
float *CalculateTfIdfVector(const char *text, size_t vector_size);

/**
 * Extract a fingerprint from audio metadata
 *
 * Creates a unique identifier string for audio content based on metadata.
 * This simplified implementation generates a deterministic fingerprint from 
 * available metadata (title, artist, genre, year, duration) to enable content
 * identification without analyzing the actual audio signal.
 *
 * In a production implementation, this would analyze audio samples to create
 * a perceptual hash that identifies the content regardless of format or quality.
 *
 * @param metadata The metadata to analyze
 * @return Fingerprint string or NULL on error, caller must free with free()
 */
char *ExtractAudioFingerprint(ai_media_metadata_t *metadata);

/**
 * Normalize metadata fields for consistent comparison
 *
 * Prepares metadata for comparison by:
 * - Trimming whitespace from string fields
 * - Clamping float values to valid ranges (0.0-1.0)
 * - Standardizing formats for consistent matching
 *
 * This ensures that minor differences in metadata representation don't
 * affect similarity calculations or classification results.
 *
 * @param metadata The metadata to normalize
 */
void NormalizeMetadata(ai_media_metadata_t *metadata);

#endif /* VLC_AI_METADATA_ANALYZER_H */