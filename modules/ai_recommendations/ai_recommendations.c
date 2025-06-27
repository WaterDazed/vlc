/*****************************************************************************
 * ai_recommendations.c: AI-Powered Media Recommendation module for VLC
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
 * AI-Powered Media Recommendation module for VLC
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "custom_compat.h"
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_threads.h>
#include <vlc_playlist.h>
#include <vlc_input_item.h>
#include <vlc_media_library.h>

/* Headers for the recommendation module */
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#include "ai_recommendations.h"
#include "profile_manager.h"
#include "feature_extractor.h"
#include "recommendation_engine.h"
#include "metadata_analyzer.h"

/* Forward declarations for callbacks */
static void playlist_items_reset(vlc_playlist_t *, vlc_playlist_item_t *const[], size_t, void *);
static void playlist_item_added(vlc_playlist_t *, size_t, vlc_playlist_item_t *const[], size_t, void *);
static void playlist_item_updated(vlc_playlist_t *, size_t, vlc_playlist_item_t *const[], size_t, void *);
static void playlist_playback_repeat_changed(vlc_playlist_t *, enum vlc_playlist_playback_repeat, void *);
static void playlist_playback_order_changed(vlc_playlist_t *, enum vlc_playlist_playback_order, void *);
static void playlist_current_index_changed(vlc_playlist_t *, ssize_t, void *);
static void OnMediaAdded(void *, const vlc_ml_event_t *);
static void OnMediaUpdated(vlc_medialibrary_t *, input_item_t *, void *);
static void OnMediaDeleted(vlc_medialibrary_t *, int64_t, void *);
static void OnBackgroundTasksCompleted(vlc_medialibrary_t *, void *);
static void *BackgroundWorker(void *);
static void ScanTimerCallback(void *);
static void ai_recommender_cleanup(ai_recommender_t *, vlc_playlist_t *, vlc_medialibrary_t *);
static void RecalculateRecommendations(ai_recommender_t *);
static vlc_playlist_item_t *CreateRecommendation(ai_recommender_t *, input_item_t *);
static int CompareRecommendationScores(const void *, const void *);
static void UpdateUserPreferences(ai_recommender_t *, input_item_t *, bool);
static bool IsItemInRecentHistory(ai_recommender_t *, const input_item_t *);
static void TriggerRecommendationUpdate(ai_recommender_t *);

/* Define module capability and callbacks */
/* Open and Close functions are declared in ai_recommendations.h */

/* Module definition */
#define CONFIG_PREFIX "ai-recommendations-"

#define MODULE_STRING "ai_recommendations"

vlc_module_begin()
    set_shortname(N_("AI Recommendations"))
    set_description(N_("AI-powered content recommendations based on viewing habits"))
    set_subcategory(SUBCAT_INTERFACE_CONTROL)
    set_capability("interface", 0)
    set_callbacks(Open_ai_recommendations, Close_ai_recommendations)
    
    /* Module configuration options */
    add_bool(CONFIG_PREFIX "enabled", true, N_("Enable AI recommendations"), 
             N_("Enable or disable AI-powered content recommendations"))
    add_integer(CONFIG_PREFIX "max-recommendations", 5, 
                N_("Maximum number of recommendations"), 
                N_("Maximum number of recommendations to display"))
    add_integer(CONFIG_PREFIX "update-interval", 60, 
                N_("Recommendation update interval (seconds)"), 
                N_("How often to update recommendations"))
    add_bool(CONFIG_PREFIX "auto-update", true, 
             N_("Auto-update recommendations"), 
             N_("Automatically update recommendations"))
    add_string(CONFIG_PREFIX "profile-dir", NULL, 
               N_("User profile directory"), 
               N_("Directory to store user profile data"))
vlc_module_end()

/* Main entry point for the module */
int Open_ai_recommendations(vlc_object_t *obj)
{
    ai_recommender_t *p_sys;
    intf_thread_t *intf = (intf_thread_t *)obj;
    vlc_playlist_t *playlist = NULL;
    vlc_medialibrary_t *ml = NULL;
    
    /* Check if the module is enabled */
    bool enabled = var_InheritBool(obj, CFG_PREFIX "enabled");
    if (!enabled) {
        msg_Info(obj, "AI Recommendation Engine is disabled in settings");
        return VLC_EGENERIC;
    }
    
    msg_Info(obj, "Initializing AI Recommendation Engine");
    
    playlist = vlc_intf_GetMainPlaylist(intf);
    if (playlist == NULL) {
        msg_Warn(obj, "Unable to get playlist, continuing with limited functionality");
        /* We'll continue without playlist integration */
    }
    
    ml = vlc_ml_instance_get(obj);
    if (ml == NULL) {
        msg_Warn(obj, "Unable to get media library, continuing with limited functionality");
        /* We'll continue without media library integration */
    }
    
    p_sys = calloc(1, sizeof(ai_recommender_t));
    if (p_sys == NULL) {
        msg_Err(obj, "Failed to allocate recommender system");
        return VLC_ENOMEM;
    }
    
    intf->p_sys = (intf_sys_t *)p_sys;
    p_sys->obj = obj;
    p_sys->playlist = playlist;
    p_sys->ml = ml;
    p_sys->playlist_listener = NULL;
    p_sys->ml_listener = NULL;
    
    /* Initialize locks and threading primitives */
    vlc_mutex_init(&p_sys->lock);
    p_sys->processing_active = false;
    
    /* Load configuration */
    p_sys->processing_mode = var_InheritInteger(obj, CFG_PREFIX "processing-mode");
    p_sys->max_memory_mb = var_InheritInteger(obj, CFG_PREFIX "max-memory");
    p_sys->analyze_subtitles = var_InheritBool(obj, CFG_PREFIX "analyze-subtitles");
    p_sys->scan_interval = var_InheritInteger(obj, CFG_PREFIX "scan-interval");
    p_sys->num_recommendations = var_InheritInteger(obj, CFG_PREFIX "num-recommendations");
    
    /* Initialize arrays for profiles and recommendations */
    p_sys->profiles = NULL;
    p_sys->profiles_count = 0;
    p_sys->profile_ids = NULL;
    p_sys->recommendation_cache = NULL;
    p_sys->cache_ids = NULL;
    p_sys->recommendation_cache_count = 0;
    
    /* Load user profiles */
    if (LoadUserProfiles(p_sys) != VLC_SUCCESS) {
        msg_Warn(obj, "Failed to load user profiles, creating default");
        /* We'll create a default profile below */
    }
    
    /* Create default profile if none exists */
    if (p_sys->profiles_count == 0 || p_sys->current_profile == NULL) {
        p_sys->current_profile = CreateUserProfile("default", "Default Profile");
        if (p_sys->current_profile == NULL) {
            msg_Err(obj, "Failed to create default user profile");
            goto error;
        }
        
        /* Add to profiles array */
        p_sys->profiles = malloc(sizeof(ai_user_profile_t*));
        p_sys->profile_ids = malloc(sizeof(char*));
        if (!p_sys->profiles || !p_sys->profile_ids) {
            msg_Err(obj, "Failed to allocate profile arrays");
            goto error;
        }
        
        p_sys->profiles[0] = p_sys->current_profile;
        p_sys->profile_ids[0] = strdup("default");
        if (!p_sys->profile_ids[0]) {
            msg_Err(obj, "Failed to duplicate profile ID");
            goto error;
        }
        p_sys->profiles_count = 1;
        
        /* Save the new profile */
        SaveUserProfiles(p_sys);
    }
    
    /* Register for playlist events if playlist is available */
    if (playlist) {
        static const struct vlc_playlist_callbacks cbs = {
            .on_items_reset = playlist_items_reset,
            .on_items_added = playlist_item_added,
            .on_items_updated = playlist_item_updated,
            .on_playback_repeat_changed = playlist_playback_repeat_changed,
            .on_playback_order_changed = playlist_playback_order_changed,
            .on_current_index_changed = playlist_current_index_changed,
        };
        
        p_sys->playlist_listener = vlc_playlist_AddListener(playlist, &cbs, p_sys, NULL);
        if (p_sys->playlist_listener == NULL) {
            msg_Warn(obj, "Failed to add playlist listener, continuing with limited functionality");
            /* We'll continue without playlist event handling */
        }
    }
    
    /* Register for media library events if media library is available */
    if (ml) {
        p_sys->ml_listener = vlc_ml_event_register_callback(ml, 
                                              (vlc_ml_callback_t)OnMediaAdded,
                                              p_sys);
        if (p_sys->ml_listener == NULL) {
            msg_Warn(obj, "Failed to register media library callback, continuing with limited functionality");
            /* We'll continue without media library event handling */
        }
    }
    
    /* UI integration removed */
    
    /* Start background worker thread */
    if (vlc_clone(&p_sys->worker_thread, BackgroundWorker, p_sys) != VLC_SUCCESS) {
        msg_Err(obj, "Failed to start worker thread");
        goto error;
    }
    p_sys->processing_active = true;
    
    /* Start periodic scan timer */
    if (vlc_timer_create(&p_sys->scan_timer, ScanTimerCallback, p_sys) != VLC_SUCCESS) {
        msg_Err(obj, "Failed to create scan timer");
        goto error;
    }
    
    /* Schedule first scan */
    vlc_timer_schedule(p_sys->scan_timer, false, 
                      p_sys->scan_interval * 60 * 1000,  /* Initial delay in ms */
                      p_sys->scan_interval * 60 * 1000); /* Repeat interval in ms */
    
    msg_Info(obj, "AI Recommendation Engine initialized successfully");
    return VLC_SUCCESS;
    
error:
    if (p_sys) {
        /* Safe cleanup function with proper null checks */
        ai_recommender_cleanup(p_sys, playlist, ml);
        free(p_sys);
    }
    
    return VLC_EGENERIC;
}

/* Safe cleanup helper function to reuse in error paths and Close function */
static void ai_recommender_cleanup(ai_recommender_t *p_sys, vlc_playlist_t *playlist, vlc_medialibrary_t *ml)
{
    if (!p_sys)
        return;
        
    /* Stop timer first to prevent callbacks */
    if (p_sys->scan_timer != NULL) {
        vlc_timer_destroy(p_sys->scan_timer);
        p_sys->scan_timer = NULL;
    }
    
    /* Stop worker thread */
    if (p_sys->processing_active) {
        vlc_cancel(p_sys->worker_thread);
        vlc_join(p_sys->worker_thread, NULL);
        p_sys->processing_active = false;
    }
    
    /* Remove event listeners - check both playlist and media library */
    if (p_sys->playlist_listener != NULL && p_sys->playlist != NULL) {
        vlc_playlist_RemoveListener(p_sys->playlist, p_sys->playlist_listener);
        p_sys->playlist_listener = NULL;
    }
    else if (p_sys->playlist_listener != NULL && playlist != NULL) {
        vlc_playlist_RemoveListener(playlist, p_sys->playlist_listener);
        p_sys->playlist_listener = NULL;
    }
    
    if (p_sys->ml_listener != NULL && p_sys->ml != NULL) {
        vlc_ml_event_unregister_callback(p_sys->ml, p_sys->ml_listener);
        p_sys->ml_listener = NULL;
    }
    else if (p_sys->ml_listener != NULL && ml != NULL) {
        vlc_ml_event_unregister_callback(ml, p_sys->ml_listener);
        p_sys->ml_listener = NULL;
    }
    
    /* Try to save profiles if they exist */
    if (p_sys->profiles_count > 0 && p_sys->profiles != NULL)
        SaveUserProfiles(p_sys);
    
    /* Free the current profile if it's not in the profiles array */
    bool current_in_array = false;
    
    /* Free profiles */
    for (int i = 0; i < p_sys->profiles_count; i++) {
        if (p_sys->profiles && p_sys->profiles[i]) {
            if (p_sys->profiles[i] == p_sys->current_profile)
                current_in_array = true;
                
            FreeUserProfile(p_sys->profiles[i]);
            p_sys->profiles[i] = NULL;
        }
        
        if (p_sys->profile_ids && p_sys->profile_ids[i]) {
            free(p_sys->profile_ids[i]);
            p_sys->profile_ids[i] = NULL;
        }
    }
    
    /* If current profile wasn't in the array, free it separately */
    if (!current_in_array && p_sys->current_profile) {
        FreeUserProfile(p_sys->current_profile);
        p_sys->current_profile = NULL;
    }
    
    free(p_sys->profiles);
    p_sys->profiles = NULL;
    
    free(p_sys->profile_ids);
    p_sys->profile_ids = NULL;
}

/* Module cleanup function */
void Close_ai_recommendations(vlc_object_t *obj)
{
    intf_thread_t *intf = (intf_thread_t *)obj;
    if (!intf || !intf->p_sys)
        return;
        
    ai_recommender_t *p_sys = (ai_recommender_t *)intf->p_sys;
    
    msg_Info(obj, "Shutting down AI Recommendation Engine");
    
    /* Save profiles before closing - extra safety */
    SaveUserProfiles(p_sys);
    
    /* Use the common cleanup function */
    ai_recommender_cleanup(p_sys, NULL, NULL);
    
    /* Free recommendation cache separately */
    ClearRecommendationCache(p_sys);
    
    /* Free the module struct itself */
    free(p_sys);
    intf->p_sys = NULL;
    
    msg_Info(obj, "AI Recommendation Engine shut down");
}

/**
 * Playlist callback: item added
 */
static void playlist_item_added(vlc_playlist_t *playlist, size_t index,
                           vlc_playlist_item_t *const items[], size_t count, void *data)
{
    ai_recommender_t *p_sys = (ai_recommender_t *)data;
    (void)playlist; /* Unused */
    (void)index;    /* Unused */
    
    vlc_mutex_lock(&p_sys->lock);
    for (size_t i = 0; i < count; i++) {
        input_item_t *item = vlc_playlist_item_GetMedia(items[i]);
        if (item) {
            msg_Dbg(p_sys->obj, "Item added to playlist: %s", 
                   item->psz_name ? item->psz_name : "(unknown)");
            /* No immediate action needed, features will be extracted during scan */
        }
    }
    vlc_mutex_unlock(&p_sys->lock);
}

/**
 * Playlist callback: item updated
 */
static void playlist_item_updated(vlc_playlist_t *playlist, size_t index,
                             vlc_playlist_item_t *const items[], size_t count, void *data)
{
    ai_recommender_t *p_sys = (ai_recommender_t *)data;
    (void)playlist; /* Unused */
    (void)index;    /* Unused */
    (void)items;    /* Unused */
    (void)count;    /* Unused */
    
    vlc_mutex_lock(&p_sys->lock);
    /* Item updated in playlist, nothing specific to do here */
    vlc_mutex_unlock(&p_sys->lock);
}

/**
 * Playlist callback: items reset
 */
static void playlist_items_reset(vlc_playlist_t *playlist, 
                            vlc_playlist_item_t *const items[], size_t count, 
                            void *data)
{
    ai_recommender_t *p_sys = (ai_recommender_t *)data;
    (void)playlist; /* Unused */
    (void)items;    /* Unused */
    
    vlc_mutex_lock(&p_sys->lock);
    /* Playlist was reset, clear any transient recommendations */
    msg_Dbg(p_sys->obj, "Playlist reset (%zu items)", count);
    vlc_mutex_unlock(&p_sys->lock);
}

/**
 * Playlist callback: repeat mode changed
 */
static void playlist_playback_repeat_changed(vlc_playlist_t *playlist,
                                        enum vlc_playlist_playback_repeat repeat,
                                        void *data)
{
    ai_recommender_t *p_sys = (ai_recommender_t *)data;
    (void)playlist; /* Unused */
    (void)repeat;   /* Unused */
    
    vlc_mutex_lock(&p_sys->lock);
    /* Repeat mode changed, nothing specific to do here */
    vlc_mutex_unlock(&p_sys->lock);
}

/**
 * Playlist callback: playback order changed
 */
static void playlist_playback_order_changed(vlc_playlist_t *playlist,
                                       enum vlc_playlist_playback_order order,
                                       void *data)
{
    ai_recommender_t *p_sys = (ai_recommender_t *)data;
    (void)playlist; /* Unused */
    (void)order;    /* Unused */
    
    vlc_mutex_lock(&p_sys->lock);
    /* Playback order changed, nothing specific to do here */
    vlc_mutex_unlock(&p_sys->lock);
}

/**
 * Playlist callback: current item changed
 * This is a key event for the recommendation engine
 */
static void playlist_current_index_changed(vlc_playlist_t *playlist,
                                      ssize_t index, void *data)
{
    ai_recommender_t *p_sys = (ai_recommender_t *)data;
    
    if (index < 0) {
        /* No current item */
        return;
    }
    
    /* Get the current item */
    vlc_playlist_item_t *pl_item = vlc_playlist_Get(playlist, index);
    if (!pl_item)
        return;
        
    input_item_t *item = vlc_playlist_item_GetMedia(pl_item);
    if (!item)
        return;
        
    char *uri = input_item_GetURI(item);
    if (!uri)
        return;
        
    msg_Dbg(p_sys->obj, "Current item changed: %s", 
           item->psz_name ? item->psz_name : uri);
            
    /* Check if we need to update the profile */
    if (p_sys->current_profile) {
        /* We only update with partial completion for now */
        /* In a real implementation, we'd track viewing duration in a callback */
        vlc_mutex_lock(&p_sys->lock);
        UpdateUserProfile(p_sys, p_sys->current_profile, item, 0.5f);
        vlc_mutex_unlock(&p_sys->lock);
    }
    
    /* Generate recommendations for this item if not already in cache */
    bool found_in_cache = false;
    
    vlc_mutex_lock(&p_sys->lock);
    
    /* Check if recommendations for this item are in cache */
    for (int i = 0; i < p_sys->recommendation_cache_count; i++) {
        if (p_sys->cache_ids[i] && strcmp(p_sys->cache_ids[i], uri) == 0) {
            found_in_cache = true;
            
            /* Update the UI with these recommendations */
            // UI integration removed
            // UpdateRecommendationDisplay(p_sys, p_sys->recommendation_cache[i]);
            break;
        }
    }
    
    /* If not in cache, generate new recommendations */
    if (!found_in_cache) {
        /* Generate recommendations */
        ai_recommendation_set_t *recs = GenerateRecommendations(
            p_sys, item, p_sys->num_recommendations);
            
        if (recs) {
            /* Display them */
            // UI integration removed
            // UpdateRecommendationDisplay(p_sys, recs);
            
            /* Cache is updated inside GenerateRecommendations */
        }
    }
    
    vlc_mutex_unlock(&p_sys->lock);
    free(uri);
}

/**
 * Media library callback: media added
 */
static void OnMediaAdded(void *data, const vlc_ml_event_t *event)
{
    ai_recommender_t *p_sys = (ai_recommender_t *)data;
    
    if (!event || event->i_type != VLC_ML_EVENT_MEDIA_ADDED)
        return;
    
    const vlc_ml_media_t *media = event->creation.p_media;
    if (!media)
        return;
        
    vlc_mutex_lock(&p_sys->lock);
    /* Media was added to the library, schedule for analysis */
    p_sys->processing_active = true;
    vlc_mutex_unlock(&p_sys->lock);
}

/**
 * Media library callback: media updated
 */
static void OnMediaUpdated(vlc_medialibrary_t *ml, input_item_t *item, void *data)
{
    ai_recommender_t *p_sys = (ai_recommender_t *)data;
    (void)ml; /* Unused */
    
    if (!item)
        return;
        
    vlc_mutex_lock(&p_sys->lock);
    /* Media was updated in the library, update cache if needed */
    ClearRecommendationCache(p_sys);
    vlc_mutex_unlock(&p_sys->lock);
}

/**
 * Media library callback: media deleted
 */
static void OnMediaDeleted(vlc_medialibrary_t *ml, int64_t id, void *data)
{
    ai_recommender_t *p_sys = (ai_recommender_t *)data;
    (void)ml; /* Unused */
    (void)id; /* Unused */
    
    vlc_mutex_lock(&p_sys->lock);
    /* Media was deleted from the library, cleanup cache */
    ClearRecommendationCache(p_sys);
    vlc_mutex_unlock(&p_sys->lock);
}

/**
 * Media library callback: background tasks completed
 */
static void OnBackgroundTasksCompleted(vlc_medialibrary_t *ml, void *data)
{
    ai_recommender_t *p_sys = (ai_recommender_t *)data;
    (void)ml; /* Unused */
    
    vlc_mutex_lock(&p_sys->lock);
    /* Background tasks in media library completed */
    msg_Dbg(p_sys->obj, "Media library background tasks completed");
    vlc_mutex_unlock(&p_sys->lock);
}

/**
 * Background worker thread
 * This thread handles processor-intensive tasks like feature extraction
 */
static void *BackgroundWorker(void *data)
{
    ai_recommender_t *p_sys = (ai_recommender_t *)data;
    
    vlc_thread_set_name("vlc-ai-recommender");
    
    while (vlc_object_alive_hold(p_sys->obj)) {
        vlc_tick_t sleep_time = VLC_TICK_FROM_SEC(10); /* Default sleep time */
        
        vlc_mutex_lock(&p_sys->lock);
        
        if (p_sys->processing_active) {
            /* Here we could scan the media library and extract features */
            /* For performance reasons, we'll just set a flag */
            msg_Dbg(p_sys->obj, "Background processing active");
            p_sys->processing_active = false;
            
            /* Request UI update with any new recommendations */
            /* This could be triggered after processing new items */
        }
        
        vlc_mutex_unlock(&p_sys->lock);
        
        /* Sleep for a bit before next cycle */
        vlc_tick_sleep(sleep_time);
    }
    
    return NULL;
}

/**
 * Periodic scan timer callback
 */
static void ScanTimerCallback(void *data)
{
    ai_recommender_t *p_sys = (ai_recommender_t *)data;
    
    vlc_mutex_lock(&p_sys->lock);
    
    /* Update user profiles based on new data */
    SaveUserProfiles(p_sys);
    
    /* Trigger a new media library scan cycle */
    p_sys->processing_active = true;
    
    vlc_mutex_unlock(&p_sys->lock);
}

/**
 * Recalculate recommendations based on current user profile
 */
static void RecalculateRecommendations(ai_recommender_t *p_sys)
{
    if (!p_sys || !p_sys->current_profile)
        return;
        
    vlc_mutex_lock(&p_sys->lock);
    
    /* Clear existing cache */
    ClearRecommendationCache(p_sys);
    
    /* In a real implementation, we would:
     * 1. Scan the media library
     * 2. Extract features from items
     * 3. Compare with user preferences
     * 4. Generate new recommendations
     */
    
    msg_Dbg(p_sys->obj, "Recalculating recommendations for profile: %s", 
            p_sys->current_profile->name);
    
    /* For now, we'll just set a flag to indicate processing is needed */
    p_sys->processing_active = true;
    
    vlc_mutex_unlock(&p_sys->lock);
}

/**
 * Create a recommendation item from a media item
 */
static vlc_playlist_item_t *CreateRecommendation(ai_recommender_t *p_sys, input_item_t *media)
{
    if (!p_sys || !media || !p_sys->playlist)
        return NULL;
        
    /* Create a playlist item from the input item */
    vlc_playlist_item_t *item = vlc_playlist_item_New(media);
    if (!item) {
        msg_Err(p_sys->obj, "Failed to create playlist item for recommendation");
        return NULL;
    }
    
    return item;
}

/**
 * Compare function for sorting recommendations by score
 */
static int CompareRecommendationScores(const void *a, const void *b)
{
    const ai_recommendation_item_t *item_a = *(const ai_recommendation_item_t **)a;
    const ai_recommendation_item_t *item_b = *(const ai_recommendation_item_t **)b;
    
    /* Sort in descending order (higher scores first) */
    if (item_a->score > item_b->score)
        return -1;
    else if (item_a->score < item_b->score)
        return 1;
    else
        return 0;
}

/**
 * Update user preferences based on media interaction
 */
static void UpdateUserPreferences(ai_recommender_t *p_sys, input_item_t *media, bool explicit_action)
{
    if (!p_sys || !media || !p_sys->current_profile)
        return;
        
    vlc_mutex_lock(&p_sys->lock);
    
    /* Extract metadata from the item */
    char *genre = NULL;
    char *artist = NULL;
    
    /* Get metadata */
    char *meta_genre = input_item_GetMeta(media, vlc_meta_Genre);
    char *meta_artist = input_item_GetMeta(media, vlc_meta_Artist);
    
    if (meta_genre)
        genre = strdup(meta_genre);
        
    if (meta_artist)
        artist = strdup(meta_artist);
        
    /* Update profile preferences */
    if (genre) {
        /* Find existing genre preference or add new one */
        bool found = false;
        for (int i = 0; i < p_sys->current_profile->genre_preferences_count; i++) {
            if (strcmp(p_sys->current_profile->genre_preferences[i].key, genre) == 0) {
                /* Update existing preference */
                float increment = explicit_action ? 0.2f : 0.1f;
                p_sys->current_profile->genre_preferences[i].value += increment;
                if (p_sys->current_profile->genre_preferences[i].value > 1.0f)
                    p_sys->current_profile->genre_preferences[i].value = 1.0f;
                found = true;
                break;
            }
        }
        
        /* Add new genre preference if not found */
        if (!found) {
            /* Implementation would add a new genre preference here */
            msg_Dbg(p_sys->obj, "Would add new genre preference: %s", genre);
        }
    }
    
    /* Similar logic for artist preferences */
    if (artist) {
        /* Implementation would update artist preferences here */
        msg_Dbg(p_sys->obj, "Would update artist preference: %s", artist);
    }
    
    /* Save updated profile */
    SaveUserProfiles(p_sys);
    
    /* Free allocated strings */
    free(genre);
    free(artist);
    free(meta_genre);
    free(meta_artist);
    
    vlc_mutex_unlock(&p_sys->lock);
}

/**
 * Check if an item is in the user's recent history
 */
static bool IsItemInRecentHistory(ai_recommender_t *p_sys, const input_item_t *media)
{
    if (!p_sys || !media || !p_sys->current_profile)
        return false;
        
    bool found = false;
    /* Cast away const - this is safe as GetURI doesn't modify the item */
    char *uri = input_item_GetURI((input_item_t *)media);
    if (!uri)
        return false;
        
    vlc_mutex_lock(&p_sys->lock);
    
    /* Check viewing history */
    for (int i = 0; i < p_sys->current_profile->viewing_history_count; i++) {
        if (p_sys->current_profile->viewing_history[i] && 
            p_sys->current_profile->viewing_history[i]->media_id &&
            strcmp(p_sys->current_profile->viewing_history[i]->media_id, uri) == 0) {
            found = true;
            break;
        }
    }
    
    vlc_mutex_unlock(&p_sys->lock);
    free(uri);
    
    return found;
}

/**
 * Trigger an update of recommendations
 */
static void TriggerRecommendationUpdate(ai_recommender_t *p_sys)
{
    if (!p_sys)
        return;
        
    vlc_mutex_lock(&p_sys->lock);
    
    /* Set flag to trigger processing in background thread */
    p_sys->processing_active = true;
    
    /* In a real implementation, we might also:
     * 1. Signal a condition variable to wake the worker thread
     * 2. Schedule immediate processing
     */
    
    vlc_mutex_unlock(&p_sys->lock);
}