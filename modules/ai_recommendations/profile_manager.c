/*****************************************************************************
 * profile_manager.c: AI-Powered Media Recommendation user profile manager
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
 * AI-Powered Media Recommendation user profile manager
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "custom_compat.h"
#include <vlc_fs.h>
#include <vlc_plugin.h>
#include <vlc_fs.h>
#include <vlc_configuration.h>
#include <vlc_charset.h>
#include <vlc_strings.h>
#include <vlc_memstream.h>
#include <vlc_arrays.h>
#include <vlc_playlist.h>
#include <vlc_input_item.h>
#include <vlc_media_library.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <ctype.h>

#include "ai_recommendations.h"
#include "profile_manager.h"
#include "metadata_analyzer.h"
#include "feature_extractor.h"

/* Constants for profile manager */
#define MAX_PROFILES 10
#define MAX_HISTORY_SIZE 100
#define MAX_PREFERENCES 50
#define PROFILE_FORMAT_VERSION 1
#define MAX_LINE_LENGTH 1024

/* File format delimiters */
#define FIELD_SEPARATOR ":"
#define RECORD_SEPARATOR ";"
#define SECTION_MARKER "#"

/**
 * Create a new user profile
 */
ai_user_profile_t *CreateUserProfile(const char *id, const char *name)
{
    if (!id || !name)
        return NULL;
        
    ai_user_profile_t *profile = calloc(1, sizeof(ai_user_profile_t));
    if (!profile)
        return NULL;
        
    profile->id = strdup(id);
    profile->name = strdup(name);
    if (!profile->id || !profile->name) {
        FreeUserProfile(profile);
        return NULL;
    }
    
    /* Initialize arrays */
    profile->genre_preferences = NULL;
    profile->genre_preferences_count = 0;
    
    profile->creator_preferences = NULL;
    profile->creator_preferences_count = 0;
    
    profile->feature_preferences = NULL;
    profile->feature_preferences_count = 0;
    
    profile->viewing_history = NULL;
    profile->viewing_history_count = 0;
    
    /* Set timestamps */
    profile->creation_time = time(NULL);
    profile->last_updated = profile->creation_time;
    
    return profile;
}

/**
 * Free a viewing record
 */
void FreeViewingRecord(ai_viewing_record_t *record)
{
    if (!record)
        return;
        
    free(record->media_id);
    free(record->context);
    free(record);
}

/**
 * Free a user profile and its resources
 */
void FreeUserProfile(ai_user_profile_t *profile)
{
    if (!profile)
        return;
        
    free(profile->id);
    free(profile->name);
    
    /* Free genre preferences */
    for (int i = 0; i < profile->genre_preferences_count; i++) {
        free(profile->genre_preferences[i].key);
    }
    free(profile->genre_preferences);
    
    /* Free creator preferences */
    for (int i = 0; i < profile->creator_preferences_count; i++) {
        free(profile->creator_preferences[i].key);
    }
    free(profile->creator_preferences);
    
    /* Free feature preferences */
    for (int i = 0; i < profile->feature_preferences_count; i++) {
        free(profile->feature_preferences[i].key);
    }
    free(profile->feature_preferences);
    
    /* Free viewing history */
    for (int i = 0; i < profile->viewing_history_count; i++) {
        FreeViewingRecord(profile->viewing_history[i]);
    }
    free(profile->viewing_history);
    
    free(profile);
}

/**
 * Create a viewing record
 */
ai_viewing_record_t *CreateViewingRecord(const char *media_id, float completion_ratio)
{
    if (!media_id)
        return NULL;
        
    ai_viewing_record_t *record = calloc(1, sizeof(ai_viewing_record_t));
    if (!record)
        return NULL;
        
    record->media_id = strdup(media_id);
    if (!record->media_id) {
        free(record);
        return NULL;
    }
    
    record->timestamp = time(NULL);
    record->completion_ratio = completion_ratio;
    
    /* For now, no context information */
    record->context = NULL;
    
    return record;
}

/**
 * Add a viewing record to a user profile
 */
void AddViewingRecord(ai_user_profile_t *profile, ai_viewing_record_t *record)
{
    if (!profile || !record)
        return;
        
    /* Check for duplicate media_id */
    for (int i = 0; i < profile->viewing_history_count; i++) {
        if (profile->viewing_history[i] && 
            strcmp(profile->viewing_history[i]->media_id, record->media_id) == 0) {
            
            /* Update existing record */
            profile->viewing_history[i]->timestamp = record->timestamp;
            profile->viewing_history[i]->completion_ratio = record->completion_ratio;
            
            free(profile->viewing_history[i]->context);
            profile->viewing_history[i]->context = record->context ? strdup(record->context) : NULL;
            
            /* Free the new record since we didn't use it */
            free(record->media_id);
            free(record);
            
            /* Update profile timestamp */
            profile->last_updated = time(NULL);
            return;
        }
    }
    
    /* Check if we need to limit history size */
    if (profile->viewing_history_count >= MAX_HISTORY_SIZE) {
        /* Find oldest record */
        int oldest_idx = 0;
        time_t oldest_time = profile->viewing_history[0]->timestamp;
        
        for (int i = 1; i < profile->viewing_history_count; i++) {
            if (profile->viewing_history[i]->timestamp < oldest_time) {
                oldest_time = profile->viewing_history[i]->timestamp;
                oldest_idx = i;
            }
        }
        
        /* Replace oldest record */
        FreeViewingRecord(profile->viewing_history[oldest_idx]);
        profile->viewing_history[oldest_idx] = record;
    } else {
        /* Add new record */
        ai_viewing_record_t **new_history = realloc(profile->viewing_history,
            (profile->viewing_history_count + 1) * sizeof(ai_viewing_record_t*));
            
        if (new_history) {
            profile->viewing_history = new_history;
            profile->viewing_history[profile->viewing_history_count++] = record;
        } else {
            /* If realloc failed, free the record */
            FreeViewingRecord(record);
        }
    }
    
    /* Update profile timestamp */
    profile->last_updated = time(NULL);
}

/**
 * Update user profile with information about media viewing
 */
void UpdateUserProfile(ai_recommender_t *recommender, 
                      ai_user_profile_t *profile, 
                      input_item_t *media, 
                      float completion)
{
    if (!recommender || !profile || !media)
        return;
        
    /* Extract media ID */
    char *media_id = input_item_GetURI(media);
    if (!media_id)
        return;
        
    /* Create and add viewing record */
    ai_viewing_record_t *record = CreateViewingRecord(media_id, completion);
    if (record) {
        AddViewingRecord(profile, record);
    }
    
    /* Extract metadata for preference updates */
    ai_media_metadata_t *metadata = ExtractMediaMetadata(media);
    if (metadata) {
        /* Update genre preference */
        if (metadata->genre) {
            UpdateGenrePreference(profile, metadata->genre, 0.1f);
        }
        
        /* Update creator preference */
        if (metadata->artist) {
            UpdateCreatorPreference(profile, metadata->artist, 0.1f);
        }
        
        /* Free metadata */
        FreeMediaMetadata(metadata);
    }
    
    free(media_id);
}

/**
 * Update genre preference in user profile
 */
void UpdateGenrePreference(ai_user_profile_t *profile, const char *genre, float weight)
{
    if (!profile || !genre)
        return;
        
    /* Check if genre already exists */
    for (int i = 0; i < profile->genre_preferences_count; i++) {
        if (!strcasecmp(profile->genre_preferences[i].key, genre)) {
            /* Update existing preference */
            profile->genre_preferences[i].value += weight;
            
            /* Clamp to valid range */
            if (profile->genre_preferences[i].value > 1.0f)
                profile->genre_preferences[i].value = 1.0f;
                
            /* Update profile timestamp */
            profile->last_updated = time(NULL);
            return;
        }
    }
    
    /* Limit number of preferences */
    if (profile->genre_preferences_count >= MAX_PREFERENCES)
        return;
        
    /* Add new preference */
    void *new_prefs = realloc(profile->genre_preferences,
        (profile->genre_preferences_count + 1) * sizeof(profile->genre_preferences[0]));
        
    if (!new_prefs)
        return;
        
    profile->genre_preferences = new_prefs;
    profile->genre_preferences[profile->genre_preferences_count].key = strdup(genre);
    
    if (!profile->genre_preferences[profile->genre_preferences_count].key)
        return;
        
    profile->genre_preferences[profile->genre_preferences_count].value = 
        0.5f + weight; /* Start with medium preference and add weight */
        
    /* Clamp to valid range */
    if (profile->genre_preferences[profile->genre_preferences_count].value > 1.0f)
        profile->genre_preferences[profile->genre_preferences_count].value = 1.0f;
        
    profile->genre_preferences_count++;
    
    /* Update profile timestamp */
    profile->last_updated = time(NULL);
}

/**
 * Update creator preference in user profile
 */
void UpdateCreatorPreference(ai_user_profile_t *profile, const char *creator, float weight)
{
    if (!profile || !creator)
        return;
        
    /* Check if creator already exists */
    for (int i = 0; i < profile->creator_preferences_count; i++) {
        if (!strcasecmp(profile->creator_preferences[i].key, creator)) {
            /* Update existing preference */
            profile->creator_preferences[i].value += weight;
            
            /* Clamp to valid range */
            if (profile->creator_preferences[i].value > 1.0f)
                profile->creator_preferences[i].value = 1.0f;
                
            /* Update profile timestamp */
            profile->last_updated = time(NULL);
            return;
        }
    }
    
    /* Limit number of preferences */
    if (profile->creator_preferences_count >= MAX_PREFERENCES)
        return;
        
    /* Add new preference */
    void *new_prefs = realloc(profile->creator_preferences,
        (profile->creator_preferences_count + 1) * sizeof(profile->creator_preferences[0]));
        
    if (!new_prefs)
        return;
        
    profile->creator_preferences = new_prefs;
    profile->creator_preferences[profile->creator_preferences_count].key = strdup(creator);
    
    if (!profile->creator_preferences[profile->creator_preferences_count].key)
        return;
        
    profile->creator_preferences[profile->creator_preferences_count].value = 
        0.5f + weight; /* Start with medium preference and add weight */
        
    /* Clamp to valid range */
    if (profile->creator_preferences[profile->creator_preferences_count].value > 1.0f)
        profile->creator_preferences[profile->creator_preferences_count].value = 1.0f;
        
    profile->creator_preferences_count++;
    
    /* Update profile timestamp */
    profile->last_updated = time(NULL);
}

/**
 * Update feature preference in user profile
 */
void UpdateFeaturePreference(ai_user_profile_t *profile, const char *feature, float weight)
{
    if (!profile || !feature)
        return;
        
    /* Check if feature already exists */
    for (int i = 0; i < profile->feature_preferences_count; i++) {
        if (!strcasecmp(profile->feature_preferences[i].key, feature)) {
            /* Update existing preference */
            profile->feature_preferences[i].value += weight;
            
            /* Clamp to valid range */
            if (profile->feature_preferences[i].value > 1.0f)
                profile->feature_preferences[i].value = 1.0f;
                
            /* Update profile timestamp */
            profile->last_updated = time(NULL);
            return;
        }
    }
    
    /* Limit number of preferences */
    if (profile->feature_preferences_count >= MAX_PREFERENCES)
        return;
        
    /* Add new preference */
    void *new_prefs = realloc(profile->feature_preferences,
        (profile->feature_preferences_count + 1) * sizeof(profile->feature_preferences[0]));
        
    if (!new_prefs)
        return;
        
    profile->feature_preferences = new_prefs;
    profile->feature_preferences[profile->feature_preferences_count].key = strdup(feature);
    
    if (!profile->feature_preferences[profile->feature_preferences_count].key)
        return;
        
    profile->feature_preferences[profile->feature_preferences_count].value = 
        0.5f + weight; /* Start with medium preference and add weight */
        
    /* Clamp to valid range */
    if (profile->feature_preferences[profile->feature_preferences_count].value > 1.0f)
        profile->feature_preferences[profile->feature_preferences_count].value = 1.0f;
        
    profile->feature_preferences_count++;
    
    /* Update profile timestamp */
    profile->last_updated = time(NULL);
}

/**
 * Get the preference weight for a genre
 */
float GetGenrePreference(ai_user_profile_t *profile, const char *genre)
{
    if (!profile || !genre)
        return 0.5f; /* Default neutral preference */
        
    for (int i = 0; i < profile->genre_preferences_count; i++) {
        if (!strcasecmp(profile->genre_preferences[i].key, genre)) {
            return profile->genre_preferences[i].value;
        }
    }
    
    return 0.5f; /* Default if not found */
}

/**
 * Get the preference weight for a creator
 */
float GetCreatorPreference(ai_user_profile_t *profile, const char *creator)
{
    if (!profile || !creator)
        return 0.5f; /* Default neutral preference */
        
    for (int i = 0; i < profile->creator_preferences_count; i++) {
        if (!strcasecmp(profile->creator_preferences[i].key, creator)) {
            return profile->creator_preferences[i].value;
        }
    }
    
    return 0.5f; /* Default if not found */
}

/**
 * Get the preference weight for a feature
 */
float GetFeaturePreference(ai_user_profile_t *profile, const char *feature)
{
    if (!profile || !feature)
        return 0.5f; /* Default neutral preference */
        
    for (int i = 0; i < profile->feature_preferences_count; i++) {
        if (!strcasecmp(profile->feature_preferences[i].key, feature)) {
            return profile->feature_preferences[i].value;
        }
    }
    
    return 0.5f; /* Default if not found */
}

/**
 * Get recently viewed media IDs
 */
char **GetRecentlyViewed(ai_user_profile_t *profile, int count)
{
    if (!profile || count <= 0)
        return NULL;
        
    /* Limit count to actual history size */
    if (count > profile->viewing_history_count)
        count = profile->viewing_history_count;
        
    if (count == 0)
        return NULL;
        
    /* Allocate result array */
    char **result = calloc(count + 1, sizeof(char*));
    if (!result)
        return NULL;
        
    /* Create a temporary array for sorting */
    typedef struct {
        ai_viewing_record_t *record;
        time_t timestamp;
    } sorted_record_t;
    
    sorted_record_t *sorted = calloc(profile->viewing_history_count, sizeof(sorted_record_t));
    if (!sorted) {
        free(result);
        return NULL;
    }
    
    /* Fill sort array */
    for (int i = 0; i < profile->viewing_history_count; i++) {
        sorted[i].record = profile->viewing_history[i];
        sorted[i].timestamp = profile->viewing_history[i]->timestamp;
    }
    
    /* Sort by timestamp (newest first) */
    for (int i = 0; i < profile->viewing_history_count - 1; i++) {
        for (int j = i + 1; j < profile->viewing_history_count; j++) {
            if (sorted[j].timestamp > sorted[i].timestamp) {
                sorted_record_t temp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = temp;
            }
        }
    }
    
    /* Copy the media IDs */
    for (int i = 0; i < count; i++) {
        result[i] = strdup(sorted[i].record->media_id);
    }
    
    free(sorted);
    return result;
}

/**
 * Get frequently viewed media IDs
 */
char **GetFrequentlyViewed(ai_user_profile_t *profile, int count)
{
    if (!profile || count <= 0)
        return NULL;
        
    /* Need to count media occurrences */
    typedef struct {
        char *media_id;
        int count;
    } media_count_t;
    
    /* Create a map of media IDs to counts */
    media_count_t *counts = calloc(profile->viewing_history_count, sizeof(media_count_t));
    if (!counts)
        return NULL;
        
    int unique_count = 0;
    
    /* Count occurrences */
    for (int i = 0; i < profile->viewing_history_count; i++) {
        char *media_id = profile->viewing_history[i]->media_id;
        
        /* Check if already counted */
        int found = -1;
        for (int j = 0; j < unique_count; j++) {
            if (!strcmp(counts[j].media_id, media_id)) {
                found = j;
                break;
            }
        }
        
        if (found >= 0) {
            counts[found].count++;
        } else {
            counts[unique_count].media_id = media_id;
            counts[unique_count].count = 1;
            unique_count++;
        }
    }
    
    /* Sort by count (highest first) */
    for (int i = 0; i < unique_count - 1; i++) {
        for (int j = i + 1; j < unique_count; j++) {
            if (counts[j].count > counts[i].count) {
                media_count_t temp = counts[i];
                counts[i] = counts[j];
                counts[j] = temp;
            }
        }
    }
    
    /* Limit count to actual unique items */
    if (count > unique_count)
        count = unique_count;
        
    /* Create result array */
    char **result = calloc(count + 1, sizeof(char*));
    if (!result) {
        free(counts);
        return NULL;
    }
    
    /* Copy the most frequent media IDs */
    for (int i = 0; i < count; i++) {
        result[i] = strdup(counts[i].media_id);
    }
    
    free(counts);
    return result;
}

/**
 * Get VLC configuration directory for profiles
 */
static char *GetProfilesDirectory(void)
{
    char *config_dir = config_GetUserDir(VLC_CONFIG_DIR);
    if (!config_dir)
        return NULL;
        
    char *profiles_dir;
    if (asprintf(&profiles_dir, "%s/%s", config_dir, PROFILES_DIR_NAME) < 0) {
        free(config_dir);
        return NULL;
    }
    
    free(config_dir);
    
    /* Create directory if it doesn't exist */
    if (vlc_mkdir(profiles_dir, 0755) != 0 && errno != EEXIST) {
        free(profiles_dir);
        return NULL;
    }
    
    return profiles_dir;
}

/**
 * Get file path for a profile
 */
static char *GetProfilePath(const char *profile_id)
{
    char *profiles_dir = GetProfilesDirectory();
    if (!profiles_dir)
        return NULL;
        
    char *profile_path;
    if (asprintf(&profile_path, "%s/%s%s", profiles_dir, profile_id, PROFILE_FILE_EXT) < 0) {
        free(profiles_dir);
        return NULL;
    }
    
    free(profiles_dir);
    return profile_path;
}

/**
 * Save user profiles to storage
 */
void SaveUserProfiles(ai_recommender_t *recommender)
{
    if (!recommender)
        return;
        
    vlc_mutex_lock(&recommender->lock);
    
    if (!recommender->profiles || recommender->profiles_count <= 0) {
        vlc_mutex_unlock(&recommender->lock);
        return;
    }
    
    /* Create profiles directory if it doesn't exist */
    char *profiles_dir = GetProfilesDirectory();
    if (!profiles_dir) {
        vlc_mutex_unlock(&recommender->lock);
        return;
    }
    free(profiles_dir);
    
    /* Save each profile */
    for (int i = 0; i < recommender->profiles_count; i++) {
        ai_user_profile_t *profile = recommender->profiles[i];
        if (!profile)
            continue;
            
        /* Get profile file path */
        char *profile_path = GetProfilePath(profile->id);
        if (!profile_path)
            continue;
            
        /* Open file for writing */
        FILE *fp = vlc_fopen(profile_path, "w");
        if (!fp) {
            free(profile_path);
            continue;
        }
        
        /* Write file header */
        fprintf(fp, "# VLC AI Recommendations Profile\n");
        fprintf(fp, "# Version: %d\n", PROFILE_FORMAT_VERSION);
        fprintf(fp, "# Created: %ld\n", (long)profile->creation_time);
        fprintf(fp, "# Updated: %ld\n\n", (long)profile->last_updated);
        
        /* Write basic profile info */
        fprintf(fp, "[Profile]\n");
        fprintf(fp, "ID: %s\n", profile->id);
        fprintf(fp, "Name: %s\n", profile->name);
        fprintf(fp, "CreationTime: %ld\n", (long)profile->creation_time);
        fprintf(fp, "LastUpdated: %ld\n\n", (long)profile->last_updated);
        
        /* Write genre preferences */
        fprintf(fp, "[GenrePreferences]\n");
        for (int j = 0; j < profile->genre_preferences_count; j++) {
            fprintf(fp, "%s: %.6f\n", profile->genre_preferences[j].key, 
                                     profile->genre_preferences[j].value);
        }
        fprintf(fp, "\n");
        
        /* Write creator preferences */
        fprintf(fp, "[CreatorPreferences]\n");
        for (int j = 0; j < profile->creator_preferences_count; j++) {
            fprintf(fp, "%s: %.6f\n", profile->creator_preferences[j].key, 
                                     profile->creator_preferences[j].value);
        }
        fprintf(fp, "\n");
        
        /* Write feature preferences */
        fprintf(fp, "[FeaturePreferences]\n");
        for (int j = 0; j < profile->feature_preferences_count; j++) {
            fprintf(fp, "%s: %.6f\n", profile->feature_preferences[j].key, 
                                     profile->feature_preferences[j].value);
        }
        fprintf(fp, "\n");
        
        /* Write viewing history */
        fprintf(fp, "[ViewingHistory]\n");
        for (int j = 0; j < profile->viewing_history_count; j++) {
            ai_viewing_record_t *record = profile->viewing_history[j];
            fprintf(fp, "%s: %ld, %.6f\n", record->media_id, 
                                         (long)record->timestamp, 
                                         record->completion_ratio);
        }
        
        fclose(fp);
        free(profile_path);
    }
    
    vlc_mutex_unlock(&recommender->lock);
}

/**
 * Parse a line with key/value pair
 */
static int ParseKeyValue(char *line, char **key, char **value)
{
    /* Remove leading/trailing whitespace */
    char *p = line;
    while (*p && isspace(*p)) p++;
    
    char *end = p + strlen(p) - 1;
    while (end > p && isspace(*end)) {
        *end = '\0';
        end--;
    }
    
    if (*p == '\0' || *p == '#')
        return -1;  /* Comment or empty line */
    
    /* Find separator */
    char *sep = strchr(p, ':');
    if (!sep)
        return -1;  /* No separator */
    
    /* Split into key/value */
    *sep = '\0';
    *key = p;
    *value = sep + 1;
    
    /* Trim key */
    end = *key + strlen(*key) - 1;
    while (end > *key && isspace(*end)) {
        *end = '\0';
        end--;
    }
    
    /* Trim value */
    while (**value && isspace(**value)) (*value)++;
    end = *value + strlen(*value) - 1;
    while (end > *value && isspace(*end)) {
        *end = '\0';
        end--;
    }
    
    return 0;
}

/**
 * Load user profiles from storage
 */
int LoadUserProfiles(ai_recommender_t *recommender)
{
    if (!recommender)
        return VLC_EGENERIC;
        
    vlc_mutex_lock(&recommender->lock);
    
    /* Free existing profiles */
    for (int i = 0; i < recommender->profiles_count; i++) {
        if (recommender->profiles[i])
            FreeUserProfile(recommender->profiles[i]);
        free(recommender->profile_ids[i]);
    }
    free(recommender->profiles);
    free(recommender->profile_ids);
    
    recommender->profiles = NULL;
    recommender->profile_ids = NULL;
    recommender->profiles_count = 0;
    recommender->current_profile = NULL;
    
    /* Get profiles directory */
    char *profiles_dir = GetProfilesDirectory();
    if (!profiles_dir) {
        vlc_mutex_unlock(&recommender->lock);
        return VLC_EGENERIC;
    }
    
    /* Open directory */
    DIR *dir = vlc_opendir(profiles_dir);
    if (!dir) {
        free(profiles_dir);
        vlc_mutex_unlock(&recommender->lock);
        return VLC_EGENERIC;
    }
    
    /* Read directory entries */
    const char *ext = PROFILE_FILE_EXT;
    int ext_len = strlen(ext);
    int count = 0;
    
    struct dirent *entry;
    while ((entry = (struct dirent *)vlc_readdir(dir)) != NULL) {
        /* Check if file has the correct extension */
        char *name = entry->d_name;
        int name_len = strlen(name);
        
        if (name_len <= ext_len ||
            strcasecmp(name + name_len - ext_len, ext) != 0)
            continue;
            
        /* Extract profile ID from filename */
        char *profile_id = strndup(name, name_len - ext_len);
        if (!profile_id)
            continue;
            
        /* Get full path */
        char *profile_path;
        if (asprintf(&profile_path, "%s/%s", profiles_dir, name) < 0) {
            free(profile_id);
            continue;
        }
        
        /* Open profile file */
        FILE *fp = vlc_fopen(profile_path, "r");
        if (!fp) {
            free(profile_id);
            free(profile_path);
            continue;
        }
        
        /* Create new profile */
        ai_user_profile_t *profile = CreateUserProfile(profile_id, profile_id);
        if (!profile) {
            free(profile_id);
            free(profile_path);
            fclose(fp);
            continue;
        }
        
        /* Parse file */
        char line[MAX_LINE_LENGTH];
        enum {
            SECTION_NONE,
            SECTION_PROFILE,
            SECTION_GENRE_PREFS,
            SECTION_CREATOR_PREFS,
            SECTION_FEATURE_PREFS,
            SECTION_HISTORY
        } section = SECTION_NONE;
        
        while (fgets(line, sizeof(line), fp) != NULL) {
            /* Remove newline */
            char *nl = strchr(line, '\n');
            if (nl) *nl = '\0';
            
            /* Skip empty lines and comments */
            if (line[0] == '\0' || line[0] == '#')
                continue;
                
            /* Check for section markers */
            if (line[0] == '[') {
                char *end = strchr(line, ']');
                if (end) {
                    *end = '\0';
                    char *section_name = line + 1;
                    
                    if (!strcasecmp(section_name, "Profile"))
                        section = SECTION_PROFILE;
                    else if (!strcasecmp(section_name, "GenrePreferences"))
                        section = SECTION_GENRE_PREFS;
                    else if (!strcasecmp(section_name, "CreatorPreferences"))
                        section = SECTION_CREATOR_PREFS;
                    else if (!strcasecmp(section_name, "FeaturePreferences"))
                        section = SECTION_FEATURE_PREFS;
                    else if (!strcasecmp(section_name, "ViewingHistory"))
                        section = SECTION_HISTORY;
                }
                continue;
            }
            
            /* Parse key-value pairs based on current section */
            char *key, *value;
            if (ParseKeyValue(line, &key, &value) < 0)
                continue;
                
            switch (section) {
                case SECTION_PROFILE:
                    if (!strcasecmp(key, "Name")) {
                        free(profile->name);
                        profile->name = strdup(value);
                    } else if (!strcasecmp(key, "CreationTime")) {
                        profile->creation_time = atol(value);
                    } else if (!strcasecmp(key, "LastUpdated")) {
                        profile->last_updated = atol(value);
                    }
                    break;
                    
                case SECTION_GENRE_PREFS:
                    {
                        float weight = atof(value);
                        if (weight > 0.0f) {
                            UpdateGenrePreference(profile, key, weight - 0.5f);
                        }
                    }
                    break;
                    
                case SECTION_CREATOR_PREFS:
                    {
                        float weight = atof(value);
                        if (weight > 0.0f) {
                            UpdateCreatorPreference(profile, key, weight - 0.5f);
                        }
                    }
                    break;
                    
                case SECTION_FEATURE_PREFS:
                    {
                        float weight = atof(value);
                        if (weight > 0.0f) {
                            UpdateFeaturePreference(profile, key, weight - 0.5f);
                        }
                    }
                    break;
                    
                case SECTION_HISTORY:
                    {
                        /* Parse viewing history record */
                        /* Format: media_id: timestamp, completion_ratio */
                        char *timestamp_str = strchr(value, ',');
                        if (timestamp_str) {
                            *timestamp_str = '\0';
                            timestamp_str++;
                            
                            /* Trim whitespace */
                            while (*timestamp_str && isspace(*timestamp_str)) timestamp_str++;
                            
                            time_t timestamp = atol(value);
                            float completion = atof(timestamp_str);
                            
                            /* Create and add viewing record */
                            ai_viewing_record_t *record = CreateViewingRecord(key, completion);
                            if (record) {
                                record->timestamp = timestamp;
                                
                                /* Add to profile without checking for duplicates */
                                ai_viewing_record_t **new_history = realloc(profile->viewing_history,
                                    (profile->viewing_history_count + 1) * sizeof(ai_viewing_record_t*));
                                    
                                if (new_history) {
                                    profile->viewing_history = new_history;
                                    profile->viewing_history[profile->viewing_history_count++] = record;
                                } else {
                                    FreeViewingRecord(record);
                                }
                            }
                        }
                    }
                    break;
                    
                default:
                    /* Unknown section, ignore */
                    break;
            }
        }
        
        /* Close file */
        fclose(fp);
        free(profile_path);
        
        /* Add profile to recommender */
        void *new_profiles = realloc(recommender->profiles,
            (count + 1) * sizeof(ai_user_profile_t*));
        void *new_ids = realloc(recommender->profile_ids,
            (count + 1) * sizeof(char*));
            
        if (!new_profiles || !new_ids) {
            FreeUserProfile(profile);
            free(profile_id);
            continue;
        }
        
        recommender->profiles = new_profiles;
        recommender->profile_ids = new_ids;
        recommender->profiles[count] = profile;
        recommender->profile_ids[count] = profile_id;
        
        /* Set as current profile if it's the default or first one */
        if (!recommender->current_profile || !strcmp(profile_id, "default")) {
            recommender->current_profile = profile;
        }
        
        count++;
    }
    
    /* Close directory */
    closedir(dir);
    free(profiles_dir);
    
    recommender->profiles_count = count;
    vlc_mutex_unlock(&recommender->lock);
    
    return VLC_SUCCESS;
}