/*****************************************************************************
 * ai_recommendations.h: AI-Powered Media Recommendation module for VLC
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

#ifndef VLC_AI_RECOMMENDATIONS_H
#define VLC_AI_RECOMMENDATIONS_H

#include <vlc_common.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <vlc_input_item.h>
#include <vlc_media_library.h>
#include <vlc_arrays.h>

/**
 * @file
 * This file implements an AI-powered media recommendation engine for VLC
 */

#define AI_RECOMMENDER_CFG_PREFIX "ai-recommender-"

/* Module settings */
#define CFG_PREFIX AI_RECOMMENDER_CFG_PREFIX

/* Macro for translation */
#ifndef N_
#define N_(str) (str)
#endif

/* Configuration options */
#define PROCESSING_MODE_TEXT N_("Processing mode")
#define PROCESSING_MODE_LONGTEXT N_("Choose how intensive the recommendation processing should be")
static const int processing_mode_values[] = { 0, 1, 2 };
static const char *const processing_mode_texts[] = {
    N_("Light (metadata only)"), 
    N_("Standard (metadata + basic content)"),
    N_("Deep (comprehensive analysis)")
};

#define MAX_MEMORY_TEXT N_("Maximum memory usage (MB)")
#define MAX_MEMORY_LONGTEXT N_("Limit memory consumption for analysis tasks")

#define ENABLE_SUBTITLES_TEXT N_("Analyze subtitles")
#define ENABLE_SUBTITLES_LONGTEXT N_("Extract themes and topics from subtitle content")

#define SCAN_INTERVAL_TEXT N_("Library scan interval (minutes)")
#define SCAN_INTERVAL_LONGTEXT N_("Time between automatic media library scans")

#define NUM_RECOMMENDATIONS_TEXT N_("Number of recommendations")
#define NUM_RECOMMENDATIONS_LONGTEXT N_("Number of recommendations to display")

/* Recommendation types */
typedef enum {
    RECOMMENDATION_TYPE_SIMILAR_CONTENT = 0,
    RECOMMENDATION_TYPE_CONTINUATION = 1,
    RECOMMENDATION_TYPE_MOOD_BASED = 2,
    RECOMMENDATION_TYPE_DISCOVERY = 3,
    RECOMMENDATION_TYPE_COUNT
} ai_recommendation_type_t;

/* Media feature data structure */
typedef struct {
    char *id;
    char *title;
    char *genre;
    char *creator;
    int year;
    int duration;
    float rating;
    void *feature_vector;
    size_t feature_vector_size;
} ai_media_feature_t;

/* Viewing record */
typedef struct {
    char *media_id;
    time_t timestamp;
    float completion_ratio;
    char *context;
} ai_viewing_record_t;

/* User profile data structure */
typedef struct {
    char *id;
    char *name;
    struct {
        char *key;
        float value;
    } *genre_preferences;
    int genre_preferences_count;
    struct {
        char *key;
        float value;
    } *creator_preferences;
    int creator_preferences_count;
    struct {
        char *key;
        float value;
    } *feature_preferences;
    int feature_preferences_count;
    ai_viewing_record_t **viewing_history;
    int viewing_history_count;
    time_t creation_time;
    time_t last_updated;
} ai_user_profile_t;

/* Recommendation item */
typedef struct {
    input_item_t *item;
    float score;
    char *explanation;
    ai_recommendation_type_t type;
} ai_recommendation_item_t;

/* Recommendation set */
typedef struct {
    ai_recommendation_item_t **items;
    int items_count;
    char *reference_id;
    time_t generation_time;
} ai_recommendation_set_t;

/* Main module structure */
typedef struct {
    vlc_object_t *obj;
    vlc_playlist_t *playlist;
    vlc_medialibrary_t *ml;
    vlc_mutex_t lock;
    vlc_timer_t scan_timer;
    vlc_thread_t worker_thread;
    
    /* Event listeners */
    vlc_playlist_listener_id *playlist_listener;
    vlc_ml_event_callback_t *ml_listener;
    
    /* Feature extraction and analysis */
    bool processing_active;
    int processing_mode;
    int max_memory_mb;
    bool analyze_subtitles;
    
    /* User profiles */
    ai_user_profile_t *current_profile;
    char **profile_ids;
    ai_user_profile_t **profiles;
    int profiles_count;
    
    /* Recommendation cache */
    char **cache_ids;
    ai_recommendation_set_t **recommendation_cache;
    int recommendation_cache_count;
    
    /* UI Integration */
    bool ui_created;
    void *ui_data;
    int ui_mode;
    
    /* Configuration */
    int scan_interval;
    int num_recommendations;
} ai_recommender_t;

/* Module functions */
int Open_ai_recommendations(vlc_object_t *);
void Close_ai_recommendations(vlc_object_t *);

/* Feature extraction */
ai_media_feature_t *ExtractMediaFeatures(ai_recommender_t *, input_item_t *);
void FreeMediaFeature(ai_media_feature_t *);

/* Profile management */
ai_user_profile_t *CreateUserProfile(const char *id, const char *name);
void UpdateUserProfile(ai_recommender_t *, ai_user_profile_t *, input_item_t *, float completion);
void SaveUserProfiles(ai_recommender_t *);
int LoadUserProfiles(ai_recommender_t *);
void FreeUserProfile(ai_user_profile_t *);

/* Recommendation engine */
ai_recommendation_set_t *GenerateRecommendations(ai_recommender_t *, input_item_t *, int count);
void FreeRecommendationSet(ai_recommendation_set_t *);
void UpdateRecommendationCache(ai_recommender_t *);
void ClearRecommendationCache(ai_recommender_t *);

#endif /* VLC_AI_RECOMMENDATIONS_H */