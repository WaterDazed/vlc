/*****************************************************************************
 * recommendation_engine.c: AI-Powered Media Recommendation engine
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
 * AI-Powered Media Recommendation engine core implementation
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include "custom_compat.h"
#include <vlc_plugin.h>
#include <vlc_playlist.h>
#include <vlc_threads.h>
#include <vlc_arrays.h>
#include <vlc_input_item.h>
#include <vlc_media_library.h>
#include <vlc_memstream.h>
#include <vlc_tick.h>
#include <vlc_fs.h>

#include <time.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "ai_recommendations.h"
#include "recommendation_engine.h"
#include "metadata_analyzer.h"
#include "feature_extractor.h"
#include "profile_manager.h"

/* Constants for recommendation parameters */
#define MAX_RECOMMENDATIONS 50
#define MIN_SIMILARITY_THRESHOLD 0.15f
#define DEFAULT_DIVERSITY_FACTOR 0.3f
#define TIME_DECAY_FACTOR 0.1f
#define CACHE_EXPIRY_TIME (24 * 60 * 60)  /* 24 hours in seconds */

/**
 * Helper structure for sorting recommendations 
 */
typedef struct {
    ai_recommendation_item_t *item;
    float score;
    float diversity_score;
} recommendation_sort_item_t;

/**
 * Compare recommendations for sorting (descending order by score)
 */
static int compare_recommendations(const void *a, const void *b)
{
    const recommendation_sort_item_t *item_a = (const recommendation_sort_item_t *)a;
    const recommendation_sort_item_t *item_b = (const recommendation_sort_item_t *)b;
    
    /* Sort by score (higher scores first) */
    if (item_a->score > item_b->score)
        return -1;
    if (item_a->score < item_b->score)
        return 1;
    return 0;
}

/**
 * Create a new recommendation set
 */
ai_recommendation_set_t *CreateRecommendationSet(const char *reference_id)
{
    if (!reference_id)
        return NULL;
        
    ai_recommendation_set_t *set = calloc(1, sizeof(ai_recommendation_set_t));
    if (!set)
        return NULL;
        
    set->reference_id = strdup(reference_id);
    if (!set->reference_id) {
        free(set);
        return NULL;
    }
    
    set->items = NULL;
    set->items_count = 0;
    set->generation_time = time(NULL);
    
    return set;
}

/**
 * Add a recommendation item to a set
 */
int AddRecommendationItem(ai_recommendation_set_t *set, ai_recommendation_item_t *item)
{
    if (!set || !item)
        return VLC_EGENERIC;
        
    /* Expand the array */
    ai_recommendation_item_t **new_items = realloc(set->items, 
        (set->items_count + 1) * sizeof(ai_recommendation_item_t*));
        
    if (!new_items)
        return VLC_ENOMEM;
        
    set->items = new_items;
    set->items[set->items_count] = item;
    set->items_count++;
    
    return VLC_SUCCESS;
}

/**
 * Create a new recommendation item
 */
ai_recommendation_item_t *CreateRecommendationItem(input_item_t *item, float score,
                                                const char *explanation,
                                                ai_recommendation_type_t type)
{
    if (!item)
        return NULL;
        
    ai_recommendation_item_t *rec_item = calloc(1, sizeof(ai_recommendation_item_t));
    if (!rec_item)
        return NULL;
        
    rec_item->item = item;
    input_item_Hold(item);
    rec_item->score = score;
    
    if (explanation) {
        rec_item->explanation = strdup(explanation);
        if (!rec_item->explanation) {
            /* Handle memory allocation failure */
            input_item_Release(item);
            free(rec_item);
            return NULL;
        }
    }
    
    rec_item->type = type;
    return rec_item;
}

/**
 * Free a recommendation item
 */
void FreeRecommendationItem(ai_recommendation_item_t *item)
{
    if (!item)
        return;
        
    if (item->item)
        input_item_Release(item->item);
        
    if (item->explanation)
        free(item->explanation);
        
    free(item);
}

/**
 * Free a recommendation set
 */
void FreeRecommendationSet(ai_recommendation_set_t *set)
{
    if (!set)
        return;
        
    /* Free each item */
    for (int i = 0; i < set->items_count; i++) {
        if (set->items[i])
            FreeRecommendationItem(set->items[i]);
    }
    
    free(set->items);
    free(set->reference_id);
    free(set);
}

/**
 * Generate recommendations based on a reference item
 */
ai_recommendation_set_t *GenerateRecommendations(ai_recommender_t *recommender, 
                                                input_item_t *reference, 
                                                int count)
{
    if (!recommender || !reference || count <= 0)
        return NULL;
        
    /* Limit count to maximum */
    if (count > MAX_RECOMMENDATIONS)
        count = MAX_RECOMMENDATIONS;
        
    /* Get reference item ID */
    char *ref_id = input_item_GetURI(reference);
    if (!ref_id)
        return NULL;
        
    /* Create recommendation set */
    ai_recommendation_set_t *set = CreateRecommendationSet(ref_id);
    if (!set) {
        free(ref_id);
        return NULL;
    }
        
    /* Lock to prevent concurrent updates */
    vlc_mutex_lock(&recommender->lock);
    
    /* Check cache first */
    for (int i = 0; i < recommender->recommendation_cache_count; i++) {
        if (recommender->cache_ids[i] && 
            strcmp(recommender->cache_ids[i], ref_id) == 0) {
            /* Found in cache - copy items */
            ai_recommendation_set_t *cached = recommender->recommendation_cache[i];
            
            /* Check if cache is fresh */
            time_t now = time(NULL);
            if (now - cached->generation_time > CACHE_EXPIRY_TIME) {
                /* Cache expired, generate new recommendations */
                break;
            }
            
            for (int j = 0; j < cached->items_count && j < count; j++) {
                ai_recommendation_item_t *item = cached->items[j];
                if (item && item->item) {
                    char *explanation = item->explanation ? strdup(item->explanation) : NULL;
                    ai_recommendation_item_t *new_item = CreateRecommendationItem(
                        item->item,
                        item->score,
                        explanation,
                        item->type
                    );
                    free(explanation);
                    
                    if (new_item) {
                        if (AddRecommendationItem(set, new_item) != VLC_SUCCESS)
                            FreeRecommendationItem(new_item);
                    }
                }
            }
            free(ref_id);
            vlc_mutex_unlock(&recommender->lock);
            return set;
        }
    }
    
    /* Generate new recommendations */
    ai_recommendation_context_t context = GetCurrentContext();
    
    /* Generate content-based recommendations */
    ai_recommendation_set_t *content_recs = GenerateContentBasedRecommendations(
        recommender, reference, recommender->current_profile, count);
        
    /* Generate collaborative recommendations */
    ai_recommendation_set_t *collab_recs = GenerateCollaborativeRecommendations(
        recommender, reference, recommender->current_profile, count);
        
    /* Generate contextual recommendations */
    ai_recommendation_set_t *context_recs = GenerateContextualRecommendations(
        recommender, reference, recommender->current_profile, &context, count);
        
    /* Merge recommendations */
    ai_recommendation_set_t *sets[3] = {content_recs, collab_recs, context_recs};
    float weights[3] = {0.5f, 0.3f, 0.2f};  /* Content-based given higher weight */
    
    ai_recommendation_set_t *merged = MergeRecommendations(sets, weights, 3, count);
    
    /* Free individual sets */
    for (int i = 0; i < 3; i++) {
        if (sets[i])
            FreeRecommendationSet(sets[i]);
    }
    
    if (merged) {
        /* Store new set in cache */
        UpdateRecommendationCache(recommender);
        
        /* Add to cache */
        ai_recommendation_set_t **new_cache = realloc(recommender->recommendation_cache,
            (recommender->recommendation_cache_count + 1) * sizeof(ai_recommendation_set_t*));
        char **new_ids = realloc(recommender->cache_ids,
            (recommender->recommendation_cache_count + 1) * sizeof(char*));
            
        if (new_cache && new_ids) {
            recommender->recommendation_cache = new_cache;
            recommender->cache_ids = new_ids;
            recommender->recommendation_cache[recommender->recommendation_cache_count] = merged;
            recommender->cache_ids[recommender->recommendation_cache_count] = strdup(ref_id);
            
            if (recommender->cache_ids[recommender->recommendation_cache_count])
                recommender->recommendation_cache_count++;
        }
        
        /* Copy the merged recommendations to return */
        for (int i = 0; i < merged->items_count && i < count; i++) {
            ai_recommendation_item_t *item = merged->items[i];
            if (item && item->item) {
                char *explanation = item->explanation ? strdup(item->explanation) : NULL;
                ai_recommendation_item_t *new_item = CreateRecommendationItem(
                    item->item,
                    item->score,
                    explanation,
                    item->type
                );
                free(explanation);
                
                if (new_item) {
                    if (AddRecommendationItem(set, new_item) != VLC_SUCCESS)
                        FreeRecommendationItem(new_item);
                }
            }
        }
    }
    
    free(ref_id);
    vlc_mutex_unlock(&recommender->lock);
    return set;
}

/**
 * Get the current context for recommendations
 */
ai_recommendation_context_t GetCurrentContext(void)
{
    ai_recommendation_context_t context;
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    
    context.hour_of_day = tm_info->tm_hour;
    context.day_of_week = tm_info->tm_wday;
    context.weekend = (tm_info->tm_wday == 0 || tm_info->tm_wday == 6);
    
    /* Estimate available time based on time of day */
    if (context.hour_of_day >= 20 || context.hour_of_day < 6) {
        context.available_time = 120;  /* 2 hours in the evening/night */
    } else if (context.weekend) {
        context.available_time = 180;  /* 3 hours on weekend days */
    } else {
        context.available_time = 60;   /* 1 hour on weekdays */
    }
    
    strcpy(context.device_type, "desktop");  /* Default device type */
    
    return context;
}

/**
 * Update recommendation cache
 */
void UpdateRecommendationCache(ai_recommender_t *recommender)
{
    if (!recommender)
        return;
        
    vlc_mutex_lock(&recommender->lock);
    
    /* Limit cache size */
    const int max_cache_items = 10;
    
    if (recommender->recommendation_cache_count > max_cache_items) {
        /* Find oldest cache entry */
        time_t oldest_time = time(NULL);
        int oldest_index = -1;
        
        for (int i = 0; i < recommender->recommendation_cache_count; i++) {
            ai_recommendation_set_t *set = recommender->recommendation_cache[i];
            if (set && set->generation_time < oldest_time) {
                oldest_time = set->generation_time;
                oldest_index = i;
            }
        }
        
        /* Remove oldest entry if found */
        if (oldest_index >= 0) {
            FreeRecommendationSet(recommender->recommendation_cache[oldest_index]);
            free(recommender->cache_ids[oldest_index]);
            
            /* Move last item to this position */
            int last = recommender->recommendation_cache_count - 1;
            recommender->recommendation_cache[oldest_index] = recommender->recommendation_cache[last];
            recommender->cache_ids[oldest_index] = recommender->cache_ids[last];
            
            /* Decrease count */
            recommender->recommendation_cache_count--;
        }
    }
    
    vlc_mutex_unlock(&recommender->lock);
}

/**
 * Clear recommendation cache
 */
void ClearRecommendationCache(ai_recommender_t *recommender)
{
    if (!recommender)
        return;
        
    vlc_mutex_lock(&recommender->lock);
    
    /* Free all cache entries */
    for (int i = 0; i < recommender->recommendation_cache_count; i++) {
        if (recommender->recommendation_cache[i])
            FreeRecommendationSet(recommender->recommendation_cache[i]);
            
        if (recommender->cache_ids[i])
            free(recommender->cache_ids[i]);
    }
    
    free(recommender->recommendation_cache);
    recommender->recommendation_cache = NULL;
    
    free(recommender->cache_ids);
    recommender->cache_ids = NULL;
    
    recommender->recommendation_cache_count = 0;
    
    vlc_mutex_unlock(&recommender->lock);
}

/**
 * Generate content-based recommendations
 */
ai_recommendation_set_t *GenerateContentBasedRecommendations(ai_recommender_t *recommender,
                                                           input_item_t *reference,
                                                           ai_user_profile_t *profile,
                                                           int count)
{
    if (!recommender || !reference || !profile || count <= 0)
        return NULL;
    
    char *ref_mrl = input_item_GetURI(reference);
    if (!ref_mrl)
        return NULL;
    
    ai_recommendation_set_t *set = CreateRecommendationSet(ref_mrl);
    free(ref_mrl);
    
    if (!set)
        return NULL;
    
    /* Extract metadata from the reference item */
    ai_media_metadata_t *ref_metadata = ExtractMediaMetadata(reference);
    if (!ref_metadata)
        return set;  /* Return empty set */
    
    /* For demo purposes, we create some sample recommendations */
    const char *title = ref_metadata->title ? ref_metadata->title : "your selection";
    const char *genre = ref_metadata->genre ? ref_metadata->genre : "unknown genre";
    const char *artist = ref_metadata->artist ? ref_metadata->artist : NULL;
    
    /* Create a few recommendations based on the reference metadata */
    for (int i = 0; i < 3 && i < count; i++) {
        char uri[256];
        char name[256];
        char explanation[256];
        
        /* Create different types of content-based recommendations */
        if (i == 0) {
            /* Similar genre */
            snprintf(uri, sizeof(uri), "file:///similar_genre_%d.mp4", i+1);
            snprintf(name, sizeof(name), "Similar %s content", genre);
            snprintf(explanation, sizeof(explanation), 
                    "Content similar to \"%s\" with matching genre", title);
        } else if (i == 1 && artist) {
            /* Same artist/creator */
            snprintf(uri, sizeof(uri), "file:///same_artist_%d.mp4", i+1);
            snprintf(name, sizeof(name), "More from %s", artist);
            snprintf(explanation, sizeof(explanation), 
                    "Another popular title from %s", artist);
        } else {
            /* Generally similar content */
            snprintf(uri, sizeof(uri), "file:///similar_content_%d.mp4", i+1);
            snprintf(name, sizeof(name), "Similar to %s", title);
            snprintf(explanation, sizeof(explanation), 
                    "Content with similar themes and style");
        }
        
        /* Create the recommendation item */
        input_item_t *rec_item = input_item_New(uri, name);
        if (rec_item) {
            float score = 0.9f - (i * 0.1f);  /* Decreasing scores */
            
            /* Create recommendation item */
            ai_recommendation_item_t *recommendation = CreateRecommendationItem(
                rec_item, 
                score, 
                explanation, 
                RECOMMENDATION_TYPE_SIMILAR_CONTENT
            );
            
            if (recommendation) {
                AddRecommendationItem(set, recommendation);
            }
            
            input_item_Release(rec_item);
        }
    }
    
    FreeMediaMetadata(ref_metadata);
    return set;
}

/**
 * Generate collaborative-filtered recommendations
 */
ai_recommendation_set_t *GenerateCollaborativeRecommendations(ai_recommender_t *recommender,
                                                            input_item_t *reference,
                                                            ai_user_profile_t *profile,
                                                            int count)
{
    if (!recommender || !reference || !profile || count <= 0)
        return NULL;
    
    char *ref_mrl = input_item_GetURI(reference);
    if (!ref_mrl)
        return NULL;
    
    ai_recommendation_set_t *set = CreateRecommendationSet(ref_mrl);
    free(ref_mrl);
    
    if (!set)
        return NULL;
    
    /* Extract basic information about reference */
    ai_media_metadata_t *ref_metadata = ExtractMediaMetadata(reference);
    if (!ref_metadata)
        return set;  /* Return empty set */
    
    /* For demo, we create some collaborative-style recommendations */
    const char *title = ref_metadata->title ? ref_metadata->title : "current selection";
    
    /* Create continuation recommendation */
    char uri1[256], name1[256], explanation1[256];
    snprintf(uri1, sizeof(uri1), "file:///continuation.mp4");
    snprintf(name1, sizeof(name1), "Continue watching: %s series", title);
    snprintf(explanation1, sizeof(explanation1), 
             "Next in series after \"%s\"", title);
    
    input_item_t *cont_item = input_item_New(uri1, name1);
    if (cont_item) {
        ai_recommendation_item_t *recommendation = CreateRecommendationItem(
            cont_item,
            0.95f,
            explanation1,
            RECOMMENDATION_TYPE_CONTINUATION
        );
        
        if (recommendation) {
            AddRecommendationItem(set, recommendation);
        }
        
        input_item_Release(cont_item);
    }
    
    /* Create "popular with others" recommendation */
    if (count > 1) {
        char uri2[256], name2[256], explanation2[256];
        snprintf(uri2, sizeof(uri2), "file:///popular_related.mp4");
        snprintf(name2, sizeof(name2), "Popular with fans of %s", title);
        snprintf(explanation2, sizeof(explanation2), 
                "People who watched \"%s\" also enjoyed this", title);
        
        input_item_t *pop_item = input_item_New(uri2, name2);
        if (pop_item) {
            ai_recommendation_item_t *recommendation = CreateRecommendationItem(
                pop_item,
                0.85f,
                explanation2,
                RECOMMENDATION_TYPE_DISCOVERY
            );
            
            if (recommendation) {
                AddRecommendationItem(set, recommendation);
            }
            
            input_item_Release(pop_item);
        }
    }
    
    FreeMediaMetadata(ref_metadata);
    return set;
}

/**
 * Generate context-aware recommendations
 */
ai_recommendation_set_t *GenerateContextualRecommendations(ai_recommender_t *recommender,
                                                          input_item_t *reference,
                                                          ai_user_profile_t *profile,
                                                          ai_recommendation_context_t *context,
                                                          int count)
{
    if (!recommender || !reference || !profile || !context || count <= 0)
        return NULL;
    
    char *ref_mrl = input_item_GetURI(reference);
    if (!ref_mrl)
        return NULL;
    
    ai_recommendation_set_t *set = CreateRecommendationSet(ref_mrl);
    free(ref_mrl);
    
    if (!set)
        return NULL;
    
    /* Create time-based recommendation */
    int hour = context->hour_of_day;
    const char *time_context;
    
    if (hour >= 5 && hour < 12) {
        time_context = "morning";
    } else if (hour >= 12 && hour < 17) {
        time_context = "afternoon";
    } else if (hour >= 17 && hour < 22) {
        time_context = "evening";
    } else {
        time_context = "late night";
    }
    
    /* Create mood-based recommendation for current time context */
    char uri[256], name[256], explanation[256];
    snprintf(uri, sizeof(uri), "file:///mood_%s.mp4", time_context);
    snprintf(name, sizeof(name), "%s Recommendation", time_context);
    
    if (context->weekend) {
        snprintf(explanation, sizeof(explanation), 
                "Perfect for your %s weekend viewing (%d minutes available)", 
                time_context, context->available_time);
    } else {
        snprintf(explanation, sizeof(explanation), 
                "Ideal for your %s weekday schedule (%d minutes available)", 
                time_context, context->available_time);
    }
    
    input_item_t *mood_item = input_item_New(uri, name);
    if (mood_item) {
        ai_recommendation_item_t *recommendation = CreateRecommendationItem(
            mood_item,
            0.9f,
            explanation,
            RECOMMENDATION_TYPE_MOOD_BASED
        );
        
        if (recommendation) {
            AddRecommendationItem(set, recommendation);
        }
        
        input_item_Release(mood_item);
    }
    
    return set;
}

/**
 * Merge multiple recommendation sets
 */
ai_recommendation_set_t *MergeRecommendations(ai_recommendation_set_t **sets,
                                             float *weights,
                                             int count,
                                             int max_items)
{
    if (!sets || count <= 0 || max_items <= 0)
        return NULL;
    
    /* Create a new set for merged recommendations */
    ai_recommendation_set_t *merged = CreateRecommendationSet("merged_recommendations");
    if (!merged)
        return NULL;
    
    /* Create a temporary array for sorting */
    recommendation_sort_item_t *sort_items = NULL;
    int total_items = 0;
    
    /* Count total number of items across all sets */
    for (int i = 0; i < count; i++) {
        if (sets[i])
            total_items += sets[i]->items_count;
    }
    
    if (total_items == 0)
        return merged;  /* Empty result */
    
    /* Allocate sort array */
    sort_items = calloc(total_items, sizeof(recommendation_sort_item_t));
    if (!sort_items) {
        FreeRecommendationSet(merged);
        return NULL;
    }
    
    /* Fill sort array with weighted scores */
    int idx = 0;
    for (int i = 0; i < count; i++) {
        if (!sets[i])
            continue;
            
        float weight = (weights && i < count) ? weights[i] : 1.0f / count;
        
        for (int j = 0; j < sets[i]->items_count; j++) {
            if (idx >= total_items)
                break;
                
            sort_items[idx].item = sets[i]->items[j];
            sort_items[idx].score = sets[i]->items[j]->score * weight;
            idx++;
        }
    }
    
    /* Sort recommendations by score */
    qsort(sort_items, idx, sizeof(recommendation_sort_item_t), compare_recommendations);
    
    /* Add top items to result set, avoiding duplicates */
    for (int i = 0; i < idx && merged->items_count < max_items; i++) {
        ai_recommendation_item_t *item = sort_items[i].item;
        
        /* Avoid duplicates by checking URIs */
        bool is_duplicate = false;
        char *uri = input_item_GetURI(item->item);
        
        if (uri) {
            for (int j = 0; j < merged->items_count; j++) {
                char *existing_uri = input_item_GetURI(merged->items[j]->item);
                if (existing_uri) {
                    if (strcmp(uri, existing_uri) == 0)
                        is_duplicate = true;
                    free(existing_uri);
                }
                
                if (is_duplicate)
                    break;
            }
            free(uri);
        }
        
        if (!is_duplicate) {
            /* Create a copy of the item with adjusted score */
            ai_recommendation_item_t *new_item = CreateRecommendationItem(
                item->item,
                sort_items[i].score,
                item->explanation,
                item->type
            );
            
            if (new_item) {
                if (AddRecommendationItem(merged, new_item) != VLC_SUCCESS)
                    FreeRecommendationItem(new_item);
            }
        }
    }
    
    free(sort_items);
    return merged;
}

/**
 * Generate explanation for a recommendation
 */
char *GenerateRecommendationExplanation(input_item_t *reference,
                                       input_item_t *recommendation,
                                       float similarity,
                                       ai_recommendation_type_t type)
{
    if (!reference || !recommendation)
        return NULL;
    
    char *ref_title = input_item_GetTitle(reference);
    char *rec_title = input_item_GetTitle(recommendation);
    
    if (!ref_title)
        ref_title = input_item_GetName(reference);
        
    if (!rec_title)
        rec_title = input_item_GetName(recommendation);
    
    if (!ref_title)
        ref_title = strdup("your selection");
        
    if (!rec_title)
        rec_title = strdup("this content");
    
    /* Format varies based on recommendation type */
    char *explanation = NULL;
    struct vlc_memstream ms;
    
    if (vlc_memstream_open(&ms) == 0) {
        switch (type) {
            case RECOMMENDATION_TYPE_SIMILAR_CONTENT:
                if (similarity > 0.8f) {
                    vlc_memstream_printf(&ms, "Very similar to \"%s\" in style and content", ref_title);
                } else if (similarity > 0.5f) {
                    vlc_memstream_printf(&ms, "Similar to \"%s\" with some shared elements", ref_title);
                } else {
                    vlc_memstream_printf(&ms, "Shares some themes with \"%s\"", ref_title);
                }
                break;
                
            case RECOMMENDATION_TYPE_CONTINUATION:
                vlc_memstream_printf(&ms, "Continue from \"%s\" with this related content", ref_title);
                break;
                
            case RECOMMENDATION_TYPE_MOOD_BASED:
                vlc_memstream_printf(&ms, "Matches the mood and themes of \"%s\"", ref_title);
                break;
                
            case RECOMMENDATION_TYPE_DISCOVERY:
                vlc_memstream_printf(&ms, "Discover something new based on your interest in \"%s\"", ref_title);
                break;
                
            default:
                vlc_memstream_printf(&ms, "Recommended because you watched \"%s\"", ref_title);
                break;
        }
        
        /* Add similarity info for very similar items */
        if (similarity > 0.7f && type == RECOMMENDATION_TYPE_SIMILAR_CONTENT) {
            vlc_memstream_printf(&ms, " (%.0f%% match)", similarity * 100.0f);
        }
        
        if (vlc_memstream_close(&ms) == 0)
            explanation = ms.ptr;
    }
    
    free(ref_title);
    free(rec_title);
    
    return explanation;
}

/**
 * Diversify a recommendation set
 */
void DiversifyRecommendations(ai_recommendation_set_t *set, float diversity_factor)
{
    /* Placeholder for diversity implementation
     * A real implementation would:
     * 1. Calculate similarities between all items
     * 2. Reduce scores of very similar items
     * 3. Possibly re-sort the recommendation list
     */
}