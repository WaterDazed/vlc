/*****************************************************************************
 * recommendation_engine.h: AI-Powered Media Recommendation engine core
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

#ifndef VLC_AI_RECOMMENDATION_ENGINE_H
#define VLC_AI_RECOMMENDATION_ENGINE_H

#include <vlc_common.h>
#include <vlc_playlist.h>
#include <vlc_input_item.h>
#include <vlc_media_library.h>

#include "ai_recommendations.h"

/**
 * @file
 * This file implements the core recommendation engine
 */

/* Recommendation strategy types */
typedef enum {
    RECOMMENDATION_STRATEGY_CONTENT_BASED = 0,
    RECOMMENDATION_STRATEGY_COLLABORATIVE = 1,
    RECOMMENDATION_STRATEGY_CONTEXTUAL = 2,
    RECOMMENDATION_STRATEGY_HYBRID = 3,
    RECOMMENDATION_STRATEGY_COUNT
} ai_recommendation_strategy_t;

/* Recommendation weights */
typedef struct {
    float content_based_weight;
    float collaborative_weight;
    float contextual_weight;
    float genre_weight;
    float creator_weight;
    float year_weight;
    float popularity_weight;
    float recency_weight;
} ai_recommendation_weights_t;

/* Context for recommendations */
typedef struct {
    int hour_of_day;
    int day_of_week;
    bool weekend;
    int available_time;  /* In minutes */
    char device_type[32];
} ai_recommendation_context_t;

/**
 * Generate recommendations based on a reference item
 *
 * @param recommender The recommender module instance
 * @param reference The reference media item
 * @param count Number of recommendations to generate
 * @return Set of recommendations or NULL on error
 */
ai_recommendation_set_t *GenerateRecommendations(ai_recommender_t *recommender, 
                                                input_item_t *reference, 
                                                int count);

/**
 * Generate content-based recommendations
 *
 * @param recommender The recommender module instance
 * @param reference The reference media item
 * @param profile User profile to personalize recommendations
 * @param count Number of recommendations to generate
 * @return Set of recommendations or NULL on error
 */
ai_recommendation_set_t *GenerateContentBasedRecommendations(ai_recommender_t *recommender,
                                                           input_item_t *reference,
                                                           ai_user_profile_t *profile,
                                                           int count);

/**
 * Generate collaborative-filtered recommendations
 *
 * @param recommender The recommender module instance
 * @param reference The reference media item
 * @param profile User profile to personalize recommendations
 * @param count Number of recommendations to generate
 * @return Set of recommendations or NULL on error
 */
ai_recommendation_set_t *GenerateCollaborativeRecommendations(ai_recommender_t *recommender,
                                                            input_item_t *reference,
                                                            ai_user_profile_t *profile,
                                                            int count);

/**
 * Generate context-aware recommendations
 *
 * @param recommender The recommender module instance
 * @param reference The reference media item
 * @param profile User profile to personalize recommendations
 * @param context Current context information
 * @param count Number of recommendations to generate
 * @return Set of recommendations or NULL on error
 */
ai_recommendation_set_t *GenerateContextualRecommendations(ai_recommender_t *recommender,
                                                          input_item_t *reference,
                                                          ai_user_profile_t *profile,
                                                          ai_recommendation_context_t *context,
                                                          int count);

/**
 * Merge multiple recommendation sets with weighting
 *
 * @param sets Array of recommendation sets to merge
 * @param weights Weights for each set (can be NULL for equal weights)
 * @param count Number of sets in the array
 * @param max_items Maximum number of items in the result
 * @return Merged recommendation set or NULL on error
 */
ai_recommendation_set_t *MergeRecommendations(ai_recommendation_set_t **sets,
                                             float *weights,
                                             int count,
                                             int max_items);

/**
 * Create a new recommendation set
 *
 * @param reference_id Unique identifier for the recommendation set
 * @return New recommendation set or NULL on error
 */
ai_recommendation_set_t *CreateRecommendationSet(const char *reference_id);

/**
 * Add a recommendation item to a set
 *
 * @param set The recommendation set to add to
 * @param item The item to add
 * @return VLC_SUCCESS or VLC_EGENERIC
 */
int AddRecommendationItem(ai_recommendation_set_t *set, ai_recommendation_item_t *item);

/**
 * Create a new recommendation item
 *
 * @param item The media item
 * @param score The recommendation score
 * @param explanation Optional explanation text
 * @param type The recommendation type
 * @return New recommendation item or NULL on error
 */
ai_recommendation_item_t *CreateRecommendationItem(input_item_t *item, float score,
                                                 const char *explanation,
                                                 ai_recommendation_type_t type);

/**
 * Free a recommendation item
 *
 * @param item The item to free
 */
void FreeRecommendationItem(ai_recommendation_item_t *item);

/**
 * Free a recommendation set
 *
 * @param set The set to free
 */
void FreeRecommendationSet(ai_recommendation_set_t *set);

/**
 * Get current context for recommendations
 *
 * @return Current context information
 */
ai_recommendation_context_t GetCurrentContext(void);

/**
 * Update recommendation cache with recently computed recommendations
 *
 * @param recommender The recommender module instance
 */
void UpdateRecommendationCache(ai_recommender_t *recommender);

/**
 * Clear recommendation cache
 *
 * @param recommender The recommender module instance
 */
void ClearRecommendationCache(ai_recommender_t *recommender);

/**
 * Generate explanation for a recommendation
 *
 * @param reference Reference media item
 * @param recommendation Recommended media item
 * @param similarity Similarity score
 * @param type Recommendation type
 * @return Explanation text or NULL on error, caller must free with free()
 */
char *GenerateRecommendationExplanation(input_item_t *reference,
                                       input_item_t *recommendation,
                                       float similarity,
                                       ai_recommendation_type_t type);

/**
 * Diversify a recommendation set to avoid too-similar items
 *
 * @param set The recommendation set to diversify
 * @param diversity_factor How much to prioritize diversity (0.0 to 1.0)
 */
void DiversifyRecommendations(ai_recommendation_set_t *set, float diversity_factor);

#endif /* VLC_AI_RECOMMENDATION_ENGINE_H */