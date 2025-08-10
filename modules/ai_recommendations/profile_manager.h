/*****************************************************************************
 * profile_manager.h: AI-Powered Media Recommendation user profile manager
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

#ifndef VLC_AI_PROFILE_MANAGER_H
#define VLC_AI_PROFILE_MANAGER_H

#include <vlc_common.h>
#include <vlc_fs.h>
#include <vlc_configuration.h>
#include <vlc_charset.h>
#include <vlc_strings.h>
#include <vlc_memstream.h>
#include <vlc_arrays.h>
#include <vlc_playlist.h>
#include <vlc_input_item.h>
#include <vlc_media_library.h>

#include "ai_recommendations.h"

/**
 * @file
 * This file implements user profile management for the AI recommendation engine
 */

/* Profile storage paths */
#define PROFILES_DIR_NAME "ai_recommendations"
#define PROFILE_FILE_EXT ".json"

/**
 * Create a new user profile
 *
 * @param id Unique identifier for the profile
 * @param name Display name for the profile
 * @return New user profile or NULL on error
 */
ai_user_profile_t *CreateUserProfile(const char *id, const char *name);

/**
 * Update a user profile with information about media viewing
 *
 * @param recommender The recommender module instance
 * @param profile The user profile to update
 * @param media The media item that was viewed
 * @param completion Completion ratio (0.0 to 1.0) of the media
 */
void UpdateUserProfile(ai_recommender_t *recommender, 
                     ai_user_profile_t *profile, 
                     input_item_t *media, 
                     float completion);

/**
 * Add a viewing record to a user profile
 *
 * @param profile The user profile to update
 * @param record The viewing record to add
 */
void AddViewingRecord(ai_user_profile_t *profile, ai_viewing_record_t *record);

/**
 * Update genre preference in user profile
 *
 * @param profile The user profile to update
 * @param genre The genre to update preference for
 * @param weight Preference weight (0.0 to 1.0)
 */
void UpdateGenrePreference(ai_user_profile_t *profile, const char *genre, float weight);

/**
 * Update creator preference in user profile
 *
 * @param profile The user profile to update
 * @param creator The creator to update preference for
 * @param weight Preference weight (0.0 to 1.0)
 */
void UpdateCreatorPreference(ai_user_profile_t *profile, const char *creator, float weight);

/**
 * Update feature preference in user profile
 *
 * @param profile The user profile to update
 * @param feature The feature to update preference for
 * @param weight Preference weight (0.0 to 1.0)
 */
void UpdateFeaturePreference(ai_user_profile_t *profile, const char *feature, float weight);

/**
 * Get the preference weight for a genre
 *
 * @param profile The user profile to query
 * @param genre The genre to get preference for
 * @return Preference weight (0.0 to 1.0), defaults to 0.5 if not set
 */
float GetGenrePreference(ai_user_profile_t *profile, const char *genre);

/**
 * Get the preference weight for a creator
 *
 * @param profile The user profile to query
 * @param creator The creator to get preference for
 * @return Preference weight (0.0 to 1.0), defaults to 0.5 if not set
 */
float GetCreatorPreference(ai_user_profile_t *profile, const char *creator);

/**
 * Get the preference weight for a feature
 *
 * @param profile The user profile to query
 * @param feature The feature to get preference for
 * @return Preference weight (0.0 to 1.0), defaults to 0.5 if not set
 */
float GetFeaturePreference(ai_user_profile_t *profile, const char *feature);

/**
 * Get recently viewed media IDs
 *
 * @param profile The user profile to query
 * @param count Maximum number of items to return
 * @return Array of media IDs, caller must free with free()
 */
char **GetRecentlyViewed(ai_user_profile_t *profile, int count);

/**
 * Get frequently viewed media IDs
 *
 * @param profile The user profile to query
 * @param count Maximum number of items to return
 * @return Array of media IDs, caller must free with free()
 */
char **GetFrequentlyViewed(ai_user_profile_t *profile, int count);

/**
 * Load user profiles from storage
 *
 * @param recommender The recommender module instance
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
int LoadUserProfiles(ai_recommender_t *recommender);

/**
 * Save user profiles to storage
 *
 * @param recommender The recommender module instance
 */
void SaveUserProfiles(ai_recommender_t *recommender);

/**
 * Free a user profile and its resources
 *
 * @param profile The profile to free
 */
void FreeUserProfile(ai_user_profile_t *profile);

/**
 * Free a viewing record
 *
 * @param record The viewing record to free
 */
void FreeViewingRecord(ai_viewing_record_t *record);

/**
 * Create a viewing record
 *
 * @param media_id The ID of the media that was viewed
 * @param completion_ratio The completion ratio of the viewing
 * @return Newly created viewing record or NULL on error
 */
ai_viewing_record_t *CreateViewingRecord(const char *media_id, float completion_ratio);

#endif /* VLC_AI_PROFILE_MANAGER_H */