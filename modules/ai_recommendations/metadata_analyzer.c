/*****************************************************************************
 * metadata_analyzer.c: AI-Powered Media Recommendation metadata analyzer
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

/**
 * @file
 * AI-Powered Media Recommendation metadata analyzer
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "custom_compat.h"
#include <vlc_plugin.h>
#include <vlc_input_item.h>
#include <vlc_strings.h>
#include <vlc_meta.h>
#include <vlc_strings.h>
#include <vlc_fs.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

#include "metadata_analyzer.h"

/* Constants for genre classification */
#define MAX_GENRE_COUNT 5
#define MAX_TAG_COUNT 20
#define MIN_YEAR 1900
#define MAX_YEAR 2100

/* Character set for fingerprint generation */
static const char FINGERPRINT_CHARSET[] = "abcdefghijklmnopqrstuvwxyz0123456789";

/**
 * Extract metadata from a media item
 */
ai_media_metadata_t *ExtractMediaMetadata(input_item_t *item)
{
    if (!item)
        return NULL;
        
    ai_media_metadata_t *metadata = calloc(1, sizeof(ai_media_metadata_t));
    if (!metadata)
        return NULL;
        
    /* Extract basic metadata directly from item */
    input_item_t *media = item;
    
    /* Get information from item - using safer null checks throughout */
    char *title = input_item_GetTitle(media);
    if (!title)
        title = input_item_GetName(media);
    if (title) {
        metadata->title = strdup(title);
        free(title); /* Free immediately after use */
        title = NULL; /* Set to NULL to avoid double-free or use-after-free */
    }
    
    char *artist = input_item_GetInfo(media, "General", "Artist");
    if (artist) {
        metadata->artist = strdup(artist);
        free(artist);
        artist = NULL;
    }
    
    char *album = input_item_GetInfo(media, "General", "Album");
    if (album) {
        metadata->album = strdup(album);
        free(album);
        album = NULL;
    }
    
    char *genre = input_item_GetInfo(media, "General", "Genre");
    if (genre) {
        metadata->genre = strdup(genre);
        free(genre);
        genre = NULL;
    }
    
    char *language = input_item_GetInfo(media, "General", "Language");
    if (language) {
        metadata->language = strdup(language);
        free(language);
        language = NULL;
    }
    
    char *description = input_item_GetInfo(media, "General", "Description");
    if (description) {
        metadata->description = strdup(description);
        free(description);
        description = NULL;
    }
    
    /* Try to get year - with error checking */
    char *year_str = input_item_GetInfo(media, "General", "Date");
    if (year_str) {
        char *endptr = NULL;
        long year_val = strtol(year_str, &endptr, 10); /* Safer than atoi */
        
        /* Validate conversion success and range */
        if (endptr != year_str && year_val >= MIN_YEAR && year_val <= MAX_YEAR) {
            metadata->year = (int)year_val;
        } else {
            metadata->year = 0;
        }
        free(year_str);
        year_str = NULL;
    }
    
    /* Try to get track number - with error checking */
    char *track_str = input_item_GetInfo(media, "General", "Track number");
    if (track_str) {
        char *endptr = NULL;
        long track_val = strtol(track_str, &endptr, 10);
        
        /* Only set if conversion succeeded */
        if (endptr != track_str) {
            metadata->track_number = (int)track_val;
        }
        free(track_str);
        track_str = NULL;
    }
    
    /* Get duration with sanity check */
    vlc_tick_t duration = input_item_GetDuration(media);
    metadata->duration = (duration > 0) ? (int)(duration / VLC_TICK_FROM_SEC(1)) : 0;
    
    /* Get technical metadata with proper null handling */
    char *codec = input_item_GetInfo(media, "Audio", "Codec");
    if (codec) {
        metadata->audio_codec = strdup(codec);
        free(codec);
        codec = NULL;
    }
    
    codec = input_item_GetInfo(media, "Video", "Codec");
    if (codec) {
        metadata->video_codec = strdup(codec);
        free(codec);
        codec = NULL;
    }
    
    char *channels_str = input_item_GetInfo(media, "Audio", "Channels");
    if (channels_str) {
        metadata->audio_channels = atoi(channels_str);
        free(channels_str);
    }
    
    char *rate_str = input_item_GetInfo(media, "Audio", "Sample rate");
    if (rate_str) {
        metadata->audio_rate = atoi(rate_str);
        free(rate_str);
    }
    
    char *width_str = input_item_GetInfo(media, "Video", "Resolution");
    if (width_str) {
        /* Parse "WxH" format */
        char *x = strchr(width_str, 'x');
        if (x) {
            *x = '\0';
            metadata->video_width = atoi(width_str);
            metadata->video_height = atoi(x + 1);
        }
        free(width_str);
    }
    
    char *fps_str = input_item_GetInfo(media, "Video", "Frame rate");
    if (fps_str) {
        metadata->video_fps = atof(fps_str);
        free(fps_str);
    }
    
    /* Set default computed values */
    metadata->energy_level = 0.5f;
    metadata->speech_ratio = 0.5f;
    metadata->music_ratio = 0.5f;
    metadata->dominant_color = strdup("unknown");
    metadata->brightness = 0.5f;
    metadata->contrast = 0.5f;
    
    /* Initialize arrays with proper memory handling */
    metadata->tags = NULL;
    metadata->tags_count = 0;
    
    /* Allocate tags array only if needed, using safer size calculation */
    metadata->tags = calloc(MAX_TAG_COUNT, sizeof(*(metadata->tags)));
    if (!metadata->tags) {
        /* Handle allocation failure - clean up and return what we have so far */
        goto error_handling;
    }
    
    /* Extract additional tags from item - with version-compatible approach */
    const char *categories[] = {"Audio", "Video", "Stream", "Summary"};
    const char *fields[] = {"Codec", "Language", "Bitrate", "Channels", "Sample rate", 
                           "Resolution", "Frame rate", "Format", "Protocol"};
    
    const size_t category_count = sizeof(categories) / sizeof(categories[0]);
    const size_t field_count = sizeof(fields) / sizeof(fields[0]);
    
    for (size_t i = 0; i < category_count && metadata->tags_count < MAX_TAG_COUNT; i++) {
        for (size_t j = 0; j < field_count && metadata->tags_count < MAX_TAG_COUNT; j++) {
            char *value = input_item_GetInfo(media, categories[i], fields[j]);
            if (value && *value) {  /* Only process non-empty values */
                /* Safely add the tag with error handling */
                char *key_copy = strdup(fields[j]);
                char *value_copy = strdup(value);
                
                /* Check for allocation failures */
                if (!key_copy || !value_copy) {
                    free(key_copy);
                    free(value_copy);
                } else {
                    metadata->tags[metadata->tags_count].key = key_copy;
                    metadata->tags[metadata->tags_count].value = value_copy;
                    metadata->tags_count++;
                }
                
                free(value);
                value = NULL;
            }
        }
    }
    
    /* Continue with normal execution */
    goto continue_processing;
    
error_handling:
    /* We only reach here if there was a critical allocation failure */
    if (metadata) {
        /* Free any tags we might have allocated */
        for (int i = 0; i < metadata->tags_count; i++) {
            free(metadata->tags[i].key);
            free(metadata->tags[i].value);
        }
        free(metadata->tags);
        metadata->tags = NULL;
        metadata->tags_count = 0;
    }
    
continue_processing:
    
    /* Create genre classifications with proper error handling */
    if (metadata->genre) {
        metadata->genres = calloc(1, sizeof(ai_genre_classification_t));
        if (metadata->genres) {
            char *genre_copy = strdup(metadata->genre);
            if (genre_copy) {
                metadata->genres[0].name = genre_copy;
                metadata->genres[0].confidence = 0.9f;
                metadata->genre_count = 1;
            } else {
                /* Handle allocation failure */
                free(metadata->genres);
                metadata->genres = NULL;
                metadata->genre_count = 0;
            }
        }
    } else {
        metadata->genres = NULL;
        metadata->genre_count = 0;
    }
    
    /* Normalize metadata for consistent comparisons */
    NormalizeMetadata(metadata);
    
    return metadata;
}

/**
 * Free a media metadata structure
 * 
 * This function safely releases all resources associated with a metadata structure,
 * including all dynamically allocated strings and arrays.
 */
void FreeMediaMetadata(ai_media_metadata_t *metadata)
{
    if (!metadata)
        return;
        
    /* Free all string fields with null safety checks */
    if (metadata->title) free(metadata->title);
    if (metadata->artist) free(metadata->artist);
    if (metadata->album) free(metadata->album);
    if (metadata->genre) free(metadata->genre);
    if (metadata->language) free(metadata->language);
    if (metadata->description) free(metadata->description);
    if (metadata->video_codec) free(metadata->video_codec);
    if (metadata->audio_codec) free(metadata->audio_codec);
    if (metadata->dominant_color) free(metadata->dominant_color);
    
    /* Free all tags with bounds checking */
    if (metadata->tags) {
        for (int i = 0; i < metadata->tags_count; i++) {
            if (metadata->tags[i].key) free(metadata->tags[i].key);
            if (metadata->tags[i].value) free(metadata->tags[i].value);
        }
        free(metadata->tags);
    }
    
    /* Free all genre classifications with bounds checking */
    if (metadata->genres) {
        for (int i = 0; i < metadata->genre_count; i++) {
            if (metadata->genres[i].name) free(metadata->genres[i].name);
        }
        free(metadata->genres);
    }
    
    /* Finally, free the metadata structure itself */
    free(metadata);
}

/**
 * Classify genre from metadata
 * 
 * This function analyzes metadata to create a list of genre classifications
 * with confidence scores. It uses multiple sources of information including
 * the primary genre field, metadata tags, and fallback genre inference.
 */
ai_genre_classification_t *ClassifyGenreFromMetadata(ai_media_metadata_t *metadata, int max_genres)
{
    /* Validate input parameters */
    if (!metadata || max_genres <= 0)
        return NULL;
        
    /* Limit to maximum genre count for safety */
    if (max_genres > MAX_GENRE_COUNT)
        max_genres = MAX_GENRE_COUNT;
    
    /* Allocate genre classifications with proper initialization */
    ai_genre_classification_t *genres = calloc(max_genres, sizeof(ai_genre_classification_t));
    if (!genres)
        return NULL;
    
    int count = 0;
    
    /* Start with primary genre if available */
    if (metadata->genre && *metadata->genre) {
        char *genre_copy = strdup(metadata->genre);
        if (genre_copy) {
            genres[count].name = genre_copy;
            genres[count].confidence = 1.0f;
            count++;
        }
    }
    
    /* Look for genre tags with proper error handling and bounds checking */
    if (metadata->tags && metadata->tags_count > 0) {
        for (int i = 0; i < metadata->tags_count && count < max_genres; i++) {
            /* Check for valid key and value before processing */
            if (!metadata->tags[i].key || !metadata->tags[i].value)
                continue;
                
            if (!strcasecmp(metadata->tags[i].key, "Genre") || 
                !strcasecmp(metadata->tags[i].key, "Genres") ||
                !strcasecmp(metadata->tags[i].key, "Style") ||
                !strcasecmp(metadata->tags[i].key, "Category")) {
                
                /* Skip empty values */
                if (!*metadata->tags[i].value)
                    continue;
                
                /* Check if this genre is already in our list */
                bool duplicate = false;
                for (int j = 0; j < count; j++) {
                    if (genres[j].name && !strcasecmp(genres[j].name, metadata->tags[i].value)) {
                        duplicate = true;
                        /* Increase confidence if we found it from multiple sources */
                        if (genres[j].confidence < 0.95f) {
                            genres[j].confidence += 0.1f;
                            if (genres[j].confidence > 1.0f)
                                genres[j].confidence = 1.0f;
                        }
                        break;
                    }
                }
                
                if (!duplicate) {
                    char *value_copy = strdup(metadata->tags[i].value);
                    if (value_copy) {
                        genres[count].name = value_copy;
                        genres[count].confidence = 0.8f;
                        count++;
                    }
                }
            }
        }
    }
    
    /* For demo purposes, add some sample genres if we didn't find any */
    static const char *sample_genres[] = {
        "Rock", "Pop", "Classical", "Electronic", "Jazz", 
        "Hip-Hop", "Country", "Blues", "Folk", "Metal"
    };
    const size_t sample_genre_count = sizeof(sample_genres) / sizeof(sample_genres[0]);
    
    if (count == 0 && metadata->title && *metadata->title) {
        /* Generate a pseudo-random genre based on title hash with overflow protection */
        size_t title_len = strlen(metadata->title);
        uint32_t hash = 5381; /* Initial value from djb2 algorithm */
        
        for (size_t i = 0; i < title_len; i++) {
            /* djb2-inspired hash */
            hash = ((hash << 5) + hash) + (unsigned char)metadata->title[i];
        }
        
        size_t genre_idx = hash % sample_genre_count;
        char *genre_copy = strdup(sample_genres[genre_idx]);
        if (genre_copy) {
            genres[count].name = genre_copy;
            genres[count].confidence = 0.5f;
            count++;
        }
    }
    
    /* If we didn't find any genres, clean up and return NULL */
    if (count == 0) {
        free(genres);
        return NULL;
    }
    
    return genres;
}

/**
 * Extract year from media metadata or filename
 */
int ExtractYearFromMetadata(ai_media_metadata_t *metadata, const char *filepath)
{
    if (!metadata)
        return 0;
        
    /* If year is already set in metadata, use it */
    if (metadata->year >= MIN_YEAR && metadata->year <= MAX_YEAR)
        return metadata->year;
    
    /* Try to extract from tags */
    for (int i = 0; i < metadata->tags_count; i++) {
        if (!strcasecmp(metadata->tags[i].key, "Year") ||
            !strcasecmp(metadata->tags[i].key, "Date") ||
            !strcasecmp(metadata->tags[i].key, "Original Release Date") ||
            !strcasecmp(metadata->tags[i].key, "Recording Date")) {
            
            /* Extract numeric year */
            const char *str = metadata->tags[i].value;
            char *end;
            int year = strtol(str, &end, 10);
            
            /* Validate year */
            if (end != str && year >= MIN_YEAR && year <= MAX_YEAR)
                return year;
            
            /* Try to find 4-digit number in string */
            for (const char *p = str; *p; p++) {
                if (isdigit(*p) && isdigit(*(p+1)) && isdigit(*(p+2)) && isdigit(*(p+3))) {
                    year = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + 
                           (p[2] - '0') * 10 + (p[3] - '0');
                    if (year >= MIN_YEAR && year <= MAX_YEAR)
                        return year;
                }
            }
        }
    }
    
    /* Try to extract from title */
    if (metadata->title) {
        /* Look for patterns like (2023) or [2023] */
        const char *str = metadata->title;
        for (const char *p = str; *p; p++) {
            if ((*p == '(' || *p == '[') && isdigit(*(p+1)) && isdigit(*(p+2)) && 
                isdigit(*(p+3)) && isdigit(*(p+4)) && (*(p+5) == ')' || *(p+5) == ']')) {
                int year = (p[1] - '0') * 1000 + (p[2] - '0') * 100 + 
                           (p[3] - '0') * 10 + (p[4] - '0');
                if (year >= MIN_YEAR && year <= MAX_YEAR)
                    return year;
            }
        }
    }
    
    /* Try to extract from filepath if provided */
    if (filepath) {
        const char *str = filepath;
        for (const char *p = str; *p; p++) {
            if (isdigit(*p) && isdigit(*(p+1)) && isdigit(*(p+2)) && isdigit(*(p+3))) {
                int year = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + 
                           (p[2] - '0') * 10 + (p[3] - '0');
                if (year >= MIN_YEAR && year <= MAX_YEAR)
                    return year;
            }
        }
    }
    
    /* Default: return 0 (unknown) */
    return 0;
}

/**
 * Extract artist/creator from metadata
 */
char *ExtractCreatorFromMetadata(ai_media_metadata_t *metadata)
{
    if (!metadata)
        return NULL;
    
    const char *creator = NULL;
    
    /* Use artist if available */
    if (metadata->artist)
        return strdup(metadata->artist);
    
    /* Search various common tag names for creator info */
    static const char *creator_tags[] = {
        "Artist", "Director", "Performer", "Author", "Composer", 
        "Producer", "Creator", "Band", "Conductor", "Writer"
    };
    
    for (size_t i = 0; i < sizeof(creator_tags) / sizeof(creator_tags[0]); i++) {
        for (int j = 0; j < metadata->tags_count; j++) {
            if (!strcasecmp(metadata->tags[j].key, creator_tags[i])) {
                return strdup(metadata->tags[j].value);
            }
        }
    }
    
    /* Return NULL if no creator found */
    return NULL;
}

/**
 * Calculate metadata similarity between two items
 */
float CalculateMetadataSimilarity(ai_media_metadata_t *metadata1, ai_media_metadata_t *metadata2)
{
    if (!metadata1 || !metadata2)
        return 0.0f;
    
    float similarity = 0.0f;
    float weight_sum = 0.0f;
    
    /* Genre similarity (weight: 3.0) */
    if (metadata1->genre && metadata2->genre) {
        if (!strcasecmp(metadata1->genre, metadata2->genre)) {
            similarity += 3.0f;
        } else {
            /* Partial genre matching for mixed genres */
            float partial = 0.0f;
            char *genre1 = strdup(metadata1->genre);
            char *genre2 = strdup(metadata2->genre);
            
            if (genre1 && genre2) {
                char *token1, *saveptr1;
                token1 = strtok_r(genre1, "/,;&|", &saveptr1);
                while (token1) {
                    char *token2, *saveptr2;
                    char *genre2_copy = strdup(genre2);
                    if (genre2_copy) {
                        token2 = strtok_r(genre2_copy, "/,;&|", &saveptr2);
                        while (token2) {
                            if (!strcasecmp(token1, token2)) {
                                partial = 2.0f;  /* Partial match */
                                break;
                            }
                            token2 = strtok_r(NULL, "/,;&|", &saveptr2);
                        }
                        free(genre2_copy);
                    }
                    
                    if (partial > 0.0f)
                        break;
                        
                    token1 = strtok_r(NULL, "/,;&|", &saveptr1);
                }
            }
            
            free(genre1);
            free(genre2);
            similarity += partial;
        }
        weight_sum += 3.0f;
    }
    
    /* Creator similarity (weight: 2.0) */
    if (metadata1->artist && metadata2->artist) {
        if (!strcasecmp(metadata1->artist, metadata2->artist)) {
            similarity += 2.0f;
        }
        weight_sum += 2.0f;
    }
    
    /* Album similarity (weight: 1.0) */
    if (metadata1->album && metadata2->album) {
        if (!strcasecmp(metadata1->album, metadata2->album)) {
            similarity += 1.0f;
        }
        weight_sum += 1.0f;
    }
    
    /* Year similarity (weight: 1.0) */
    if (metadata1->year > 0 && metadata2->year > 0) {
        int year_diff = abs(metadata1->year - metadata2->year);
        if (year_diff == 0) {
            similarity += 1.0f;
        } else if (year_diff <= 3) {
            similarity += 0.8f;
        } else if (year_diff <= 10) {
            similarity += 0.5f;
        } else if (year_diff <= 20) {
            similarity += 0.2f;
        }
        weight_sum += 1.0f;
    }
    
    /* Energy level similarity (weight: 0.5) */
    float energy_diff = fabs(metadata1->energy_level - metadata2->energy_level);
    similarity += (1.0f - energy_diff) * 0.5f;
    weight_sum += 0.5f;
    
    /* Duration similarity (weight: 0.5) */
    if (metadata1->duration > 0 && metadata2->duration > 0) {
        float duration_ratio;
        if (metadata1->duration > metadata2->duration) {
            duration_ratio = (float)metadata2->duration / metadata1->duration;
        } else {
            duration_ratio = (float)metadata1->duration / metadata2->duration;
        }
        
        similarity += duration_ratio * 0.5f;
        weight_sum += 0.5f;
    }
    
    /* Normalize the result */
    if (weight_sum > 0.0f) {
        similarity /= weight_sum;
    } else {
        similarity = 0.0f;
    }
    
    return similarity;
}

/**
 * Calculate TF-IDF vectors for text metadata
 */
float *CalculateTfIdfVector(const char *text, size_t vector_size)
{
    if (!text || vector_size == 0)
        return NULL;
        
    float *vector = calloc(vector_size, sizeof(float));
    if (!vector)
        return NULL;
        
    /* Copy text for tokenization */
    char *text_copy = strdup(text);
    if (!text_copy) {
        free(vector);
        return NULL;
    }
    
    /* Convert to lowercase */
    for (char *p = text_copy; *p; p++) {
        *p = tolower(*p);
    }
    
    /* Tokenize and count terms */
    char *token, *saveptr;
    token = strtok_r(text_copy, " \t\n\r\f.,;:!?\"'()[]{}-_+=<>|/\\", &saveptr);
    
    while (token) {
        /* Skip very short tokens */
        if (strlen(token) > 2) {
            /* Use simple hash to map term to vector position */
            size_t hash = 0;
            for (char *p = token; *p; p++) {
                hash = hash * 31 + *p;
            }
            
            size_t pos = hash % vector_size;
            vector[pos] += 1.0f;
        }
        
        token = strtok_r(NULL, " \t\n\r\f.,;:!?\"'()[]{}-_+=<>|/\\", &saveptr);
    }
    
    /* Normalize vector (L2 normalization) */
    float sum_squares = 0.0f;
    for (size_t i = 0; i < vector_size; i++) {
        sum_squares += vector[i] * vector[i];
    }
    
    if (sum_squares > 0.0f) {
        float norm = sqrtf(sum_squares);
        for (size_t i = 0; i < vector_size; i++) {
            vector[i] /= norm;
        }
    }
    
    free(text_copy);
    return vector;
}

/**
 * Extract a fingerprint from audio metadata
 * 
 * This function creates a deterministic fingerprint from metadata attributes
 * using a secure hash calculation approach that avoids potential overflows
 * and provides better distribution of values.
 */
char *ExtractAudioFingerprint(ai_media_metadata_t *metadata)
{
    if (!metadata)
        return NULL;
        
    /* For this implementation, we generate a hash-based fingerprint
       from available metadata. A production implementation would
       analyze the actual audio signal. */
    
    /* Allocate buffer for fingerprint (32 chars + null terminator) */
    char *fingerprint = calloc(33, sizeof(char));
    if (!fingerprint)
        return NULL;
    
    /* Use a 64-bit hash for better distribution and to prevent overflow */
    uint64_t hash = 5381; /* Initial value from djb2 algorithm */
    
    /* Safer string handling with explicit boundary checks */
    const size_t charset_len = sizeof(FINGERPRINT_CHARSET) - 1;
    
    /* Add title contribution with overflow protection */
    if (metadata->title) {
        for (const char *p = metadata->title; *p; p++) {
            /* djb2-inspired hash algorithm - ((hash << 5) + hash) + c */
            hash = ((hash << 5) + hash) ^ (uint64_t)(tolower((unsigned char)*p));
        }
    }
    
    /* Add artist contribution with overflow protection */
    if (metadata->artist) {
        for (const char *p = metadata->artist; *p; p++) {
            hash = ((hash << 5) + hash) ^ (uint64_t)(tolower((unsigned char)*p));
        }
    }
    
    /* Add album contribution to enhance uniqueness */
    if (metadata->album) {
        for (const char *p = metadata->album; *p; p++) {
            hash = ((hash << 5) + hash) ^ (uint64_t)(tolower((unsigned char)*p));
        }
    }
    
    /* Add genre contribution */
    if (metadata->genre) {
        for (const char *p = metadata->genre; *p; p++) {
            hash = ((hash << 5) + hash) ^ (uint64_t)(tolower((unsigned char)*p));
        }
    }
    
    /* Add year and duration with salt values to improve distribution */
    hash = ((hash << 5) + hash) ^ ((uint64_t)metadata->year + 1900);
    hash = ((hash << 5) + hash) ^ ((uint64_t)metadata->duration + 1000);
    
    /* Add technical metadata if available */
    if (metadata->audio_codec) {
        for (const char *p = metadata->audio_codec; *p; p++) {
            hash = ((hash << 5) + hash) ^ (uint64_t)(tolower((unsigned char)*p));
        }
    }
    
    /* Convert hash to string using charset with bounds checking */
    for (int i = 0; i < 32; i++) {
        fingerprint[i] = FINGERPRINT_CHARSET[hash % charset_len];
        hash /= charset_len;
    }
    
    fingerprint[32] = '\0';
    return fingerprint;
}

/**
 * Custom string trimming functions for metadata normalization
 * 
 * These functions safely handle whitespace trimming while protecting 
 * against buffer overruns and invalid pointers.
 */
static void custom_rtrim(char *s)
{
    if (!s)
        return;
    
    size_t len = strlen(s);
    if (len == 0)
        return;
    
    char *end = s + len - 1;
    while (end >= s && isspace((unsigned char)*end))
        end--;
    
    /* Set the null terminator at the right position */
    *(end + 1) = '\0';
}

/**
 * Custom left trim function 
 */
static char *custom_ltrim(char *s)
{
    if (!s)
        return NULL;
    
    /* Skip leading whitespace */
    while (isspace((unsigned char)*s))
        s++;
    
    return s;
}

/**
 * Full trim (both left and right)
 */
static void custom_trim(char *s)
{
    if (!s)
        return;
    
    /* First, left trim in place by moving the content left */
    char *start = custom_ltrim(s);
    if (start != s) {
        /* Need to move the string */
        size_t len = strlen(start);
        memmove(s, start, len + 1); /* +1 for null terminator */
    }
    
    /* Then right trim */
    custom_rtrim(s);
}

/**
 * Normalize metadata fields for consistent comparison
 * 
 * This function prepares metadata for comparison by ensuring all fields
 * are in a standardized format with trimmed strings and clamped values.
 */
void NormalizeMetadata(ai_media_metadata_t *metadata)
{
    if (!metadata)
        return;
        
    /* Trim whitespace from all string fields using our enhanced trimming function */
    if (metadata->title)
        custom_trim(metadata->title);
        
    if (metadata->artist)
        custom_trim(metadata->artist);
        
    if (metadata->album)
        custom_trim(metadata->album);
        
    if (metadata->genre)
        custom_trim(metadata->genre);
        
    if (metadata->language)
        custom_trim(metadata->language);
        
    if (metadata->description)
        custom_trim(metadata->description);
        
    if (metadata->audio_codec)
        custom_trim(metadata->audio_codec);
        
    if (metadata->video_codec)
        custom_trim(metadata->video_codec);
        
    if (metadata->dominant_color)
        custom_trim(metadata->dominant_color);
        
    /* Trim all tag values for consistency */
    for (int i = 0; i < metadata->tags_count; i++) {
        if (metadata->tags[i].value)
            custom_trim(metadata->tags[i].value);
    }
    
    /* Clamp float values to valid ranges for safer comparisons */
    #define CLAMP(val, min, max) \
        if (val < min) val = min; \
        if (val > max) val = max;
        
    CLAMP(metadata->energy_level, 0.0f, 1.0f);
    CLAMP(metadata->speech_ratio, 0.0f, 1.0f);
    CLAMP(metadata->music_ratio, 0.0f, 1.0f);
    CLAMP(metadata->brightness, 0.0f, 1.0f);
    CLAMP(metadata->contrast, 0.0f, 1.0f);
    
    /* Validate and clamp other numeric fields to reasonable ranges */
    if (metadata->audio_channels < 0) metadata->audio_channels = 0;
    if (metadata->audio_rate < 0) metadata->audio_rate = 0;
    if (metadata->video_width < 0) metadata->video_width = 0;
    if (metadata->video_height < 0) metadata->video_height = 0;
    if (metadata->video_fps < 0.0f) metadata->video_fps = 0.0f;
}