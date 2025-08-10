/*****************************************************************************
 * feature_extractor.c: AI-Powered Media Recommendation feature extractor
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
 * AI-Powered Media Recommendation feature extractor
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "custom_compat.h"
#include <vlc_plugin.h>
#include <vlc_input_item.h>
#include <vlc_meta.h>
#include <vlc_url.h>
#include <vlc_fs.h>
#include <vlc_charset.h>

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>

#include "feature_extractor.h"
#include "metadata_analyzer.h"
#include "ai_recommendations.h"

/* Constants for feature vector sizes */
#define FEATURE_VECTOR_SIZE 128
#define MAX_THEMES 10
#define MAX_AUDIO_SAMPLE_SIZE (1024 * 1024)  /* 1MB */
#define MAX_SUBTITLE_SIZE (1024 * 1024)      /* 1MB */

/* Default options */
static const ai_feature_extraction_options_t DEFAULT_OPTIONS = {
    .extraction_mode = FEATURE_EXTRACTION_STANDARD,
    .analyze_subtitles = true,
    .max_memory_mb = 256,
    .max_processing_time = CLOCK_FREQ * 60 /* 60 seconds */
};

/**
 * Extract features from a media item
 */
ai_media_feature_t *ExtractMediaFeatures(ai_recommender_t *recommender, input_item_t *item)
{
    if (!recommender || !item)
        return NULL;
        
    /* Get options from recommender */
    ai_feature_extraction_options_t options;
    options.extraction_mode = recommender->processing_mode;
    options.analyze_subtitles = recommender->analyze_subtitles;
    options.max_memory_mb = recommender->max_memory_mb;
    options.max_processing_time = CLOCK_FREQ * 60;  /* 60 seconds */
    
    /* Log extraction start for debugging */
    msg_Dbg(recommender->obj, "Starting feature extraction for %s", 
           item->psz_name ? item->psz_name : "(unknown item)");
           
    /* Start timing the extraction process */
    vlc_tick_t start_time = vlc_tick_now();
           
    /* Extract metadata first */
    ai_media_feature_t *features = ExtractMetadataFeatures(item, &options);
    if (!features) {
        msg_Warn(recommender->obj, "Failed to extract metadata features");
        return NULL;
    }
    
    msg_Dbg(recommender->obj, "Metadata features extracted successfully");
        
    /* For deeper analysis, extract additional features */
    if (options.extraction_mode >= FEATURE_EXTRACTION_STANDARD) {
        bool any_additional_features = false;
        ai_media_feature_t *audio_features = NULL;
        ai_media_feature_t *video_features = NULL;
        ai_media_feature_t *subtitle_features = NULL;
        
        /* Check if we've exceeded our time budget */
        if (vlc_tick_now() - start_time < options.max_processing_time) {
            /* Extract audio features */
            audio_features = ExtractAudioFeatures(item, &options);
            if (audio_features) {
                any_additional_features = true;
                msg_Dbg(recommender->obj, "Audio features extracted successfully");
            }
        }
        
        /* Check if we've exceeded our time budget */
        if (vlc_tick_now() - start_time < options.max_processing_time) {
            /* Extract video features */
            video_features = ExtractVideoFeatures(item, &options);
            if (video_features) {
                any_additional_features = true;
                msg_Dbg(recommender->obj, "Video features extracted successfully");
            }
        }
        
        /* Extract subtitle features if enabled and within time budget */
        if (options.analyze_subtitles && vlc_tick_now() - start_time < options.max_processing_time) {
            subtitle_features = ExtractSubtitleFeatures(item, &options);
            if (subtitle_features) {
                any_additional_features = true;
                msg_Dbg(recommender->obj, "Subtitle features extracted successfully");
            }
        }
        
        /* Combine all features if we have additional ones */
        if (any_additional_features) {
            ai_media_feature_t *feature_set[4];  /* Max 4 feature sets to combine */
            int count = 0;
            
            feature_set[count++] = features;
            if (audio_features) feature_set[count++] = audio_features;
            if (video_features) feature_set[count++] = video_features;
            if (subtitle_features) feature_set[count++] = subtitle_features;
            
            msg_Dbg(recommender->obj, "Combining %d feature sets", count);
            ai_media_feature_t *combined_features = CombineFeatures(feature_set, count);
            
            /* Clean up individual feature sets */
            if (audio_features) FreeMediaFeature(audio_features);
            if (video_features) FreeMediaFeature(video_features);
            if (subtitle_features) FreeMediaFeature(subtitle_features);
            
            /* Use combined features if successful */
            if (combined_features) {
                msg_Dbg(recommender->obj, "Feature combination successful");
                FreeMediaFeature(features);
                features = combined_features;
            } else {
                msg_Warn(recommender->obj, "Feature combination failed, using metadata only");
            }
        } else {
            msg_Dbg(recommender->obj, "No additional features extracted, using metadata only");
        }
    }
    
    /* Log extraction timing */
    vlc_tick_t elapsed = vlc_tick_now() - start_time;
    msg_Dbg(recommender->obj, "Feature extraction completed in %.3f seconds", 
           (double)elapsed / CLOCK_FREQ);
    
    return features;
}

/**
 * Extract metadata features from a media item
 */
ai_media_feature_t *ExtractMetadataFeatures(input_item_t *item, ai_feature_extraction_options_t *options)
{
    if (!item)
        return NULL;
        
    /* Use default options if none provided */
    ai_feature_extraction_options_t default_options = DEFAULT_OPTIONS;
    if (!options)
        options = &default_options;
        
    /* Allocate feature structure */
    ai_media_feature_t *features = calloc(1, sizeof(ai_media_feature_t));
    if (!features)
        return NULL;
        
    /* Extract item URI as ID */
    char *uri = input_item_GetURI(item);
    if (uri) {
        features->id = uri;
    } else {
        features->id = strdup("unknown");
    }
    
    /* Extract basic metadata */
    char *title = input_item_GetTitle(item);
    if (!title)
        title = input_item_GetName(item);
    if (title)
        features->title = strdup(title);
    else
        features->title = strdup("Unknown Title");
    free(title);
    
    /* Extract genre */
    char *genre = input_item_GetInfo(item, "General", "Genre");
    if (genre)
        features->genre = strdup(genre);
    free(genre);
    
    /* Extract creator */
    char *creator = input_item_GetInfo(item, "General", "Artist");
    if (!creator)
        creator = input_item_GetInfo(item, "General", "Director");
    if (!creator)
        creator = input_item_GetInfo(item, "General", "Author");
    if (creator)
        features->creator = strdup(creator);
    free(creator);
    
    /* Extract year */
    char *year_str = input_item_GetInfo(item, "General", "Date");
    if (year_str) {
        features->year = atoi(year_str);
        free(year_str);
    }
    
    /* Extract duration */
    features->duration = item->i_duration / CLOCK_FREQ;
    
    /* Extract rating */
    char *rating_str = input_item_GetInfo(item, "General", "Rating");
    if (rating_str) {
        features->rating = atof(rating_str);
        free(rating_str);
    }
    
    /* Create feature vector based on metadata */
    features->feature_vector = calloc(FEATURE_VECTOR_SIZE, sizeof(float));
    if (features->feature_vector) {
        features->feature_vector_size = FEATURE_VECTOR_SIZE;
        
        /* For deeper analysis, extract metadata as vector */
        if (options->extraction_mode >= FEATURE_EXTRACTION_STANDARD) {
            /* Get detailed metadata */
            ai_media_metadata_t *metadata = ExtractMediaMetadata(item);
            if (metadata) {
                /* Fill feature vector with metadata values */
                float *vector = (float*)features->feature_vector;
                
                /* Basic metadata features */
                if (metadata->genre) {
                    /* Genre hash to vector positions with improved safety */
                    uint64_t hash = 5381;  /* Use 64-bit hash to prevent overflow */
                    
                    /* Safe string iteration with explicit type conversion */
                    for (const unsigned char *p = (const unsigned char *)metadata->genre; *p; p++) {
                        hash = ((hash << 5) + hash) ^ (uint64_t)tolower(*p);
                    }
                    
                    /* Map to first 10 positions with bounds check */
                    for (int i = 0; i < 10 && i < FEATURE_VECTOR_SIZE; i++) {
                        vector[i] = ((hash >> (i % 64)) & 1) ? 1.0f : 0.0f;
                    }
                }
                
                /* Artist/creator hash to vector positions with improved safety */
                if (metadata->artist) {
                    uint64_t hash = 5381;  /* Use 64-bit hash for consistency */
                    
                    /* Safe string iteration with explicit type conversion */
                    for (const unsigned char *p = (const unsigned char *)metadata->artist; *p; p++) {
                        hash = ((hash << 5) + hash) ^ (uint64_t)tolower(*p);
                    }
                    
                    /* Map to next 10 positions with bounds check */
                    for (int i = 0; i < 10 && i < FEATURE_VECTOR_SIZE - 10; i++) {
                        vector[10 + i] = ((hash >> (i % 64)) & 1) ? 1.0f : 0.0f;
                    }
                }
                
                /* Year normalized to range 0.0-1.0 (1900-2100) */
                if (metadata->year >= 1900 && metadata->year <= 2100) {
                    vector[20] = (float)(metadata->year - 1900) / 200.0f;
                }
                
                /* Duration normalized to 0.0-1.0 (0-3hrs) */
                if (metadata->duration > 0) {
                    vector[21] = fminf(1.0f, metadata->duration / (3.0f * 3600.0f));
                }
                
                /* Technical features */
                if (metadata->audio_channels > 0) {
                    vector[22] = fminf(1.0f, metadata->audio_channels / 8.0f);
                }
                
                if (metadata->video_width > 0 && metadata->video_height > 0) {
                    float aspect = (float)metadata->video_width / metadata->video_height;
                    vector[23] = fminf(1.0f, fabsf(aspect - 1.78f) / 2.0f);
                    
                    /* Resolution class (SD, HD, 4K) normalized */
                    float max_dim = fmaxf(metadata->video_width, metadata->video_height);
                    vector[24] = fminf(1.0f, max_dim / 2160.0f);
                }
                
                /* Energy level and other computed features */
                vector[25] = metadata->energy_level;
                vector[26] = metadata->speech_ratio;
                vector[27] = metadata->music_ratio;
                vector[28] = metadata->brightness;
                vector[29] = metadata->contrast;
                
                /* Free metadata */
                FreeMediaMetadata(metadata);
            }
        } else {
            /* For light mode, just use basic features */
            float *vector = (float*)features->feature_vector;
            
            /* Basic features based on available data */
            if (features->genre) {
                uint64_t hash = 5381;  /* Use 64-bit hash for consistency */
                for (const unsigned char *p = (const unsigned char *)features->genre; *p; p++) {
                    hash = ((hash << 5) + hash) ^ (uint64_t)tolower(*p);
                }
                
                vector[0] = (float)(hash & 0xFF) / 255.0f;
                vector[1] = (float)((hash >> 8) & 0xFF) / 255.0f;
            }
            
            if (features->creator) {
                uint64_t hash = 5381;  /* Use 64-bit hash for consistency */
                for (const unsigned char *p = (const unsigned char *)features->creator; *p; p++) {
                    hash = ((hash << 5) + hash) ^ (uint64_t)tolower(*p);
                }
                
                vector[2] = (float)(hash & 0xFF) / 255.0f;
                vector[3] = (float)((hash >> 8) & 0xFF) / 255.0f;
            }
            
            if (features->year >= 1900 && features->year <= 2100) {
                vector[4] = (float)(features->year - 1900) / 200.0f;
            }
            
            if (features->duration > 0) {
                vector[5] = fminf(1.0f, features->duration / (3.0f * 3600.0f));
            }
            
            vector[6] = features->rating / 10.0f;
        }
    }
    
    return features;
}

/**
 * Extract audio features from a media item
 */
ai_media_feature_t *ExtractAudioFeatures(input_item_t *item, ai_feature_extraction_options_t *options)
{
    if (!item)
        return NULL;
        
    /* Use default options if none provided */
    ai_feature_extraction_options_t default_options = DEFAULT_OPTIONS;
    if (!options)
        options = &default_options;
        
    /* For this demo, we'll create a simplified audio feature extractor */
    ai_media_feature_t *features = calloc(1, sizeof(ai_media_feature_t));
    if (!features)
        return NULL;
        
    /* Copy basic identification info */
    char *uri = input_item_GetURI(item);
    if (uri)
        features->id = uri;
    else
        features->id = strdup("unknown");
    
    /* Create audio-specific feature vector */
    features->feature_vector = calloc(FEATURE_VECTOR_SIZE, sizeof(float));
    if (!features->feature_vector) {
        FreeMediaFeature(features);
        return NULL;
    }
    
    features->feature_vector_size = FEATURE_VECTOR_SIZE;
    
    /* Simulated audio features */
    float *vector = (float*)features->feature_vector;
    
    /* For deep mode, we would actually analyze audio samples */
    if (options->extraction_mode == FEATURE_EXTRACTION_DEEP) {
        /* Simulate random audio features for demonstration */
        /* In a real implementation, this would analyze actual audio content */
        
        /* Seed random number generator with hash of URI for consistency */
        unsigned int seed = 5381;
        if (features->id) {
            const unsigned char *p = (const unsigned char *)features->id;
            while (*p) {
                seed = ((seed << 5) + seed) ^ (*p++);
            }
        }
        
        /* Generate pseudo-random audio features with bounds checking */
        for (size_t i = 0; i < features->feature_vector_size; i++) {
            float r = (float)rand_r(&seed) / RAND_MAX;
            vector[i] = r;
        }
        
        /* Detect genre */
        char *detected_genre = DetectGenreFromAudio(NULL, 0);
        if (detected_genre) {
            features->genre = detected_genre;
        }
    } else {
        /* For standard mode, extract audio info from metadata */
        ai_media_metadata_t *metadata = ExtractMediaMetadata(item);
        if (metadata) {
            /* Copy relevant metadata */
            if (metadata->genre && !features->genre)
                features->genre = strdup(metadata->genre);
                
            /* Set audio-specific features */
            vector[0] = metadata->energy_level;
            vector[1] = metadata->speech_ratio;
            vector[2] = metadata->music_ratio;
            
            /* Add audio codec info */
            if (metadata->audio_codec) {
                uint64_t hash = 5381;  /* Use 64-bit hash for consistency */
                for (const unsigned char *p = (const unsigned char *)metadata->audio_codec; *p; p++) {
                    hash = ((hash << 5) + hash) ^ (uint64_t)tolower(*p);
                }
                
                vector[3] = (float)(hash & 0xFF) / 255.0f;
                vector[4] = (float)((hash >> 8) & 0xFF) / 255.0f;
            }
            
            FreeMediaMetadata(metadata);
        }
    }
    
    return features;
}

/**
 * Extract video features from a media item
 */
ai_media_feature_t *ExtractVideoFeatures(input_item_t *item, ai_feature_extraction_options_t *options)
{
    if (!item)
        return NULL;
        
    /* Use default options if none provided */
    ai_feature_extraction_options_t default_options = DEFAULT_OPTIONS;
    if (!options)
        options = &default_options;
        
    /* For this demo, create a simplified video feature extractor */
    ai_media_feature_t *features = calloc(1, sizeof(ai_media_feature_t));
    if (!features)
        return NULL;
        
    /* Copy basic identification info */
    char *uri = input_item_GetURI(item);
    if (uri)
        features->id = uri;
    else
        features->id = strdup("unknown");
    
    /* Create video-specific feature vector */
    features->feature_vector = calloc(FEATURE_VECTOR_SIZE, sizeof(float));
    if (!features->feature_vector) {
        FreeMediaFeature(features);
        return NULL;
    }
    
    features->feature_vector_size = FEATURE_VECTOR_SIZE;
    
    /* Simulated video features */
    float *vector = (float*)features->feature_vector;
    
    /* For deep mode, we would actually analyze video frames */
    if (options->extraction_mode == FEATURE_EXTRACTION_DEEP) {
        /* Simulate random video features for demonstration */
        /* In a real implementation, this would analyze actual video content */
        
        /* Seed random number generator with hash of URI for consistency */
        unsigned int seed = 5381;
        if (features->id) {
            const unsigned char *p = (const unsigned char *)features->id;
            while (*p) {
                seed = ((seed << 5) + seed) ^ (*p++);
            }
        }
        seed += 12345; /* Different seed than audio */
        
        /* Generate pseudo-random video features with bounds checking */
        for (size_t i = 0; i < features->feature_vector_size; i++) {
            float r = (float)rand_r(&seed) / RAND_MAX;
            vector[i] = r;
        }
    } else {
        /* For standard mode, extract video info from metadata */
        ai_media_metadata_t *metadata = ExtractMediaMetadata(item);
        if (metadata) {
            /* Set video-specific features */
            vector[0] = metadata->brightness;
            vector[1] = metadata->contrast;
            
            /* Resolution features */
            if (metadata->video_width > 0 && metadata->video_height > 0) {
                float aspect = (float)metadata->video_width / metadata->video_height;
                vector[2] = fminf(1.0f, fabsf(aspect - 1.78f) / 2.0f);
                
                /* Resolution class (SD, HD, 4K) normalized */
                float max_dim = fmaxf(metadata->video_width, metadata->video_height);
                vector[3] = fminf(1.0f, max_dim / 2160.0f);
            }
            
            /* Add video codec info */
            if (metadata->video_codec) {
                uint64_t hash = 5381;  /* Use 64-bit hash for consistency */
                for (const unsigned char *p = (const unsigned char *)metadata->video_codec; *p; p++) {
                    hash = ((hash << 5) + hash) ^ (uint64_t)tolower(*p);
                }
                
                vector[4] = (float)(hash & 0xFF) / 255.0f;
                vector[5] = (float)((hash >> 8) & 0xFF) / 255.0f;
            }
            
            FreeMediaMetadata(metadata);
        }
    }
    
    return features;
}

/**
 * Extract subtitle features from a media item
 */
ai_media_feature_t *ExtractSubtitleFeatures(input_item_t *item, ai_feature_extraction_options_t *options)
{
    if (!item || !options->analyze_subtitles)
        return NULL;
        
    /* Find subtitle file */
    char *subtitle_path = FindSubtitleFile(item);
    if (!subtitle_path)
        return NULL;
        
    /* Parse subtitle to text */
    char *subtitle_text = ParseSubtitleToText(subtitle_path);
    free(subtitle_path);
    
    if (!subtitle_text)
        return NULL;
        
    /* Create feature structure */
    ai_media_feature_t *features = calloc(1, sizeof(ai_media_feature_t));
    if (!features) {
        free(subtitle_text);
        return NULL;
    }
    
    /* Copy basic identification info */
    char *uri = input_item_GetURI(item);
    if (uri)
        features->id = uri;
    else
        features->id = strdup("unknown");
    
    /* Extract themes from subtitles */
    char **themes = ExtractThemesFromSubtitles(subtitle_text, MAX_THEMES);
    free(subtitle_text);
    
    /* Create text-based feature vector */
    features->feature_vector = calloc(FEATURE_VECTOR_SIZE, sizeof(float));
    if (!features->feature_vector) {
        if (themes) {
            for (int i = 0; themes[i] != NULL; i++)
                free(themes[i]);
            free(themes);
        }
        FreeMediaFeature(features);
        return NULL;
    }
    
    features->feature_vector_size = FEATURE_VECTOR_SIZE;
    
    /* If themes were extracted, use them to create features */
    if (themes) {
        float *vector = (float*)features->feature_vector;
        
        /* Count themes */
        int theme_count = 0;
        while (themes[theme_count] != NULL)
            theme_count++;
            
        /* Set theme-based features */
        for (int i = 0; i < theme_count && i < MAX_THEMES; i++) {
            /* Hash theme to get feature vector values with improved safety */
            uint64_t hash = 5381;
            for (const unsigned char *p = (const unsigned char *)themes[i]; *p; p++) {
                hash = ((hash << 5) + hash) ^ (uint64_t)tolower(*p);
            }
                
            /* Distribute theme info across multiple vector positions with bounds checking */
            int base_pos = (i * 10) % (FEATURE_VECTOR_SIZE - 10);
            for (int j = 0; j < 10 && (base_pos + j) < FEATURE_VECTOR_SIZE; j++) {
                vector[base_pos + j] += ((hash >> (j % 64)) & 1) ? 0.1f : 0.0f;
                
                /* Clamp to valid range */
                if (vector[base_pos + j] > 1.0f)
                    vector[base_pos + j] = 1.0f;
            }
        }
        
        /* Clean up themes */
        for (int i = 0; i < theme_count; i++)
            free(themes[i]);
        free(themes);
    }
    
    return features;
}

/**
 * Combine multiple feature sets
 */
ai_media_feature_t *CombineFeatures(ai_media_feature_t **features, int count)
{
    if (!features || count <= 0)
        return NULL;
        
    /* Validate input */
    for (int i = 0; i < count; i++) {
        if (!features[i])
            return NULL;
    }
    
    /* Create new feature structure */
    ai_media_feature_t *combined = calloc(1, sizeof(ai_media_feature_t));
    if (!combined)
        return NULL;
        
    /* Copy identification from first feature set */
    if (features[0]->id)
        combined->id = strdup(features[0]->id);
    else
        combined->id = strdup("unknown");
        
    if (features[0]->title)
        combined->title = strdup(features[0]->title);
        
    if (features[0]->genre)
        combined->genre = strdup(features[0]->genre);
    else {
        /* Try to find a genre in any feature set */
        for (int i = 1; i < count; i++) {
            if (features[i]->genre) {
                combined->genre = strdup(features[i]->genre);
                break;
            }
        }
    }
    
    if (features[0]->creator)
        combined->creator = strdup(features[0]->creator);
        
    combined->year = features[0]->year;
    combined->duration = features[0]->duration;
    combined->rating = features[0]->rating;
    
    /* Create combined feature vector */
    combined->feature_vector = calloc(FEATURE_VECTOR_SIZE, sizeof(float));
    if (!combined->feature_vector) {
        FreeMediaFeature(combined);
        return NULL;
    }
    
    combined->feature_vector_size = FEATURE_VECTOR_SIZE;
    
    /* Combine feature vectors with equal weighting */
    float *dest_vector = (float*)combined->feature_vector;
    
    /* Count valid feature vectors */
    int valid_vectors = 0;
    for (int i = 0; i < count; i++) {
        if (features[i]->feature_vector && 
            features[i]->feature_vector_size == FEATURE_VECTOR_SIZE) {
            valid_vectors++;
        }
    }
    
    if (valid_vectors > 0) {
        float weight = 1.0f / valid_vectors;
        
        /* Average all feature vectors */
        for (int i = 0; i < count; i++) {
            if (features[i]->feature_vector && 
                features[i]->feature_vector_size == FEATURE_VECTOR_SIZE) {
                
                float *src_vector = (float*)features[i]->feature_vector;
                for (size_t j = 0; j < FEATURE_VECTOR_SIZE; j++) {
                    dest_vector[j] += src_vector[j] * weight;
                }
            }
        }
    }
    
    return combined;
}

/**
 * Free a media feature structure
 */
void FreeMediaFeature(ai_media_feature_t *feature)
{
    if (!feature)
        return;
        
    free(feature->id);
    free(feature->title);
    free(feature->genre);
    free(feature->creator);
    free(feature->feature_vector);
    free(feature);
}

/**
 * Find subtitle file for a media item
 */
char *FindSubtitleFile(input_item_t *item)
{
    if (!item)
        return NULL;
        
    /* Get media file path */
    char *uri = input_item_GetURI(item);
    if (!uri)
        return NULL;
        
    /* Only handle local files */
    if (strncmp(uri, "file://", 7) != 0) {
        free(uri);
        return NULL;
    }
    
    /* Get local path */
    char *path = vlc_uri2path(uri);
    free(uri);
    
    if (!path)
        return NULL;
        
    /* Remove extension */
    char *ext = strrchr(path, '.');
    if (ext)
        *ext = '\0';
        
    /* Try common subtitle extensions */
    const char *sub_exts[] = {".srt", ".sub", ".sbv", ".vtt", ".ass", NULL};
    
    for (int i = 0; sub_exts[i]; i++) {
        char *sub_path;
        if (asprintf(&sub_path, "%s%s", path, sub_exts[i]) < 0) {
            sub_path = NULL;
            continue;
        }
        
        /* Check if file exists */
        if (access(sub_path, R_OK) == 0) {
            free(path);
            return sub_path;
        }
        
        free(sub_path);
    }
    
    free(path);
    return NULL;
}

/**
 * Calculate similarity between feature sets
 */
float CalculateFeatureSimilarity(ai_media_feature_t *feature1, 
                                ai_media_feature_t *feature2,
                                float *weights)
{
    if (!feature1 || !feature2)
        return 0.0f;
        
    /* Start with metadata similarity */
    float similarity = 0.0f;
    float weight_sum = 0.0f;
    
    /* Genre matching */
    if (feature1->genre && feature2->genre) {
        float genre_weight = weights ? weights[0] : 3.0f;
        if (strcasecmp(feature1->genre, feature2->genre) == 0) {
            similarity += genre_weight;
        } else {
            /* Partial genre matching with improved safety */
            char *genre1 = feature1->genre ? strdup(feature1->genre) : NULL;
            char *genre2 = feature2->genre ? strdup(feature2->genre) : NULL;
            
            if (genre1 && genre2) {
                /* Convert to lowercase safely */
                for (unsigned char *p = (unsigned char *)genre1; *p; p++) {
                    *p = (unsigned char)tolower(*p);
                }
                for (unsigned char *p = (unsigned char *)genre2; *p; p++) {
                    *p = (unsigned char)tolower(*p);
                }
                    
                /* Check if one contains the other */
                if (strstr(genre1, genre2) || strstr(genre2, genre1)) {
                    similarity += genre_weight * 0.7f;
                }
            }
            
            free(genre1);
            free(genre2);
        }
        weight_sum += genre_weight;
    }
    
    /* Creator matching */
    if (feature1->creator && feature2->creator) {
        float creator_weight = weights ? weights[1] : 2.0f;
        if (strcasecmp(feature1->creator, feature2->creator) == 0) {
            similarity += creator_weight;
        }
        weight_sum += creator_weight;
    }
    
    /* Year proximity */
    if (feature1->year > 0 && feature2->year > 0) {
        float year_weight = weights ? weights[2] : 1.0f;
        int year_diff = abs(feature1->year - feature2->year);
        if (year_diff <= 10) {
            similarity += year_weight * (1.0f - (year_diff / 10.0f));
        }
        weight_sum += year_weight;
    }
    
    /* Feature vector similarity */
    if (feature1->feature_vector && feature2->feature_vector &&
        feature1->feature_vector_size == feature2->feature_vector_size) {
        
        float vector_weight = weights ? weights[3] : 4.0f;
        float *vec1 = (float*)feature1->feature_vector;
        float *vec2 = (float*)feature2->feature_vector;
        size_t size = feature1->feature_vector_size;
        
        /* Calculate cosine similarity */
        float dot_product = 0.0f;
        float norm1 = 0.0f;
        float norm2 = 0.0f;
        
        for (size_t i = 0; i < size; i++) {
            dot_product += vec1[i] * vec2[i];
            norm1 += vec1[i] * vec1[i];
            norm2 += vec2[i] * vec2[i];
        }
        
        if (norm1 > 0.0f && norm2 > 0.0f) {
            float cosine = dot_product / (sqrtf(norm1) * sqrtf(norm2));
            similarity += vector_weight * cosine;
        }
        
        weight_sum += vector_weight;
    }
    
    /* Normalize result */
    if (weight_sum > 0.0f) {
        similarity /= weight_sum;
    }
    
    return similarity;
}

/**
 * Detect genre from audio features
 * This is a simplified implementation using signal characteristics
 */
char *DetectGenreFromAudio(uint8_t *audio_data, size_t audio_size)
{
    /* In a real implementation, this would analyze the audio spectrum,
       tempo, rhythm patterns, etc. to classify the genre */
       
    /* Define genres for our classifier */
    static const char *genres[] = {
        "Rock", "Pop", "Jazz", "Classical", "Hip-Hop", 
        "Electronic", "Country", "Blues", "Metal", "Folk"
    };
    static const int genre_count = 10; /* Number of predefined genres */
    
    /* If no data provided, use a deterministic approach based on time */
    if (!audio_data || audio_size == 0) {
        /* Use current time to create a more varied but deterministic selection */
        time_t now = time(NULL);
        if (now == (time_t)-1) {
            return NULL; /* Time function failed */
        }
        
        struct tm *tm_info = localtime(&now);
        if (!tm_info) {
            return NULL; /* localtime failed */
        }
        
        /* Use hour, minute and day to create a more varied selection */
        unsigned int seed = (tm_info->tm_hour * 100) + tm_info->tm_min + (tm_info->tm_mday * 1000);
        
        /* Create a deterministic but varied selection with bounds check */
        int index = seed % genre_count;
        
        char *result = strdup(genres[index]);
        if (!result) {
            return NULL; /* Memory allocation failed */
        }
        return result;
    }
    
    /* For actual audio data, analyze basic characteristics */
    /* This is still simplified but demonstrates the concept */
    
    /* Analyze first 1024 bytes for patterns (if available) */
    size_t analysis_size = audio_size > 1024 ? 1024 : audio_size;
    
    /* Simple metrics from audio data */
    double average = 0;
    double variance = 0;
    int zero_crossings = 0;
    int peaks = 0;
    
    /* First pass - calculate average */
    for (size_t i = 0; i < analysis_size; i++) {
        average += (double)audio_data[i];
    }
    average /= analysis_size;
    
    /* Second pass - calculate variance and other metrics */
    double prev_value = audio_data[0] - average;
    for (size_t i = 1; i < analysis_size; i++) {
        double value = audio_data[i] - average;
        variance += value * value;
        
        /* Count zero crossings (sign changes) */
        if ((prev_value >= 0 && value < 0) || (prev_value < 0 && value >= 0))
            zero_crossings++;
            
        /* Count peaks */
        if ((i > 0 && i < analysis_size - 1) &&
            ((audio_data[i] > audio_data[i-1] && audio_data[i] > audio_data[i+1]) ||
             (audio_data[i] < audio_data[i-1] && audio_data[i] < audio_data[i+1])))
            peaks++;
            
        prev_value = value;
    }
    variance /= analysis_size;
    
    /* Simple genre classification based on these metrics */
    /* These are arbitrary thresholds for demonstration */
    
    if (zero_crossings > analysis_size / 10 && peaks > analysis_size / 20)
        return strdup("Metal"); /* High zero crossings and peaks - metal */
        
    if (zero_crossings > analysis_size / 15 && peaks > analysis_size / 30)
        return strdup("Rock"); /* Moderately high - rock */
        
    if (zero_crossings < analysis_size / 40 && variance < 1000)
        return strdup("Classical"); /* Low zero crossings and variance - classical */
        
    if (zero_crossings > analysis_size / 25 && peaks < analysis_size / 40)
        return strdup("Electronic"); /* Many zero crossings but fewer peaks - electronic */
        
    if (zero_crossings < analysis_size / 30 && peaks > analysis_size / 25)
        return strdup("Country"); /* Lower zero crossings but many peaks - country */
        
    /* Default to a common genre */
    return strdup("Pop");
}

/**
 * Extract themes from subtitle text
 */
char **ExtractThemesFromSubtitles(const char *subtitle_text, int max_themes)
{
    if (!subtitle_text || max_themes <= 0)
        return NULL;
        
    /* Skip stop words */
    static const char *stop_words[] = {
        "a", "an", "the", "in", "on", "at", "of", "to", "for", "with", 
        "by", "as", "and", "or", "but", "is", "are", "was", "were", "be",
        "been", "have", "has", "had", "do", "does", "did", "will", "would",
        "can", "could", "should", "i", "you", "he", "she", "it", "we", "they",
        "this", "that", "these", "those", NULL
    };
    
    /* Allocate memory for themes */
    char **themes = calloc(max_themes + 1, sizeof(char*));
    if (!themes)
        return NULL;
        
    /* Copy text for tokenization */
    char *text_copy = strdup(subtitle_text);
    if (!text_copy) {
        free(themes);
        return NULL;
    }
    
    /* Convert to lowercase */
    for (char *p = text_copy; *p; p++) {
        *p = tolower(*p);
    }
    
    typedef struct {
        char *word;
        int count;
    } word_count_t;
    
    /* Count word frequencies (simple approach) */
    word_count_t *words = calloc(MAX_THEMES * 10, sizeof(word_count_t));
    if (!words) {
        free(text_copy);
        free(themes);
        return NULL;
    }
    
    int word_count = 0;
    char *saveptr;
    char *token = strtok_r(text_copy, " \t\n\r\f.,;:!?\"'()[]{}-_+=<>|/\\", &saveptr);
    
    while (token && word_count < MAX_THEMES * 10) {
        /* Skip short words and stop words */
        if (strlen(token) <= 2)
            goto next_token;
            
        for (int i = 0; stop_words[i]; i++) {
            if (!strcmp(token, stop_words[i]))
                goto next_token;
        }
        
        /* Check if word already exists */
        int found = -1;
        for (int i = 0; i < word_count; i++) {
            if (!strcmp(words[i].word, token)) {
                found = i;
                break;
            }
        }
        
        if (found >= 0) {
            /* Increment count for existing word */
            words[found].count++;
        } else {
            /* Add new word */
            words[word_count].word = strdup(token);
            if (words[word_count].word) {  /* Check for allocation success */
                words[word_count].count = 1;
                word_count++;
            }
        }
        
    next_token:
        token = strtok_r(NULL, " \t\n\r\f.,;:!?\"'()[]{}-_+=<>|/\\", &saveptr);
    }
    
    free(text_copy);
    
    /* Sort words by frequency */
    for (int i = 0; i < word_count - 1; i++) {
        for (int j = i + 1; j < word_count; j++) {
            if (words[j].count > words[i].count) {
                /* Swap */
                word_count_t temp = words[i];
                words[i] = words[j];
                words[j] = temp;
            }
        }
    }
    
    /* Copy top words as themes */
    int theme_count = 0;
    for (int i = 0; i < word_count && theme_count < max_themes; i++) {
        themes[theme_count++] = words[i].word;
    }
    themes[theme_count] = NULL;
    
    /* Free word array but not the words themselves (transferred to themes) */
    free(words);
    
    return themes;
}

/**
 * Parse subtitle file into plain text
 */
char *ParseSubtitleToText(const char *subtitle_path)
{
    if (!subtitle_path)
        return NULL;
        
    /* Open subtitle file */
    FILE *file = fopen(subtitle_path, "r");
    if (!file)
        return NULL;
        
    /* Get file size */
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    /* Don't process excessively large files */
    if (size <= 0 || size > MAX_SUBTITLE_SIZE) {
        fclose(file);
        return NULL;
    }
    
    /* Read entire file */
    char *content = malloc(size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }
    
    size_t read_size = fread(content, 1, size, file);
    fclose(file);
    
    if (read_size != (size_t)size) {
        free(content);
        return NULL;
    }
    
    content[size] = '\0';
    
    /* Process based on file extension */
    const char *ext = strrchr(subtitle_path, '.');
    if (!ext) {
        free(content);
        return NULL;
    }
    
    /* Allocate buffer for cleaned text */
    char *text = malloc(size + 1);
    if (!text) {
        free(content);
        return NULL;
    }
    
    /* Process different subtitle formats */
    if (strcasecmp(ext, ".srt") == 0) {
        /* SRT format */
        char *src = content;
        char *dst = text;
        int line_num = 0;
        
        while (*src) {
            /* Skip index numbers and timestamps */
            if (isdigit(*src) && line_num == 0) {
                /* Skip this line (index) */
                while (*src && *src != '\n')
                    src++;
                if (*src) src++;
                line_num = 1;
                continue;
            } else if (strchr("0123456789:", *src) && 
                       (strstr(src, "-->") != NULL) && 
                       line_num == 1) {
                /* Skip this line (timestamp) */
                while (*src && *src != '\n')
                    src++;
                if (*src) src++;
                line_num = 2;
                continue;
            } else if (*src == '\n') {
                /* Empty line, reset line counter */
                *dst++ = ' ';
                src++;
                line_num = 0;
                continue;
            }
            
            /* Copy actual subtitle text */
            if (line_num >= 2) {
                *dst++ = *src;
            }
            
            src++;
        }
        
        *dst = '\0';
    } else if (strcasecmp(ext, ".sub") == 0 || 
               strcasecmp(ext, ".vtt") == 0) {
        /* WebVTT/SUB format */
        char *src = content;
        char *dst = text;
        int in_header = 1;
        
        /* Skip header */
        while (in_header && *src) {
            if (*src == '\n' && *(src+1) == '\n') {
                in_header = 0;
                src += 2;
            } else {
                src++;
            }
        }
        
        int line_num = 0;
        while (*src) {
            /* Skip timestamps */
            if ((isdigit(*src) || *src == '-' || *src == '>') && 
                (strstr(src, "-->") != NULL) &&
                line_num == 0) {
                while (*src && *src != '\n')
                    src++;
                if (*src) src++;
                line_num = 1;
                continue;
            } else if (*src == '\n') {
                *dst++ = ' ';
                src++;
                line_num = 0;
                continue;
            }
            
            /* Copy actual subtitle text */
            if (line_num > 0) {
                *dst++ = *src;
            }
            
            src++;
        }
        
        *dst = '\0';
    } else {
        /* Unknown format - just strip newlines */
        char *src = content;
        char *dst = text;
        
        while (*src) {
            if (*src == '\n' || *src == '\r') {
                *dst++ = ' ';
            } else {
                *dst++ = *src;
            }
            src++;
        }
        
        *dst = '\0';
    }
    
    free(content);
    return text;
}
